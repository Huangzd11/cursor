#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace dlt645 {

struct Frame {
    std::array<uint8_t, 6> address;  // 电表地址，BCD码，低位在前
    uint8_t control;                  // 控制码
    std::vector<uint8_t> data;        // 数据域（已减0x33后的原始数据）

    // 组装为完整帧字节流（含4字节前导符0xFE）
    std::vector<uint8_t> encode() const;

    // 从字节流中解析帧（自动跳过前导符0xFE），解析失败返回 nullopt
    static std::optional<Frame> decode(const std::vector<uint8_t>& bytes);

    // 计算校验码：从第一个0x68到数据域最后一字节的模256和
    static uint8_t calc_checksum(const std::vector<uint8_t>& frame_bytes,
                                 size_t start, size_t end);
};

// 控制码常量
constexpr uint8_t CTRL_READ_DATA     = 0x11;  // 读数据请求
constexpr uint8_t CTRL_READ_DATA_OK  = 0x91;  // 读数据正常应答
constexpr uint8_t CTRL_READ_DATA_ERR = 0xD1;  // 读数据异常应答

constexpr uint8_t FRAME_START = 0x68;  // 帧起始符
constexpr uint8_t FRAME_END   = 0x16;  // 帧结束符
constexpr uint8_t PREAMBLE    = 0xFE;  // 前导符
constexpr uint8_t DATA_MASK   = 0x33;  // 数据域掩码

}  // namespace dlt645
