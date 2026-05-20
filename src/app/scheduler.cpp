#include "app/scheduler.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace dlt645 {

Scheduler::Scheduler(AppConfig config, std::vector<std::shared_ptr<MeterReader>> readers)
    : config_(std::move(config)), readers_(std::move(readers)) {}

void Scheduler::run() {
    running_ = true;
    SPDLOG_INFO("采集调度器已启动，轮询间隔 {} 秒", config_.poll_interval_seconds);

    while (running_) {
        poll_once();

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
    for (size_t i = 0; i < config_.meters.size(); ++i) {
        if (!running_) {
            return;
        }
        const auto& meter = config_.meters[i];
        if (!meter.enabled) {
            continue;
        }
        if (i >= readers_.size()) {
            continue;
        }
        const auto& reader = readers_[i];
        if (!reader) {
            continue;
        }

        for (const auto& item : config_.data_items) {
            if (!running_) {
                return;
            }
            if (!item.enabled) {
                continue;
            }

            auto [code, result] = reader->read_data(meter.name, meter.address, item);

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
