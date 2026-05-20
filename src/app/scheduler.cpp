#include "app/scheduler.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace dlt645 {

Scheduler::Scheduler(std::shared_ptr<MeterReader> reader,
                     const AppConfig& config)
    : reader_(std::move(reader)), config_(config) {}

void Scheduler::run() {
    running_ = true;
    SPDLOG_INFO("采集调度器已启动，轮询间隔 {} 秒", config_.poll_interval_seconds);

    while (running_) {
        poll_once();

        // 等待轮询间隔，期间检查停止标志
        for (int i = 0; i < config_.poll_interval_seconds && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    SPDLOG_INFO("采集调度器已停止");
}

void Scheduler::stop() {
    running_ = false;
}

void Scheduler::poll_once() {
    for (const auto& meter : config_.meters) {
        for (const auto& item : config_.data_items) {
            auto [code, result] =
                reader_->read_data(meter.name, meter.address, item);

            if (callback_) {
                callback_(meter.name, item.name, code, result);
            }
        }
    }
}

void Scheduler::set_result_callback(ResultCallback callback) {
    callback_ = std::move(callback);
}

}  // namespace dlt645
