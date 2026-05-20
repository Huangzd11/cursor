#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dlt645 {

// 可采集数据项的元信息定义
struct DataItem {
    uint32_t di = 0;           // 4字节数据标识（如 0x00010000 表示正向有功总电能）
    std::string name;          // 可读名称
    int data_length = 0;       // 数据字段长度（字节数，不含DI本身）
    int decimal_digits = 0;    // 小数位数
    std::string unit;          // 单位
    bool enabled = true;       // 是否采集该项（false 则所有表均跳过）

    // 将DI编码为4字节序列（低位在前，用于组装请求帧数据域）
    std::vector<uint8_t> encode_di() const;

    // 将应答帧中的原始BCD数据解码为浮点数值
    double decode_value(const std::vector<uint8_t>& raw) const;
};

}  // namespace dlt645
