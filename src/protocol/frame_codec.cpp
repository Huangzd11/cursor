#include "protocol/frame_codec.h"
#include "protocol/bcd_codec.h"

namespace dlt645 {
namespace FrameCodec {

std::vector<uint8_t> encode(const Frame& frame) {
    std::vector<uint8_t> result;

    // 4字节前导符
    for (int i = 0; i < 4; ++i) {
        result.push_back(PREAMBLE);
    }

    size_t cs_start = result.size();

    // 第一个起始符
    result.push_back(FRAME_START);

    // 地址域（6字节）
    for (auto byte : frame.address.bytes()) {
        result.push_back(byte);
    }

    // 第二个起始符
    result.push_back(FRAME_START);

    // 控制码
    result.push_back(frame.control);

    // 数据长度
    result.push_back(static_cast<uint8_t>(frame.data.size()));

    // 数据域（每字节加0x33）
    auto encoded_data = BcdCodec::add33(frame.data);
    for (auto byte : encoded_data) {
        result.push_back(byte);
    }

    // 校验码
    uint8_t cs = calc_checksum(result.data() + cs_start, result.size() - cs_start);
    result.push_back(cs);

    // 结束符
    result.push_back(FRAME_END);

    return result;
}

std::optional<DecodeResult> decode(const std::vector<uint8_t>& bytes) {
    // 跳过前导符
    size_t pos = 0;
    while (pos < bytes.size() && bytes[pos] == PREAMBLE) {
        ++pos;
    }

    // 最小帧长度: 68 + 6(addr) + 68 + C + L + CS + 16 = 12字节
    if (pos + 12 > bytes.size()) {
        return std::nullopt;
    }

    if (bytes[pos] != FRAME_START) {
        return std::nullopt;
    }

    size_t cs_start = pos;
    ++pos;

    // 地址域
    std::array<uint8_t, 6> addr_bytes{};
    for (int i = 0; i < 6; ++i) {
        addr_bytes[i] = bytes[pos++];
    }

    // 第二个起始符
    if (bytes[pos] != FRAME_START) {
        return std::nullopt;
    }
    ++pos;

    // 控制码
    uint8_t control = bytes[pos++];

    // 数据长度
    uint8_t data_len = bytes[pos++];

    // 检查剩余长度
    if (pos + data_len + 2 > bytes.size()) {
        return std::nullopt;
    }

    // 数据域（每字节减0x33）
    std::vector<uint8_t> raw_data(bytes.begin() + pos, bytes.begin() + pos + data_len);
    auto decoded_data = BcdCodec::sub33(raw_data);
    pos += data_len;

    // 校验码验证
    uint8_t expected_cs = calc_checksum(bytes.data() + cs_start, pos - cs_start);
    if (bytes[pos] != expected_cs) {
        return std::nullopt;
    }
    ++pos;

    // 结束符
    if (bytes[pos] != FRAME_END) {
        return std::nullopt;
    }
    ++pos;

    Frame frame;
    frame.address = Address::from_bytes(addr_bytes);
    frame.control = control;
    frame.data = std::move(decoded_data);

    return DecodeResult{std::move(frame), pos};
}

uint8_t calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

}  // namespace FrameCodec
}  // namespace dlt645
