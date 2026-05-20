# DL/T 645-2007 电能表数据采集程序 - 设计文档

> 项目总览（需求、系统/项目架构、数据模型、技术选型、开发计划、使用指南）见 [project.md](project.md)。技术难点与创新点见 [technical-highlights.md](technical-highlights.md)。本文档侧重协议要点与模块接口设计。

## 一、概述

本程序是一个运行在 Linux 平台上的电能表数据采集工具，基于 DL/T 645-2007 通信协议，通过串口（RS-485/RS-232）与电能表通信，支持可配置的数据项采集和多表轮询。

### 1.1 设计目标

- 正确实现 DL/T 645-2007 协议的帧编解码
- 通过串口与电能表通信，支持多表轮询采集
- 采集数据项通过 YAML 配置文件灵活定义
- 采集结果通过日志输出（终端/文件）
- 采用 TDD 开发方式，确保协议实现的正确性

### 1.2 适用范围

- 操作系统：OpenWrt Linux（aarch64，musl）
- 协议版本：DL/T 645-2007
- 通信接口：串口（RS-485/RS-232）
- 电表规模：小规模（几十块）

### 1.3 构建与部署

- **开发机**：x86_64 Linux，本地编译并运行 GoogleTest 单元测试（TDD）
- **目标机**：OpenWrt 网关终端，运行交叉编译产物
- **工具链**：`openwrt-gcc-8.3.0.tar.gz`（`aarch64-openwrt-linux-musl-gcc` 8.3.0）

详细步骤见 [build.md](build.md)。

## 二、系统架构

系统分为三层：

```
┌──────────────────────────────────────────────────────────┐
│                      应用层 (app/)                        │
│  Config / MeterReader / Scheduler / ResultReporter       │
├──────────────────────────────────────────────────────────┤
│                      协议层 (protocol/)                   │
│  BcdCodec / Address / Frame / FrameCodec / DataItem      │
├──────────────────────────────────────────────────────────┤
│                      传输层 (transport/)                  │
│  IChannel / SerialPort / FrameTransceiver                │
└──────────────────────────────────────────────────────────┘
```

- **传输层**：封装串口读写（IChannel / SerialPort），提供帧级收发能力（FrameTransceiver）
- **协议层**：BCD 编解码工具（BcdCodec）、电表地址处理（Address）、帧结构与编解码（Frame / FrameCodec）、数据标识定义与值解码（DataItem）
- **应用层**：配置管理（Config）、读表操作（MeterReader）、轮询调度（Scheduler）、结果输出（ResultReporter）

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
| 02 03 01 00      | 当前有功功率   | 3 bytes| XX.XXXX    |
| 02 05 01 00      | 当前视在功率   | 3 bytes| XX.XXXX    |
| 02 06 01 00      | 当前功率因数   | 2 bytes| X.XXX      |
| 02 06 00 00      | 频率           | 2 bytes| XX.XX      |

### 3.4 数据域加减 0x33

发送时数据域每字节加 0x33，接收时每字节减 0x33，用于避免数据中出现帧定界符。

## 四、核心模块设计

### 4.1 BcdCodec（BCD 编解码工具）

纯工具类，提供 BCD 编解码和 0x33 加减操作，被 Frame、DataItem、Address 等模块共用。

```cpp
namespace BcdCodec {
    // BCD字节序列 → 整数值（如 0x12 0x34 → 1234）
    uint64_t decode(const uint8_t* data, size_t len);

    // BCD字节序列 → 浮点值（指定小数位数，如 0x12 0x34 + 2位小数 → 12.34）
    double decode_float(const uint8_t* data, size_t len, int decimal_digits);

    // 整数值 → BCD字节序列（指定输出长度）
    std::vector<uint8_t> encode(uint64_t value, size_t len);

    // 数据域加0x33（发送前处理）
    std::vector<uint8_t> add33(const std::vector<uint8_t>& data);

    // 数据域减0x33（接收后处理）
    std::vector<uint8_t> sub33(const std::vector<uint8_t>& data);
}
```

### 4.2 Address（电表地址）

封装电表通信地址的解析、校验和格式转换。

```cpp
class Address {
public:
    // 从12位十六进制字符串构造（如 "000000000001"）
    static std::optional<Address> from_string(const std::string& hex_str);

    // 从6字节BCD数组构造
    static Address from_bytes(const std::array<uint8_t, 6>& bytes);

    // 获取原始6字节（低位在前，用于帧组装）
    const std::array<uint8_t, 6>& bytes() const;

    // 转为可读字符串
    std::string to_string() const;

    // 是否为广播地址（0x999999999999）
    bool is_broadcast() const;

    bool operator==(const Address& other) const;

private:
    std::array<uint8_t, 6> bytes_;
};
```

### 4.3 Frame（协议帧结构）

仅负责帧的结构化表示，不涉及通信。

```cpp
struct Frame {
    Address address;                   // 电表地址
    uint8_t control;                   // 控制码
    std::vector<uint8_t> data;         // 数据域（已减0x33的原始数据）

    // 是否为异常应答帧
    bool is_error_response() const;

    // 获取异常应答的错误码（仅当 is_error_response() 为 true）
    uint8_t error_code() const;
};
```

### 4.4 FrameCodec（帧编解码器）

负责帧与字节流之间的转换，包括校验和计算。

```cpp
namespace FrameCodec {
    // 将Frame编码为完整字节流（含前导符0xFE、0x33加操作、校验和）
    std::vector<uint8_t> encode(const Frame& frame);

    // 从字节流解析Frame（自动跳过前导符、0x33减操作、校验和验证）
    // 返回解析结果和已消费的字节数
    struct DecodeResult {
        Frame frame;
        size_t bytes_consumed;
    };
    std::optional<DecodeResult> decode(const std::vector<uint8_t>& bytes);

    // 计算校验和（从第一个0x68到数据域末尾的模256和）
    uint8_t calc_checksum(const uint8_t* data, size_t len);
}
```

### 4.5 DataItem（数据标识定义）

描述一个可采集的数据项元信息，并提供值解码能力。

```cpp
struct DataItem {
    uint32_t di;           // 4字节数据标识（如 0x00010000）
    std::string name;      // 可读名称（如 "正向有功总电能"）
    int data_length;       // 数据字段长度（字节数，不含DI本身）
    int decimal_digits;    // 小数位数
    std::string unit;      // 单位（如 "kWh"）

    // 将DI编码为4字节序列（低位在前，用于组装请求帧数据域）
    std::vector<uint8_t> encode_di() const;

    // 将应答帧中的原始BCD数据解码为浮点数值
    double decode_value(const std::vector<uint8_t>& raw) const;
};
```

### 4.6 IChannel（通信通道接口）

传输层抽象，隔离底层通信方式，便于单元测试时注入 MockChannel。

```cpp
class IChannel {
public:
    virtual ~IChannel() = default;

    // 打开通道
    virtual bool open() = 0;

    // 关闭通道
    virtual void close() = 0;

    // 发送数据，返回是否成功
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    // 接收数据，最多等待 timeout_ms 毫秒
    // 返回实际收到的字节（可能少于 max_bytes）
    virtual std::vector<uint8_t> receive(size_t max_bytes, int timeout_ms) = 0;

    // 通道是否已打开
    virtual bool is_open() const = 0;
};
```

### 4.7 SerialPort（串口实现）

IChannel 的 Linux 串口实现，基于 termios API。

```cpp
class SerialPort : public IChannel {
public:
    struct Config {
        std::string device;     // 设备路径，如 "/dev/ttyUSB0"
        int baudrate = 2400;    // 波特率
        int databits = 8;       // 数据位
        int stopbits = 1;       // 停止位
        char parity = 'E';      // 校验：'N'无/'E'偶/'O'奇
    };

    explicit SerialPort(const Config& config);

    bool open() override;
    void close() override;
    bool send(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receive(size_t max_bytes, int timeout_ms) override;
    bool is_open() const override;

private:
    Config config_;
    int fd_ = -1;
};
```

### 4.8 FrameTransceiver（帧收发器）

在 IChannel 之上封装帧级别的发送/接收逻辑，处理字节流中的帧边界识别和超时管理。

```cpp
class FrameTransceiver {
public:
    explicit FrameTransceiver(std::shared_ptr<IChannel> channel);

    // 发送一帧（自动编码）
    bool send_frame(const Frame& frame);

    // 接收一帧完整应答（处理部分读取、前导符跳过、帧边界检测）
    // timeout_ms 为整体超时，内部可能多次调用 channel->receive()
    std::optional<Frame> receive_frame(int timeout_ms);

private:
    std::shared_ptr<IChannel> channel_;
    std::vector<uint8_t> recv_buffer_;  // 接收缓冲区，处理跨次读取的粘包
};
```

### 4.9 MeterReader（读表器）

单次读表操作的封装，职责：组装请求帧、收发交互、应答校验、值提取。

```cpp
class MeterReader {
public:
    explicit MeterReader(std::shared_ptr<FrameTransceiver> transceiver);

    // 读取结果
    struct ReadResult {
        double value;
        std::string unit;
    };

    // 读取失败的原因
    enum class ErrorCode {
        kSuccess,
        kSendFailed,       // 发送失败
        kTimeout,          // 接收超时
        kAddressMismatch,  // 应答地址不匹配
        kErrorResponse,    // 电表返回异常应答
        kDataMismatch,     // 应答数据标识不匹配
        kDecodeFailed      // 数据解码失败
    };

    // 读取指定电表的指定数据项（meter_name 为配置中的可读名称，用于日志）
    std::pair<ErrorCode, std::optional<ReadResult>> read_data(
        std::string_view meter_name,
        const Address& address,
        const DataItem& item);

private:
    // 构造读数据请求帧
    Frame build_request(const Address& address, const DataItem& item);

    // 校验应答帧（地址匹配、控制码、数据标识匹配）
    ErrorCode validate_response(const Frame& response,
                                const Address& expected_addr,
                                const DataItem& expected_item);

    std::shared_ptr<FrameTransceiver> transceiver_;
};
```

### 4.10 Config（配置管理）

YAML 配置文件的解析和校验，将配置文件转为类型安全的结构体。

```cpp
struct AppConfig {
    SerialPort::Config serial;            // 串口配置
    std::vector<MeterConfig> meters;      // 电表列表
    std::vector<DataItem> data_items;     // 采集数据项列表
    int poll_interval_seconds;            // 轮询间隔（秒）
};

struct MeterConfig {
    Address address;
    std::string name;
};

class Config {
public:
    // 从YAML文件加载配置
    static std::optional<AppConfig> load(const std::string& filepath);

    // 校验配置合法性（地址格式、DI长度、波特率范围等）
    static std::vector<std::string> validate(const AppConfig& config);
};
```

### 4.11 Scheduler（轮询调度器）

按配置的时间间隔，循环遍历每块电表的每个数据项进行采集。

```cpp
class Scheduler {
public:
    Scheduler(std::shared_ptr<MeterReader> reader,
              const AppConfig& config);

    // 启动轮询（阻塞，直到调用 stop()）
    void run();

    // 停止轮询（可从其他线程调用）
    void stop();

    // 执行一轮采集（遍历所有电表的所有数据项）
    void poll_once();

    // 设置采集结果回调
    using ResultCallback = std::function<void(
        const std::string& meter_name,
        const std::string& item_name,
        MeterReader::ErrorCode code,
        std::optional<MeterReader::ReadResult> result)>;
    void set_result_callback(ResultCallback callback);

private:
    std::shared_ptr<MeterReader> reader_;
    AppConfig config_;
    std::atomic<bool> running_{false};
    ResultCallback callback_;
};
```

### 4.12 ResultReporter（结果输出）

将采集结果格式化并通过日志输出，与采集逻辑解耦。

```cpp
class ResultReporter {
public:
    // 作为 Scheduler 的回调使用
    void report(const std::string& meter_name,
                const std::string& item_name,
                MeterReader::ErrorCode code,
                std::optional<MeterReader::ReadResult> result);
};
```

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

1. BcdCodec BCD 编解码与 0x33 加减（纯函数，无依赖）
2. Address 电表地址解析与校验（依赖 BcdCodec）
3. Frame + FrameCodec 帧结构与编解码（依赖 BcdCodec、Address）
4. DataItem 数据标识编码与值解码（依赖 BcdCodec）
5. SerialPort 串口通信（Linux termios）
6. FrameTransceiver 帧收发器（使用 MockChannel 测试）
7. MeterReader 读表流程（使用 Mock FrameTransceiver 测试）
8. Config 配置解析与校验
9. ResultReporter 结果输出
10. Scheduler 轮询调度 + 集成测试
