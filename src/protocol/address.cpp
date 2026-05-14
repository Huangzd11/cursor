#include "protocol/address.h"

namespace dlt645 {

std::optional<Address> Address::from_string(const std::string& hex_str) {
    // TODO: 解析12位hex字符串为6字节BCD（低位在前）
    if (hex_str.size() != 12) {
        return std::nullopt;
    }
    return Address{};
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
    // TODO: 将6字节BCD转为12位hex字符串
    return "";
}

bool Address::is_broadcast() const {
    // TODO: 判断是否全为0x99
    return false;
}

bool Address::operator==(const Address& other) const {
    return bytes_ == other.bytes_;
}

bool Address::operator!=(const Address& other) const {
    return !(*this == other);
}

}  // namespace dlt645
