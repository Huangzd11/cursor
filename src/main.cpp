#include <csignal>
#include <iostream>
#include <memory>

#include <spdlog/spdlog.h>

#include "app/config.h"
#include "app/logging.h"
#include "app/meter_reader.h"
#include "app/result_reporter.h"
#include "app/scheduler.h"
#include "transport/frame_transceiver.h"
#include "transport/serial_port.h"

static dlt645::Scheduler* g_scheduler = nullptr;

static void signal_handler(int sig) {
    if (g_scheduler) {
        SPDLOG_INFO("收到信号 {}，正在停止...", sig);
        g_scheduler->stop();
    }
}

int main(int argc, char* argv[]) {
    dlt645::init_logging();

    std::string config_path = "config/collector.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }

    // 加载配置
    auto config = dlt645::Config::load(config_path);
    if (!config) {
        SPDLOG_ERROR("配置加载失败，退出");
        return 1;
    }

    auto errors = dlt645::Config::validate(*config);
    if (!errors.empty()) {
        for (const auto& err : errors) {
            SPDLOG_ERROR("配置校验: {}", err);
        }
        return 1;
    }

    // 构建组件链: SerialPort → FrameTransceiver → MeterReader → Scheduler
    auto serial = std::make_shared<dlt645::SerialPort>(config->serial);
    if (!serial->open()) {
        SPDLOG_ERROR("串口打开失败: {}", config->serial.device);
        return 1;
    }

    auto transceiver = std::make_shared<dlt645::FrameTransceiver>(serial);
    auto reader = std::make_shared<dlt645::MeterReader>(transceiver);

    dlt645::Scheduler scheduler(reader, *config);
    g_scheduler = &scheduler;

    // 注册结果回调
    dlt645::ResultReporter reporter;
    scheduler.set_result_callback(
        [&reporter](const std::string& meter, const std::string& item,
                    dlt645::MeterReader::ErrorCode code,
                    std::optional<dlt645::MeterReader::ReadResult> result) {
            reporter.report(meter, item, code, result);
        });

    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    SPDLOG_INFO("DL/T 645 数据采集程序启动");
    scheduler.run();

    return 0;
}
