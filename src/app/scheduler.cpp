#include "app/scheduler.h"

#include <chrono>
#include <iomanip>
#include <sstream>
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

        std::vector<BatchItemResult> batch_items;
        for (const auto& item : config_.data_items) {
            if (!running_) {
                return;
            }
            if (!item.enabled) {
                continue;
            }

            std::ostringstream di_oss;
            di_oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                   << static_cast<unsigned>(item.di);
            const std::string di_hex = di_oss.str();

            auto [code, result] = reader->read_data(meter.name, meter.address, item);

            if (callback_) {
                callback_(meter.name, meter.address.to_string(), item.name, di_hex, code, result);
            }

            batch_items.push_back(BatchItemResult{
                item.name,
                di_hex,
                code,
                result,
            });
        }

        if (meter_batch_callback_ && !batch_items.empty()) {
            meter_batch_callback_(meter.name, meter.address.to_string(), batch_items);
        }
    }
}

void Scheduler::set_result_callback(ResultCallback callback) {
    callback_ = std::move(callback);
}

void Scheduler::set_meter_batch_callback(MeterBatchCallback callback) {
    meter_batch_callback_ = std::move(callback);
}

}  // namespace dlt645
