#pragma once

#include <cstdint>
#include <vector>

#include "protocol/address.h"

namespace dlt645 {

// 控制码常量
constexpr uint8_t CTRL_READ_DATA     = 0x11;  // 读数据请求
constexpr uint8_t CTRL_READ_DATA_OK  = 0x91;  // 读数据正常应答
constexpr uint8_t CTRL_READ_DATA_ERR = 0xD1;  // 读数据异常应答

// 帧定界符常量
constexpr uint8_t FRAME_START = 0x68;
constexpr uint8_t FRAME_END   = 0x16;
constexpr uint8_t PREAMBLE    = 0xFE;
constexpr uint8_t DATA_MASK   = 0x33;

// 645协议帧结构（纯数据，不含编解码逻辑）
struct Frame {
    Address address;                   // 电表地址
    uint8_t control = 0;               // 控制码
    std::vector<uint8_t> data;         // 数据域（已减0x33的原始数据）

    // 是否为异常应答帧（控制码最高位为1且第6位为1）
    bool is_error_response() const;

    // 获取异常应答错误码（data[0]，仅当 is_error_response() 时有效）
    uint8_t error_code() const;
};

}  // namespace dlt645
