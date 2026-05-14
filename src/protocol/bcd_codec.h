#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlt645 {

// BCD编解码与0x33加减操作工具
namespace BcdCodec {

// BCD字节序列 → 整数值（高位在前，如 {0x12, 0x34} → 1234）
uint64_t decode(const uint8_t* data, size_t len);

// BCD字节序列 → 浮点值（指定小数位数，如 {0x12, 0x34} + 2位小数 → 12.34）
double decode_float(const uint8_t* data, size_t len, int decimal_digits);

// 整数值 → BCD字节序列（高位在前，指定输出长度）
std::vector<uint8_t> encode(uint64_t value, size_t len);

// 数据域每字节加0x33（发送前处理）
std::vector<uint8_t> add33(const std::vector<uint8_t>& data);

// 数据域每字节减0x33（接收后处理）
std::vector<uint8_t> sub33(const std::vector<uint8_t>& data);

}  // namespace BcdCodec

}  // namespace dlt645
