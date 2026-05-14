#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace dlt645 {

// 电表通信地址（6字节BCD码，低位在前）
class Address {
public:
    Address() = default;

    // 从12位十六进制字符串构造（如 "000000000001"）
    static std::optional<Address> from_string(const std::string& hex_str);

    // 从6字节数组构造
    static Address from_bytes(const std::array<uint8_t, 6>& bytes);

    // 获取原始6字节（低位在前）
    const std::array<uint8_t, 6>& bytes() const;

    // 转为12位可读字符串
    std::string to_string() const;

    // 是否为广播地址（0x999999999999）
    bool is_broadcast() const;

    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;

private:
    std::array<uint8_t, 6> bytes_{};
};

}  // namespace dlt645
