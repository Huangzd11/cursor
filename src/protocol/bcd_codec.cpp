#include "protocol/bcd_codec.h"

namespace dlt645 {
namespace BcdCodec {

uint64_t decode(const uint8_t* data, size_t len) {
    // TODO: 实现BCD解码
    return 0;
}

double decode_float(const uint8_t* data, size_t len, int decimal_digits) {
    // TODO: 实现BCD浮点解码
    return 0.0;
}

std::vector<uint8_t> encode(uint64_t value, size_t len) {
    // TODO: 实现BCD编码
    return std::vector<uint8_t>(len, 0);
}

std::vector<uint8_t> add33(const std::vector<uint8_t>& data) {
    // TODO: 实现加0x33
    return data;
}

std::vector<uint8_t> sub33(const std::vector<uint8_t>& data) {
    // TODO: 实现减0x33
    return data;
}

}  // namespace BcdCodec
}  // namespace dlt645
