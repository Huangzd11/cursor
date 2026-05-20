#pragma once

#include <spdlog/spdlog.h>

namespace dlt645 {

// 初始化全局日志格式（含源文件与行号，需配合 SPDLOG_* 宏使用）
inline void init_logging() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%s:%#] [%^%l%$] %v");
}

}  // namespace dlt645
