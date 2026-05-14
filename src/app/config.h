#pragma once

#include <optional>
#include <string>
#include <vector>

#include "protocol/address.h"
#include "protocol/data_item.h"
#include "transport/serial_port.h"

namespace dlt645 {

// 单个电表的配置
struct MeterConfig {
    Address address;
    std::string name;
};

// 整体应用配置
struct AppConfig {
    SerialPort::Config serial;
    std::vector<MeterConfig> meters;
    std::vector<DataItem> data_items;
    int poll_interval_seconds = 60;
};

// 配置管理：YAML文件的加载和校验
class Config {
public:
    // 从YAML文件加载配置
    static std::optional<AppConfig> load(const std::string& filepath);

    // 校验配置合法性，返回错误信息列表（空表示通过）
    static std::vector<std::string> validate(const AppConfig& config);
};

}  // namespace dlt645
