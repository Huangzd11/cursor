#include "app/mqtt_reporter.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

#include <spdlog/spdlog.h>

extern "C" {
#include <MQTTClient.h>
}

namespace dlt645 {
namespace {

std::string make_cache_key(const std::string& meter_name, const std::string& di_hex) {
    return meter_name + "|" + di_hex;
}

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

struct TimestampPair {
    int64_t ts_ms;
    std::string iso_utc;
};

// 上送 JSON 使用：ts_ms（整型）+ timestamp（ISO8601 UTC，毫秒，Z 结尾）
TimestampPair now_timestamp_pair() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch());
    const int64_t ts_ms = ms.count();
    const time_t sec = static_cast<time_t>(ts_ms / 1000);
    const int milli = static_cast<int>(ts_ms % 1000);

    struct tm tm_buf {};
    gmtime_r(&sec, &tm_buf);
    char base[32]{};
    std::strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    char out[40]{};
    std::snprintf(out, sizeof(out), "%s.%03dZ", base, milli);
    return {ts_ms, out};
}

std::optional<std::string> json_get_string(const std::string& payload, const char* key) {
    // 极简 JSON 提取：仅支持 {"key":"value"} 形式，不做通用解析（够用且避免引入新依赖）
    const std::string pat = std::string("\"") + key + "\":\"";
    const auto pos = payload.find(pat);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    const auto start = pos + pat.size();
    const auto end = payload.find('\"', start);
    if (end == std::string::npos || end < start) {
        return std::nullopt;
    }
    return payload.substr(start, end - start);
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

std::string MqttReporter::build_batch_topic(const std::string& meter_name) const {
    std::ostringstream oss;
    oss << topic_segment(cfg_.topic_prefix) << '/' << topic_segment(cfg_.gateway_id) << "/meter/"
        << topic_segment(meter_name) << "/readings";
    return oss.str();
}

std::string MqttReporter::build_cmd_topic() const {
    std::ostringstream oss;
    oss << topic_segment(cfg_.topic_prefix) << '/' << topic_segment(cfg_.gateway_id) << "/cmd/query";
    return oss.str();
}

std::string MqttReporter::build_payload(const MqttConfig& cfg,
                                        const std::string& meter_name,
                                        const std::string& meter_address_hex,
                                        const std::string& item_name,
                                        const std::string& di_hex,
                                        MeterReader::ErrorCode code,
                                        const std::optional<MeterReader::ReadResult>& result) {
    const TimestampPair ts = now_timestamp_pair();
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
    oss << "\"ts_ms\":" << ts.ts_ms << ",\"timestamp\":\"" << json_escape(ts.iso_utc) << "\"}";
    return oss.str();
}

std::string MqttReporter::build_batch_payload(
    const MqttConfig& cfg,
    const std::string& meter_name,
    const std::string& meter_address_hex,
    const std::vector<Scheduler::BatchItemResult>& items) {

    const TimestampPair ts = now_timestamp_pair();
    std::ostringstream oss;
    oss << std::fixed;
    oss << "{\"schema_version\":1,\"gateway_id\":\"" << json_escape(cfg.gateway_id) << "\","
        << "\"meter\":\"" << json_escape(meter_name) << "\","
        << "\"meter_address\":\"" << json_escape(meter_address_hex) << "\","
        << "\"ts_ms\":" << ts.ts_ms << ",\"timestamp\":\"" << json_escape(ts.iso_utc) << "\",\"items\":[";

    bool first = true;
    for (const auto& it : items) {
        if (!first) {
            oss << ',';
        }
        first = false;

        oss << "{\"item\":\"" << json_escape(it.item_name) << "\","
            << "\"di\":\"" << json_escape(it.di_hex) << "\",";
        if (it.code == MeterReader::ErrorCode::kSuccess && it.result) {
            oss << "\"success\":true,\"value\":" << it.result->value << ",\"unit\":\""
                << json_escape(it.result->unit) << "\"}";
        } else {
            oss << "\"success\":false,\"error_code\":" << static_cast<int>(it.code)
                << ",\"error_name\":\"" << json_escape(error_code_name(it.code)) << "\"}";
        }
    }
    oss << "]}";
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
        last_item_payload_by_key_[make_cache_key(meter_name, di_hex)] = job.payload;
    }

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

void MqttReporter::report_batch(const std::string& meter_name,
                                const std::string& meter_address_hex,
                                const std::vector<Scheduler::BatchItemResult>& items) {
    if (!cfg_.enabled) {
        return;
    }
    if (items.empty()) {
        return;
    }

    QueuedMessage job;
    job.topic = build_batch_topic(meter_name);
    job.payload = build_batch_payload(cfg_, meter_name, meter_address_hex, items);

    {
        std::lock_guard<std::mutex> lk(mutex_);
        // 同时刷新缓存，便于 cmd/query 立即返回
        for (const auto& it : items) {
            const std::string key = make_cache_key(meter_name, it.di_hex);
            last_item_payload_by_key_[key] = build_payload(
                cfg_, meter_name, meter_address_hex, it.item_name, it.di_hex, it.code, it.result);
        }

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

    // 连接成功后订阅查询指令
    const std::string cmd_topic = build_cmd_topic();
    const int src = MQTTClient_subscribe(c, cmd_topic.c_str(), 0);
    if (src != MQTTCLIENT_SUCCESS) {
        SPDLOG_WARN("MQTT 订阅失败 rc={} topic={}", src, cmd_topic);
    } else {
        SPDLOG_INFO("MQTT 已订阅查询指令: {}", cmd_topic);
    }
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

bool MqttReporter::publish_now(const std::string& topic, const std::string& payload) {
    if (!ensure_connected()) {
        return false;
    }
    MQTTClient c = static_cast<MQTTClient>(client_);
    const int rc = MQTTClient_publish(
        c, topic.c_str(), static_cast<int>(payload.size()),
        reinterpret_cast<void*>(const_cast<char*>(payload.data())), cfg_.qos, 0, nullptr);
    if (rc != MQTTCLIENT_SUCCESS) {
        SPDLOG_WARN("MQTT 发布失败 rc={} topic={}", rc, topic);
        disconnect_client();
        return false;
    }
    SPDLOG_INFO("MQTT 上送 | topic={} | qos={} | bytes={} | payload={}",
                topic, cfg_.qos, payload.size(), payload);
    return true;
}

void MqttReporter::handle_query_command(const std::string& payload) {
    // 指令格式：{"meter":"meter-1","di":"00010000"}
    const auto meter = json_get_string(payload, "meter");
    const auto di = json_get_string(payload, "di");
    if (!meter || !di) {
        SPDLOG_WARN("MQTT 查询指令格式错误: {}", payload);
        return;
    }

    std::string cached;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        const auto it = last_item_payload_by_key_.find(make_cache_key(*meter, *di));
        if (it != last_item_payload_by_key_.end()) {
            cached = it->second;
        }
    }
    if (cached.empty()) {
        const std::string resp_topic =
            topic_segment(cfg_.topic_prefix) + "/" + topic_segment(cfg_.gateway_id) + "/cmd/query/resp";
        const TimestampPair ts = now_timestamp_pair();
        std::ostringstream oss;
        oss << "{\"schema_version\":1,\"gateway_id\":\"" << json_escape(cfg_.gateway_id) << "\","
            << "\"success\":false,\"message\":\"no_cache\",\"meter\":\"" << json_escape(*meter) << "\","
            << "\"di\":\"" << json_escape(*di) << "\",\"ts_ms\":" << ts.ts_ms
            << ",\"timestamp\":\"" << json_escape(ts.iso_utc) << "\"}";
        publish_now(resp_topic, oss.str());
        return;
    }

    // 按要求“查询哪个就上报哪个”：发布到该 item 的标准 topic
    publish_now(build_topic(*meter, *di), cached);
}

void MqttReporter::handle_incoming_commands(int timeout_ms) {
    MQTTClient c = static_cast<MQTTClient>(client_);
    if (!c || !MQTTClient_isConnected(c)) {
        return;
    }

    char* topic_name = nullptr;
    int topic_len = 0;
    MQTTClient_message* msg = nullptr;
    const int rc = MQTTClient_receive(c, &topic_name, &topic_len, &msg, timeout_ms);
    if (rc != MQTTCLIENT_SUCCESS || msg == nullptr) {
        return;
    }

    std::string topic;
    if (topic_name) {
        topic = topic_name;
    }

    std::string payload;
    payload.assign(static_cast<const char*>(msg->payload),
                   static_cast<size_t>(msg->payloadlen));

    if (topic == build_cmd_topic()) {
        SPDLOG_INFO("MQTT 收到查询指令 | topic={} | payload={}", topic, payload);
        handle_query_command(payload);
    }

    MQTTClient_freeMessage(&msg);
    MQTTClient_free(topic_name);
}

void MqttReporter::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait_for(lk, std::chrono::milliseconds(200), [&] { return !running_.load() || !queue_.empty(); });
        if (!running_ && queue_.empty()) {
            break;
        }
        // 出队一个（若有），否则释放锁去处理订阅消息
        std::optional<QueuedMessage> job;
        if (!queue_.empty()) {
            job = std::move(queue_.front());
            queue_.pop();
        }
        lk.unlock();

        if (!ensure_connected()) {
            continue;
        }

        // 先处理订阅指令，再发布待上送消息
        handle_incoming_commands(0);
        if (job) {
            publish_now(job->topic, job->payload);
        } else {
            // 无待发送消息时，阻塞等待一点时间处理指令
            handle_incoming_commands(200);
        }
    }
    disconnect_client();
}

}  // namespace dlt645
