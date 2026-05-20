#include "protocol/data_item.h"
#include "protocol/bcd_codec.h"

namespace dlt645 {

std::vector<uint8_t> DataItem::encode_di() const {
    // DI低位在前：DI0, DI1, DI2, DI3
    std::vector<uint8_t> result(4);
    result[0] = static_cast<uint8_t>(di & 0xFF);
    result[1] = static_cast<uint8_t>((di >> 8) & 0xFF);
    result[2] = static_cast<uint8_t>((di >> 16) & 0xFF);
    result[3] = static_cast<uint8_t>((di >> 24) & 0xFF);
    return result;
}

double DataItem::decode_value(const std::vector<uint8_t>& raw) const {
    if (raw.empty()) {
        return 0.0;
    }
    return BcdCodec::decode_float(raw.data(), raw.size(), decimal_digits);
}

}  // namespace dlt645
