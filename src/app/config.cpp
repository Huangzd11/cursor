#include "app/config.h"

#include <yaml-cpp/yaml.h>

#include <spdlog/spdlog.h>

namespace dlt645 {
namespace {

// 从 YAML map 解析串口配置，未出现的字段沿用 base
SerialPort::Config parse_serial_node(const YAML::Node& node, const SerialPort::Config& base) {
    SerialPort::Config c = base;
    if (!node || !node.IsMap()) {
        return c;
    }
    if (node["device"]) {
        c.device = node["device"].as<std::string>(c.device);
    }
    if (node["baudrate"]) {
        c.baudrate = node["baudrate"].as<int>(c.baudrate);
    }
    if (node["databits"]) {
        c.databits = node["databits"].as<int>(c.databits);
    }
    if (node["stopbits"]) {
        c.stopbits = node["stopbits"].as<int>(c.stopbits);
    }
    if (node["parity"]) {
        const std::string p = node["parity"].as<std::string>("E");
        if (!p.empty()) {
            c.parity = p[0];
        }
    }
    return c;
}

}  // namespace

std::optional<AppConfig> Config::load(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        AppConfig config;

        // 根级 serial 作为各表 serial 的默认值（表级可覆盖）
        config.default_serial = parse_serial_node(root["serial"], SerialPort::Config{});

        if (auto meters = root["meters"]) {
            for (const auto& m : meters) {
                MeterConfig mc;
                auto addr_str = m["address"].as<std::string>("");
                auto addr = Address::from_string(addr_str);
                if (!addr) {
                    SPDLOG_ERROR("无效的电表地址: {}", addr_str);
                    return std::nullopt;
                }
                mc.address = *addr;
                mc.name = m["name"].as<std::string>("");
                mc.serial = parse_serial_node(m["serial"], config.default_serial);
                mc.enabled = m["enabled"].as<bool>(true);
                config.meters.push_back(std::move(mc));
            }
        }

        if (auto items = root["data_items"]) {
            for (const auto& item : items) {
                DataItem di;
                auto di_str = item["di"].as<std::string>("");
                di.di = static_cast<uint32_t>(std::stoul(di_str, nullptr, 16));
                di.name = item["name"].as<std::string>("");
                di.data_length = item["length"].as<int>(0);
                di.decimal_digits = item["decimal"].as<int>(0);
                di.unit = item["unit"].as<std::string>("");
                di.enabled = item["enabled"].as<bool>(true);
                config.data_items.push_back(std::move(di));
            }
        }

        config.poll_interval_seconds = root["poll_interval_seconds"].as<int>(60);

        if (const auto mq = root["mqtt"]) {
            auto& m = config.mqtt;
            m.enabled = mq["enabled"].as<bool>(false);
            m.gateway_id = mq["gateway_id"].as<std::string>("");
            m.broker = mq["broker"].as<std::string>("");
            m.port = mq["port"].as<int>(1883);
            m.tls = mq["tls"].as<bool>(false);
            m.ca_file = mq["ca_file"].as<std::string>("");
            m.username = mq["username"].as<std::string>("");
            m.password = mq["password"].as<std::string>("");
            m.client_id = mq["client_id"].as<std::string>("");
            m.topic_prefix = mq["topic_prefix"].as<std::string>("dlt645");
            m.qos = mq["qos"].as<int>(1);
            m.keepalive_sec = mq["keepalive_sec"].as<int>(60);
            m.max_queue = static_cast<std::size_t>(mq["max_queue"].as<int>(512));
        }

        return config;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("加载配置文件失败: {}", e.what());
        return std::nullopt;
    }
}

std::vector<std::string> Config::validate(const AppConfig& config) {
    std::vector<std::string> errors;

    if (config.meters.empty()) {
        errors.push_back("电表列表不能为空");
    }

    int enabled_meters = 0;
    for (const auto& m : config.meters) {
        if (!m.enabled) {
            continue;
        }
        ++enabled_meters;
        if (m.serial.device.empty()) {
            errors.push_back("启用的电表 \"" + m.name + "\" 未配置有效串口 device（请在 meters[].serial 或根 serial 中指定）");
        }
    }
    if (enabled_meters == 0) {
        errors.push_back("至少需要一块 enabled 为 true 的电表");
    }

    int enabled_items = 0;
    for (const auto& di : config.data_items) {
        if (di.enabled) {
            ++enabled_items;
        }
    }
    if (config.data_items.empty()) {
        errors.push_back("数据项列表不能为空");
    } else if (enabled_items == 0) {
        errors.push_back("至少需要一项 enabled 为 true 的 data_items");
    }

    if (config.poll_interval_seconds <= 0) {
        errors.push_back("轮询间隔必须大于0");
    }

    if (config.mqtt.enabled) {
        if (config.mqtt.tls) {
            errors.push_back("mqtt.tls: 当前版本暂不支持 TLS，请将 mqtt.tls 设为 false");
        }
        if (config.mqtt.gateway_id.empty()) {
            errors.push_back("mqtt.enabled 为 true 时 mqtt.gateway_id 不能为空");
        }
        if (config.mqtt.broker.empty()) {
            errors.push_back("mqtt.enabled 为 true 时 mqtt.broker 不能为空");
        }
        if (config.mqtt.port <= 0 || config.mqtt.port > 65535) {
            errors.push_back("mqtt.port 无效（应为 1~65535）");
        }
        if (config.mqtt.qos < 0 || config.mqtt.qos > 2) {
            errors.push_back("mqtt.qos 必须在 0~2");
        }
        if (config.mqtt.max_queue == 0) {
            errors.push_back("mqtt.max_queue 必须大于 0");
        }
    }

    return errors;
}

}  // namespace dlt645
