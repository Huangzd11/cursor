# DL/T 645-2007 电能表数据采集程序 - 设计文档

## 一、概述

本程序是一个运行在 Linux 平台上的电能表数据采集工具，基于 DL/T 645-2007 通信协议，通过串口（RS-485/RS-232）与电能表通信，支持可配置的数据项采集和多表轮询。

### 1.1 设计目标

- 正确实现 DL/T 645-2007 协议的帧编解码
- 通过串口与电能表通信，支持多表轮询采集
- 采集数据项通过 YAML 配置文件灵活定义
- 采集结果通过日志输出（终端/文件）
- 采用 TDD 开发方式，确保协议实现的正确性

### 1.2 适用范围

- 操作系统：Linux
- 协议版本：DL/T 645-2007
- 通信接口：串口（RS-485/RS-232）
- 电表规模：小规模（几十块）

## 二、系统架构

系统分为三层：

```
┌─────────────────────────────────────┐
│            应用层 (app/)             │
│  Scheduler / Config / MeterReader   │
├─────────────────────────────────────┤
│           协议层 (protocol/)         │
│       Frame / DataItem              │
├─────────────────────────────────────┤
│           传输层 (transport/)        │
│     IChannel / SerialPort           │
└─────────────────────────────────────┘
```

- **传输层**：封装串口读写，提供 IChannel 抽象接口便于测试
- **协议层**：实现 645 协议帧的组装/解析、数据标识的 BCD 编解码
- **应用层**：读表操作、轮询调度、配置管理、日志输出

## 三、DL/T 645-2007 协议要点

### 3.1 帧格式

```
FE FE FE FE 68 A0 A1 A2 A3 A4 A5 68 C L D0...Dn CS 16
|前导符(可选)| |起始|   地址域(6B)   |起始|C|L| 数据域 |校验|结束|
```

| 字段   | 长度    | 说明                                     |
|--------|---------|------------------------------------------|
| 前导符 | >=1 byte| 0xFE，用于唤醒从站，发送时至少4个        |
| 起始符 | 1 byte  | 固定 0x68                                |
| 地址域 | 6 bytes | 电表通信地址，BCD码，低字节在前          |
| 起始符 | 1 byte  | 固定 0x68                                |
| 控制码 | 1 byte  | 功能码，见下表                           |
| 数据长度| 1 byte | 数据域字节数                              |
| 数据域 | N bytes | 数据标识 + 数据内容，每字节加 0x33 传输   |
| 校验码 | 1 byte  | 从第一个 0x68 到数据域最后一字节的模256和 |
| 结束符 | 1 byte  | 固定 0x16                                |

### 3.2 控制码

| 控制码 | 方向   | 说明             |
|--------|--------|------------------|
| 0x11   | 主→从  | 读数据           |
| 0x91   | 从→主  | 读数据正常应答   |
| 0xD1   | 从→主  | 读数据异常应答   |

### 3.3 数据标识 (DI)

数据标识为 4 字节，标识具体的数据项，例如：

| DI3 DI2 DI1 DI0 | 数据项         | 长度   | 格式       |
|------------------|----------------|--------|------------|
| 00 01 00 00      | 正向有功总电能 | 4 bytes| XXXXXX.XX  |
| 00 02 00 00      | 反向有功总电能 | 4 bytes| XXXXXX.XX  |
| 02 01 01 00      | A相电压        | 2 bytes| XXX.X      |
| 02 02 01 00      | A相电流        | 3 bytes| XXX.XXX    |
| 02 03 00 00      | 瞬时总有功功率 | 3 bytes| XX.XXXX    |
| 02 06 00 00      | 频率           | 2 bytes| XX.XX      |

### 3.4 数据域加减 0x33

发送时数据域每字节加 0x33，接收时每字节减 0x33，用于避免数据中出现帧定界符。

## 四、核心模块设计

### 4.1 Frame（协议帧）

```cpp
struct Frame {
    std::array<uint8_t, 6> address;  // 电表地址（原始BCD，低位在前）
    uint8_t control;                  // 控制码
    std::vector<uint8_t> data;        // 数据域（减0x33后的原始数据）

    // 组装为完整帧字节流（含前导符）
    std::vector<uint8_t> encode() const;

    // 从字节流中解析帧（自动跳过前导符0xFE）
    static std::optional<Frame> decode(const std::vector<uint8_t>& bytes);
};
```

### 4.2 DataItem（数据标识）

```cpp
struct DataItem {
    uint32_t di;           // 4字节数据标识
    std::string name;      // 可读名称
    int data_length;       // 数据字段长度（字节数）
    int decimal_digits;    // 小数位数
    std::string unit;      // 单位

    // 将原始BCD字节解码为浮点数值
    double decode_value(const std::vector<uint8_t>& raw) const;

    // 将DI编码为4字节（用于组装请求帧数据域，需加0x33）
    std::vector<uint8_t> encode_di() const;
};
```

### 4.3 IChannel（通信通道接口）

```cpp
class IChannel {
public:
    virtual ~IChannel() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool send(const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> receive(size_t max_bytes, int timeout_ms) = 0;
};
```

### 4.4 SerialPort（串口实现）

基于 Linux termios API，默认参数：2400bps、8数据位、1停止位、偶校验。

### 4.5 MeterReader（读表器）

```cpp
class MeterReader {
public:
    explicit MeterReader(std::shared_ptr<IChannel> channel);

    // 读取指定电表的指定数据项
    std::optional<double> read_data(
        const std::array<uint8_t, 6>& address,
        const DataItem& item);
};
```

内部流程：构造请求帧 → 发送 → 接收应答 → 解析验证 → 解码数据。

### 4.6 Config（配置管理）

使用 YAML 格式配置文件，定义串口参数、电表列表和数据项列表。

### 4.7 Scheduler（轮询调度）

按配置的轮询间隔，循环遍历每块电表的每个数据项进行采集，结果通过日志输出。

## 五、配置文件格式

```yaml
serial:
  device: "/dev/ttyUSB0"
  baudrate: 2400

meters:
  - address: "000000000001"
    name: "1号电表"
  - address: "000000000002"
    name: "2号电表"

data_items:
  - di: "00010000"
    name: "正向有功总电能"
    length: 4
    decimal: 2
    unit: "kWh"
  - di: "02010100"
    name: "A相电压"
    length: 2
    decimal: 1
    unit: "V"

poll_interval_seconds: 60
```

## 六、第三方依赖

| 库         | 用途           | 说明                           |
|------------|----------------|--------------------------------|
| GoogleTest | 单元测试框架   | TDD 核心                      |
| yaml-cpp   | YAML 配置解析  | 解析采集配置文件               |
| spdlog     | 日志库         | 结构化日志输出                 |

串口通信直接使用 Linux termios API，不引入额外串口库。

## 七、TDD 开发顺序

按依赖关系从底向上，每步先写测试再实现：

1. Frame 帧编解码（纯逻辑，无外部依赖）
2. DataItem BCD 编解码
3. SerialPort 串口通信
4. MeterReader 读表流程（使用 MockChannel）
5. Config 配置解析
6. Scheduler 轮询调度 + 集成测试
