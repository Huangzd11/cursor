#include "protocol/frame.h"

namespace dlt645 {

std::vector<uint8_t> Frame::encode() const {
    std::vector<uint8_t> result;

    // 4字节前导符
    for (int i = 0; i < 4; ++i) {
        result.push_back(PREAMBLE);
    }

    // 帧起始位置（校验码从这里开始计算）
    size_t cs_start = result.size();

    // 起始符
    result.push_back(FRAME_START);

    // 地址域（6字节，低位在前）
    for (auto byte : address) {
        result.push_back(byte);
    }

    // 第二个起始符
    result.push_back(FRAME_START);

    // 控制码
    result.push_back(control);

    // 数据长度
    result.push_back(static_cast<uint8_t>(data.size()));

    // 数据域（每字节加0x33）
    for (auto byte : data) {
        result.push_back(static_cast<uint8_t>(byte + DATA_MASK));
    }

    // 校验码
    size_t cs_end = result.size();
    result.push_back(calc_checksum(result, cs_start, cs_end));

    // 结束符
    result.push_back(FRAME_END);

    return result;
}

std::optional<Frame> Frame::decode(const std::vector<uint8_t>& bytes) {
    // 跳过前导符 0xFE
    size_t pos = 0;
    while (pos < bytes.size() && bytes[pos] == PREAMBLE) {
        ++pos;
    }

    // 最小帧长度: 68 + 6(addr) + 68 + C + L + CS + 16 = 12字节（无数据域）
    if (pos + 12 > bytes.size()) {
        return std::nullopt;
    }

    // 验证第一个起始符
    if (bytes[pos] != FRAME_START) {
        return std::nullopt;
    }

    size_t cs_start = pos;
    ++pos;

    // 地址域
    Frame frame{};
    for (int i = 0; i < 6; ++i) {
        frame.address[i] = bytes[pos++];
    }

    // 验证第二个起始符
    if (bytes[pos] != FRAME_START) {
        return std::nullopt;
    }
    ++pos;

    // 控制码
    frame.control = bytes[pos++];

    // 数据长度
    uint8_t data_len = bytes[pos++];

    // 检查剩余长度是否足够（数据域 + CS + 结束符）
    if (pos + data_len + 2 > bytes.size()) {
        return std::nullopt;
    }

    // 数据域（每字节减0x33）
    frame.data.resize(data_len);
    for (int i = 0; i < data_len; ++i) {
        frame.data[i] = static_cast<uint8_t>(bytes[pos + i] - DATA_MASK);
    }
    pos += data_len;

    // 校验码验证
    size_t cs_end = pos;
    uint8_t expected_cs = calc_checksum(bytes, cs_start, cs_end);
    if (bytes[pos] != expected_cs) {
        return std::nullopt;
    }
    ++pos;

    // 验证结束符
    if (bytes[pos] != FRAME_END) {
        return std::nullopt;
    }

    return frame;
}

uint8_t Frame::calc_checksum(const std::vector<uint8_t>& frame_bytes,
                              size_t start, size_t end) {
    uint8_t sum = 0;
    for (size_t i = start; i < end; ++i) {
        sum += frame_bytes[i];
    }
    return sum;
}

}  // namespace dlt645
