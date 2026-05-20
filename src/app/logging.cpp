#include "app/logging.h"

#include <cerrno>
#include <cstdio>
#include <memory>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

namespace dlt645 {

namespace {

constexpr const char* kLogDir = "log";
// 基名含扩展名，daily_file_sink 会生成 log/dlt645_YYYY-MM-DD.log
constexpr const char* kLogBasePath = "log/dlt645.log";

void ensure_log_dir() {
    if (::mkdir(kLogDir, 0755) != 0 && errno != EEXIST) {
        std::fprintf(stderr, "创建日志目录 %s 失败: errno=%d\n", kLogDir, errno);
    }
}

}  // namespace

void init_logging() {
    ensure_log_dir();

    std::vector<spdlog::sink_ptr> sinks;

    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%s:%#] [%^%l%$] %v");
    sinks.push_back(console);

    try {
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            kLogBasePath, 0, 0, false, 0);
        // 文件不含 ANSI 颜色转义，便于 grep / 文本查看
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%s:%#] [%l] %v");
        sinks.push_back(std::move(file_sink));
    } catch (const spdlog::spdlog_ex& ex) {
        std::fprintf(stderr, "创建文件日志失败（仅控制台输出）: %s\n", ex.what());
    }

    auto logger = std::make_shared<spdlog::logger>("dlt645", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(logger));
}

}  // namespace dlt645
