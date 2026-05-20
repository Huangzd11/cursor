#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/config.h"
#include "app/meter_reader.h"

namespace dlt645 {

// 轮询调度器：按配置间隔循环采集所有电表的所有数据项
class Scheduler {
public:
    using ResultCallback = std::function<void(
        const std::string& meter_name,
        const std::string& meter_address_hex,
        const std::string& item_name,
        const std::string& di_hex,
        MeterReader::ErrorCode code,
        std::optional<MeterReader::ReadResult> result)>;

    // readers[i] 对应 meters[i]；禁用的表对应 nullptr
    Scheduler(AppConfig config, std::vector<std::shared_ptr<MeterReader>> readers);

    // 启动轮询（阻塞，直到调用 stop()）
    void run();

    // 停止轮询（可从其他线程调用）
    void stop();

    // 执行一轮采集
    void poll_once();

    void set_result_callback(ResultCallback callback);

private:
    AppConfig config_;
    std::vector<std::shared_ptr<MeterReader>> readers_;
    std::atomic<bool> running_{false};
    ResultCallback callback_;
};

}  // namespace dlt645
