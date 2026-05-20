#include "protocol/address.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace dlt645 {

namespace {

// 判断是否为十六进制字符
bool is_hex_digit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

bool is_valid_bcd(uint8_t b) {
    // 每半个字节均为 0～9 方为合法 BCD
    return ((b >> 4) <= 9) && ((b & 0x0F) <= 9);
}

}  // namespace

std::optional<Address> Address::from_string(const std::string& hex_str) {
    // 12 位十六进制：从左到右为表号高位→低位；协议帧内 6 字节 BCD 为低位在前
    if (hex_str.size() != 12) {
        return std::nullopt;
    }
    for (char c : hex_str) {
        if (!is_hex_digit(c)) {
            return std::nullopt;
        }
    }

    Address addr;
    for (int i = 0; i < 6; ++i) {
        const int str_hi = 10 - 2 * i;
        const int v_hi = hex_value(hex_str[static_cast<size_t>(str_hi)]);
        const int v_lo = hex_value(hex_str[static_cast<size_t>(str_hi + 1)]);
        const uint8_t byte = static_cast<uint8_t>((v_hi << 4) | v_lo);
        if (!is_valid_bcd(byte)) {
            return std::nullopt;
        }
        addr.bytes_[static_cast<size_t>(i)] = byte;
    }
    return addr;
}

Address Address::from_bytes(const std::array<uint8_t, 6>& bytes) {
    Address addr;
    addr.bytes_ = bytes;
    return addr;
}

const std::array<uint8_t, 6>& Address::bytes() const {
    return bytes_;
}

std::string Address::to_string() const {
    // 与 from_string 对称：从传输序高位字节到低位字节依次输出两字符十六进制
    std::ostringstream oss;
    for (int i = 5; i >= 0; --i) {
        oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(bytes_[static_cast<size_t>(i)]);
    }
    return oss.str();
}

bool Address::is_broadcast() const {
    // 广播地址：6 字节均为 0x99
    for (uint8_t b : bytes_) {
        if (b != 0x99) {
            return false;
        }
    }
    return true;
}

bool Address::operator==(const Address& other) const {
    return bytes_ == other.bytes_;
}

bool Address::operator!=(const Address& other) const {
    return !(*this == other);
}

}  // namespace dlt645
