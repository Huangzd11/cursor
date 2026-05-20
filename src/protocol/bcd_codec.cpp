#include "protocol/bcd_codec.h"

#include <cmath>
#include <limits>

namespace dlt645 {
namespace BcdCodec {

namespace {

bool is_valid_bcd_byte(uint8_t b) {
    return ((b >> 4) <= 9) && ((b & 0x0F) <= 9);
}

uint8_t bcd_byte_to_value(uint8_t b) {
    return static_cast<uint8_t>(((b >> 4) * 10) + (b & 0x0F));
}

}  // namespace

uint64_t decode(const uint8_t* data, size_t len) {
    // 缓冲区高位在前：data[0] 为最高两位十进制
    uint64_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!is_valid_bcd_byte(data[i])) {
            return 0;
        }
        if (value > std::numeric_limits<uint64_t>::max() / 100) {
            return 0;
        }
        value = value * 100 + bcd_byte_to_value(data[i]);
    }
    return value;
}

double decode_float(const uint8_t* data, size_t len, int decimal_digits) {
    // DL/T 645 数值域：低位字节在前，每字节两位 BCD
    uint64_t value = 0;
    for (size_t k = 0; k < len; ++k) {
        const size_t i = len - 1 - k;
        if (!is_valid_bcd_byte(data[i])) {
            return 0.0;
        }
        if (value > std::numeric_limits<uint64_t>::max() / 100) {
            return 0.0;
        }
        value = value * 100 + bcd_byte_to_value(data[i]);
    }
    const double scale = std::pow(10.0, decimal_digits);
    if (scale <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(value) / scale;
}

std::vector<uint8_t> encode(uint64_t value, size_t len) {
    std::vector<uint8_t> out(len, 0);
    uint64_t v = value;
    for (size_t k = 0; k < len; ++k) {
        const size_t i = len - 1 - k;
        const int digit2 = static_cast<int>(v % 10);
        v /= 10;
        const int digit1 = static_cast<int>(v % 10);
        v /= 10;
        out[i] = static_cast<uint8_t>((digit1 << 4) | digit2);
    }
    return out;
}

std::vector<uint8_t> add33(const std::vector<uint8_t>& data) {
    // 发送前数据域逐字节加 0x33（模 256）
    std::vector<uint8_t> out;
    out.reserve(data.size());
    for (uint8_t b : data) {
        out.push_back(static_cast<uint8_t>(b + 0x33));
    }
    return out;
}

std::vector<uint8_t> sub33(const std::vector<uint8_t>& data) {
    // 接收后数据域逐字节减 0x33（模 256）
    std::vector<uint8_t> out;
    out.reserve(data.size());
    for (uint8_t b : data) {
        out.push_back(static_cast<uint8_t>(b - 0x33));
    }
    return out;
}

}  // namespace BcdCodec
}  // namespace dlt645
