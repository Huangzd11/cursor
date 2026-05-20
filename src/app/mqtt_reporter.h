#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

#include "app/config.h"
#include "app/meter_reader.h"

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

private:
    struct QueuedMessage {
        std::string topic;
        std::string payload;
    };

    void worker_loop();
    bool ensure_connected();
    void disconnect_client();
    std::string build_topic(const std::string& meter_name, const std::string& di_hex) const;
    static std::string build_payload(const MqttConfig& cfg,
                                       const std::string& meter_name,
                                       const std::string& meter_address_hex,
                                       const std::string& item_name,
                                       const std::string& di_hex,
                                       MeterReader::ErrorCode code,
                                       const std::optional<MeterReader::ReadResult>& result);

    MqttConfig cfg_;
    std::atomic<bool> running_{false};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<QueuedMessage> queue_;

    std::thread worker_;

    void* client_{nullptr};  // MQTTClient，避免在头文件中暴露 C 库类型
};

}  // namespace dlt645
