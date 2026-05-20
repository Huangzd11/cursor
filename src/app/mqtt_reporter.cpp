#include "app/mqtt_reporter.h"

#include <chrono>
#include <cstdio>
#include <sstream>

#include <spdlog/spdlog.h>

extern "C" {
#include <MQTTClient.h>
}

namespace dlt645 {
namespace {

std::string topic_segment(const std::string& s) {
    std::string o;
    for (unsigned char ch : s) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.') {
            o += static_cast<char>(ch);
        } else {
            o += '_';
        }
    }
    if (o.empty()) {
        return "x";
    }
    return o;
}

const char* error_code_name(MeterReader::ErrorCode code) {
    switch (code) {
        case MeterReader::ErrorCode::kSuccess:
            return "成功";
        case MeterReader::ErrorCode::kSendFailed:
            return "发送失败";
        case MeterReader::ErrorCode::kTimeout:
            return "接收超时";
        case MeterReader::ErrorCode::kAddressMismatch:
            return "地址不匹配";
        case MeterReader::ErrorCode::kErrorResponse:
            return "电表异常应答";
        case MeterReader::ErrorCode::kDataMismatch:
            return "数据标识不匹配";
        case MeterReader::ErrorCode::kDecodeFailed:
            return "数据解码失败";
        default:
            return "未知错误";
    }
}

std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char ch : s) {
        switch (ch) {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\b':
                o += "\\b";
                break;
            case '\f':
                o += "\\f";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    o += buf;
                } else {
                    o += static_cast<char>(ch);
                }
        }
    }
    return o;
}

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

MqttReporter::MqttReporter(MqttConfig config)
    : cfg_(std::move(config)) {
    if (!cfg_.enabled) {
        return;
    }
    running_ = true;
    worker_ = std::thread([this] { worker_loop(); });
}

MqttReporter::~MqttReporter() {
    if (!cfg_.enabled) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    disconnect_client();
}

std::string MqttReporter::build_topic(const std::string& meter_name,
                                      const std::string& di_hex) const {
    std::ostringstream oss;
    oss << topic_segment(cfg_.topic_prefix) << '/' << topic_segment(cfg_.gateway_id) << "/meter/"
        << topic_segment(meter_name) << "/item/" << topic_segment(di_hex);
    return oss.str();
}

std::string MqttReporter::build_payload(const MqttConfig& cfg,
                                        const std::string& meter_name,
                                        const std::string& meter_address_hex,
                                        const std::string& item_name,
                                        const std::string& di_hex,
                                        MeterReader::ErrorCode code,
                                        const std::optional<MeterReader::ReadResult>& result) {
    std::ostringstream oss;
    oss << std::fixed;
    oss << "{\"schema_version\":1,\"gateway_id\":\"" << json_escape(cfg.gateway_id) << "\","
        << "\"meter\":\"" << json_escape(meter_name) << "\","
        << "\"meter_address\":\"" << json_escape(meter_address_hex) << "\","
        << "\"item\":\"" << json_escape(item_name) << "\","
        << "\"di\":\"" << json_escape(di_hex) << "\",";
    if (code == MeterReader::ErrorCode::kSuccess && result) {
        oss << "\"success\":true,\"value\":" << result->value << ",\"unit\":\""
            << json_escape(result->unit) << "\",";
    } else {
        oss << "\"success\":false,\"error_code\":" << static_cast<int>(code) << ",\"error_name\":\""
            << json_escape(error_code_name(code)) << "\",";
    }
    oss << "\"ts_ms\":" << now_ms() << '}';
    return oss.str();
}

void MqttReporter::report(const std::string& meter_name,
                          const std::string& meter_address_hex,
                          const std::string& item_name,
                          const std::string& di_hex,
                          MeterReader::ErrorCode code,
                          std::optional<MeterReader::ReadResult> result) {
    if (!cfg_.enabled) {
        return;
    }

    QueuedMessage job;
    job.topic = build_topic(meter_name, di_hex);
    job.payload = build_payload(cfg_, meter_name, meter_address_hex, item_name, di_hex, code, result);

    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (queue_.size() >= cfg_.max_queue) {
            static std::atomic<int> drop_log_counter{0};
            if (++drop_log_counter % 100 == 1) {
                SPDLOG_WARN("MQTT 队列已满 (max_queue={})，丢弃上报", cfg_.max_queue);
            }
            return;
        }
        queue_.push(std::move(job));
    }
    cv_.notify_one();
}

bool MqttReporter::ensure_connected() {
    MQTTClient c = static_cast<MQTTClient>(client_);
    if (c && MQTTClient_isConnected(c)) {
        return true;
    }
    disconnect_client();

    const std::string client_id =
        cfg_.client_id.empty() ? ("dlt645-" + topic_segment(cfg_.gateway_id)) : cfg_.client_id;

    const std::string uri =
        std::string("tcp://") + cfg_.broker + ':' + std::to_string(cfg_.port);

    MQTTClient created = nullptr;
    const int cr = MQTTClient_create(&created, uri.c_str(), client_id.c_str(),
                                     MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (cr != MQTTCLIENT_SUCCESS || !created) {
        SPDLOG_ERROR("MQTTClient_create 失败: {}", cr);
        return false;
    }
    client_ = created;
    c = created;

    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    opts.keepAliveInterval = cfg_.keepalive_sec;
    opts.cleansession = 1;
    opts.connectTimeout = 10;
    opts.username = cfg_.username.empty() ? nullptr : cfg_.username.c_str();
    opts.password = cfg_.password.empty() ? nullptr : cfg_.password.c_str();

    const int rc = MQTTClient_connect(c, &opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        SPDLOG_ERROR("MQTT 连接失败 broker={} rc={}", uri, rc);
        disconnect_client();
        return false;
    }
    SPDLOG_INFO("MQTT 已连接: {}", uri);
    return true;
}

void MqttReporter::disconnect_client() {
    if (!client_) {
        return;
    }
    MQTTClient c = static_cast<MQTTClient>(client_);
    if (MQTTClient_isConnected(c)) {
        MQTTClient_disconnect(c, 5000);
    }
    MQTTClient_destroy(&c);
    client_ = nullptr;
}

void MqttReporter::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [&] { return !running_.load() || !queue_.empty(); });
        if (!running_ && queue_.empty()) {
            break;
        }
        if (queue_.empty()) {
            continue;
        }
        QueuedMessage job = std::move(queue_.front());
        queue_.pop();
        lk.unlock();

        if (!ensure_connected()) {
            continue;
        }

        MQTTClient c = static_cast<MQTTClient>(client_);
        const int rc = MQTTClient_publish(
            c, job.topic.c_str(), static_cast<int>(job.payload.size()),
            reinterpret_cast<void*>(job.payload.data()), cfg_.qos, 0, nullptr);
        if (rc != MQTTCLIENT_SUCCESS) {
            SPDLOG_WARN("MQTT 发布失败 rc={} topic={}", rc, job.topic);
            disconnect_client();
        } else {
            // 与控制台/文件日志一致，便于对照 Broker 订阅内容
            SPDLOG_INFO("MQTT 上送 | topic={} | qos={} | bytes={} | payload={}",
                        job.topic, cfg_.qos, job.payload.size(), job.payload);
        }
    }
    disconnect_client();
}

}  // namespace dlt645
