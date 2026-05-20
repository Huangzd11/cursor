#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "app/config.h"
#include "app/meter_reader.h"
#include "app/scheduler.h"

namespace dlt645 {

// 将采集结果异步发布到 MQTT（独立工作线程 + 队列，不阻塞串口轮询）
class MqttReporter {
public:
    explicit MqttReporter(MqttConfig config);
    ~MqttReporter();

    MqttReporter(const MqttReporter&) = delete;
    MqttReporter& operator=(const MqttReporter&) = delete;

    void report(const std::string& meter_name,
                  const std::string& meter_address_hex,
                  const std::string& item_name,
                  const std::string& di_hex,
                  MeterReader::ErrorCode code,
                  std::optional<MeterReader::ReadResult> result);

    // 按表聚合上报（同一块表采集完所有启用项后，发布 1 条 readings 消息）
    void report_batch(const std::string& meter_name,
                      const std::string& meter_address_hex,
                      const std::vector<Scheduler::BatchItemResult>& items);

private:
    struct QueuedMessage {
        std::string topic;
        std::string payload;
    };

    void worker_loop();
    bool ensure_connected();
    void disconnect_client();
    std::string build_topic(const std::string& meter_name, const std::string& di_hex) const;
    std::string build_batch_topic(const std::string& meter_name) const;
    static std::string build_payload(const MqttConfig& cfg,
                                       const std::string& meter_name,
                                       const std::string& meter_address_hex,
                                       const std::string& item_name,
                                       const std::string& di_hex,
                                       MeterReader::ErrorCode code,
                                       const std::optional<MeterReader::ReadResult>& result);
    static std::string build_batch_payload(const MqttConfig& cfg,
                                          const std::string& meter_name,
                                          const std::string& meter_address_hex,
                                          const std::vector<Scheduler::BatchItemResult>& items);

    // 订阅查询指令：dlt645/{gateway_id}/cmd/query
    std::string build_cmd_topic() const;
    void handle_incoming_commands(int timeout_ms);
    void handle_query_command(const std::string& payload);
    bool publish_now(const std::string& topic, const std::string& payload);

    MqttConfig cfg_;
    std::atomic<bool> running_{false};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<QueuedMessage> queue_;

    std::thread worker_;

    void* client_{nullptr};  // MQTTClient，避免在头文件中暴露 C 库类型

    // 最近一次上报缓存：用于 cmd/query 立即返回（不触发串口重采集）
    std::unordered_map<std::string, std::string> last_item_payload_by_key_; // key=meter|di
 };

}  // namespace dlt645
