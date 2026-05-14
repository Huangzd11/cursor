#include "app/config.h"

#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace dlt645 {

std::optional<AppConfig> Config::load(const std::string& filepath) {
    // TODO: 解析YAML文件填充AppConfig
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        AppConfig config;

        // 串口配置
        if (auto serial = root["serial"]) {
            config.serial.device = serial["device"].as<std::string>("");
            config.serial.baudrate = serial["baudrate"].as<int>(2400);
        }

        // 电表列表
        if (auto meters = root["meters"]) {
            for (const auto& m : meters) {
                MeterConfig mc;
                auto addr_str = m["address"].as<std::string>("");
                auto addr = Address::from_string(addr_str);
                if (!addr) {
                    spdlog::error("无效的电表地址: {}", addr_str);
                    return std::nullopt;
                }
                mc.address = *addr;
                mc.name = m["name"].as<std::string>("");
                config.meters.push_back(std::move(mc));
            }
        }

        // 数据项列表
        if (auto items = root["data_items"]) {
            for (const auto& item : items) {
                DataItem di;
                auto di_str = item["di"].as<std::string>("");
                di.di = static_cast<uint32_t>(std::stoul(di_str, nullptr, 16));
                di.name = item["name"].as<std::string>("");
                di.data_length = item["length"].as<int>(0);
                di.decimal_digits = item["decimal"].as<int>(0);
                di.unit = item["unit"].as<std::string>("");
                config.data_items.push_back(std::move(di));
            }
        }

        config.poll_interval_seconds = root["poll_interval_seconds"].as<int>(60);

        return config;
    } catch (const std::exception& e) {
        spdlog::error("加载配置文件失败: {}", e.what());
        return std::nullopt;
    }
}

std::vector<std::string> Config::validate(const AppConfig& config) {
    std::vector<std::string> errors;

    if (config.serial.device.empty()) {
        errors.push_back("串口设备路径不能为空");
    }
    if (config.meters.empty()) {
        errors.push_back("电表列表不能为空");
    }
    if (config.data_items.empty()) {
        errors.push_back("数据项列表不能为空");
    }
    if (config.poll_interval_seconds <= 0) {
        errors.push_back("轮询间隔必须大于0");
    }

    return errors;
}

}  // namespace dlt645
