#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "protocol/frame.h"

namespace dlt645 {

// 帧编解码器：负责Frame与字节流之间的转换
namespace FrameCodec {

struct DecodeResult {
    Frame frame;
    size_t bytes_consumed;  // 从输入中消费的字节数（含前导符）
};

// 将Frame编码为完整字节流（含4字节前导符、0x33加操作、校验和）
std::vector<uint8_t> encode(const Frame& frame);

// 从字节流解析Frame（跳过前导符、0x33减操作、校验和验证）
std::optional<DecodeResult> decode(const std::vector<uint8_t>& bytes);

// 计算校验和：从 data[start] 到 data[end-1] 的模256和
uint8_t calc_checksum(const uint8_t* data, size_t len);

}  // namespace FrameCodec

}  // namespace dlt645
