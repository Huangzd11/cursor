# DL/T 645-2007 电能表数据采集 — 项目文档

本文档从需求、架构、数据模型、技术选型、开发计划与使用方式等维度描述本项目。模块级接口与协议细节见 [design.md](design.md)，构建与部署见 [build.md](build.md)。

---

## 1. 需求与目标

### 1.1 背景

在 OpenWrt 网关等 Linux 边缘设备上，需要通过 RS-485/RS-232 串口，按 **DL/T 645-2007** 协议轮询多块电能表，采集电量、电压、电流等运行数据，并以日志形式输出，供运维或上层系统采集。

### 1.2 功能需求

| 编号 | 需求 | 说明 |
|------|------|------|
| F1 | 协议通信 | 实现 DL/T 645-2007 帧编解码、校验、0x33 加减、读数据（控制码 0x11/0x91） |
| F2 | 串口通信 | 支持可配置波特率、数据位、停止位、校验位的串口读写 |
| F3 | 多表轮询 | 按配置依次访问多块电表，每表可采集多个数据项 |
| F4 | 可配置采集项 | 数据标识（DI）、长度、小数位、单位等通过 YAML 定义 |
| F5 | 定时轮询 | 可配置轮询间隔，循环执行采集 |
| F6 | 结果输出 | 采集成功/失败通过结构化日志输出（终端或文件） |
| F7 | 优雅退出 | 支持 SIGINT/SIGTERM 信号停止轮询 |

### 1.3 非功能需求

| 编号 | 需求 | 说明 |
|------|------|------|
| NF1 | 目标平台 | OpenWrt Linux（aarch64，musl） |
| NF2 | 规模 | 小规模部署（约几十块电表） |
| NF3 | 可测试性 | 传输层抽象 `IChannel`，核心逻辑可 Mock 单元测试 |
| NF4 | 开发方式 | 测试驱动开发（TDD），协议层须有充分单测覆盖 |
| NF5 | 可维护性 | 分层架构，模块职责单一，中文注释 |

### 1.4 项目目标

- **正确性**：协议实现与 DL/T 645-2007 一致，帧校验、地址匹配、异常应答处理可靠。
- **可配置**：电表列表、采集项、串口参数、轮询周期均无需改代码即可调整。
- **可部署**：宿主机交叉编译，网关单二进制 + 配置文件即可运行。
- **可验证**：GoogleTest 覆盖协议层至应用层，回归成本低。

### 1.5 范围外（当前版本不做）

- HTTP/MQTT 等网络上报接口
- 数据持久化（数据库、时序库）
- 写表、广播、安全认证等扩展控制码
- 图形界面或远程配置中心

---

## 2. 系统架构

### 2.1 总体视图

系统为**单进程、串口直连、周期性轮询**的采集程序，无独立服务端组件。

```
┌─────────────────────────────────────────────────────────────────┐
│                    OpenWrt 网关 (Linux aarch64)                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              dlt645_collector（本程序）                     │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │  │
│  │  │  Scheduler  │→ │ MeterReader  │→ │ FrameTransceiver │  │  │
│  │  │  轮询调度    │  │  读表逻辑     │  │  帧收发           │  │  │
│  │  └──────┬──────┘  └──────────────┘  └────────┬─────────┘  │  │
│  │         │ ResultReporter                      │ SerialPort  │  │
│  │         ↓ (spdlog)                            ↓             │  │
│  └─────────┼─────────────────────────────────────┼─────────────┘  │
│            │ 日志                                 │ termios        │
└────────────┼─────────────────────────────────────┼────────────────┘
             ↓                                     ↓
        终端 / 日志文件                      /dev/ttyUSB0 等
                                                   │
                                                   │ RS-485
                                                   ↓
                                            ┌──────────────┐
                                            │  电能表 × N   │
                                            │ DL/T 645-2007 │
                                            └──────────────┘
```

### 2.2 软件分层

| 层次 | 目录 | 职责 |
|------|------|------|
| 应用层 | `src/app/` | 配置加载、读表编排、轮询调度、结果上报 |
| 协议层 | `src/protocol/` | BCD 编解码、地址、帧结构、帧编解码、数据项定义 |
| 传输层 | `src/transport/` | 通道抽象、串口实现、帧级收发与粘包处理 |

依赖方向：**应用层 → 协议层 + 传输层**，协议层不依赖传输层。

### 2.3 运行时数据流

```
启动 → 加载 YAML 配置 → 打开串口
  → Scheduler.run() 循环：
       对每个 Meter × 每个 DataItem：
         MeterReader.read_data()
           → 组装请求帧 (0x11)
           → FrameTransceiver 发送/接收
           → 校验应答 (0x91 / 0xD1)
           → DataItem 解码 BCD 数值
         → ResultReporter 写日志
       → sleep(poll_interval_seconds)
  → 收到 SIGINT/SIGTERM → stop() → 退出
```

### 2.4 部署架构

| 角色 | 环境 | 说明 |
|------|------|------|
| 开发机 | x86_64 Linux | 编写代码、运行单元测试、交叉编译 |
| 目标机 | OpenWrt 网关 aarch64 | 运行 `dlt645_collector` + `collector.yaml` |

详见 [build.md](build.md)。

---

## 3. 项目架构

### 3.1 目录结构

```
em645/
├── CMakeLists.txt          # 主构建：库 + 可执行文件 + FetchContent 依赖
├── cmake/
│   └── openwrt-aarch64.cmake   # 交叉编译工具链
├── config/
│   └── collector.yaml      # 示例采集配置
├── document/               # 项目文档（本目录）
│   ├── project.md          # 本文档
│   ├── design.md           # 模块与协议详细设计
│   └── build.md            # 构建与部署
├── scripts/
│   └── build-cross.sh      # 一键交叉编译
├── src/
│   ├── main.cpp            # 程序入口：组装依赖、启动 Scheduler
│   ├── protocol/           # 协议层
│   ├── transport/          # 传输层
│   └── app/                # 应用层
└── tests/                  # GoogleTest 单元测试（仅宿主机构建）
    ├── CMakeLists.txt
    └── test_*.cpp
```

### 3.2 模块与文件映射

| 模块 | 头文件 | 源文件 | 测试 |
|------|--------|--------|------|
| BcdCodec | `protocol/bcd_codec.h` | `protocol/bcd_codec.cpp` | `test_bcd_codec.cpp` |
| Address | `protocol/address.h` | `protocol/address.cpp` | `test_address.cpp` |
| Frame | `protocol/frame.h` | `protocol/frame.cpp` | `test_frame.cpp` |
| FrameCodec | `protocol/frame_codec.h` | `protocol/frame_codec.cpp` | `test_frame_codec.cpp` |
| DataItem | `protocol/data_item.h` | `protocol/data_item.cpp` | `test_data_item.cpp` |
| IChannel | `transport/ichannel.h` | — | Mock（测试内） |
| SerialPort | `transport/serial_port.h` | `transport/serial_port.cpp` | 集成环境 |
| FrameTransceiver | `transport/frame_transceiver.h` | `transport/frame_transceiver.cpp` | `test_frame_transceiver.cpp` |
| Config | `app/config.h` | `app/config.cpp` | `test_config.cpp` |
| MeterReader | `app/meter_reader.h` | `app/meter_reader.cpp` | `test_meter_reader.cpp` |
| ResultReporter | `app/result_reporter.h` | `app/result_reporter.cpp` | `test_result_reporter.cpp` |
| Scheduler | `app/scheduler.h` | `app/scheduler.cpp` | `test_scheduler.cpp` |

### 3.3 构建产物

| 产物 | 说明 |
|------|------|
| `dlt645_lib` | 静态库，供主程序与测试链接 |
| `dlt645_collector` | 网关可执行采集程序 |
| `dlt645_tests` | 单元测试可执行文件（仅本地 `build/`） |

### 3.4 命名空间与编码约定

- 所有业务代码位于命名空间 `dlt645`。
- 源文件与测试一一对应，新增模块须同步补充测试。
- 注释使用中文，说明非显而易见的协议或业务规则。

---

## 4. 数据模型

本项目无关系型数据库；数据以**内存结构**、**YAML 配置**和**日志输出**三种形式存在。

### 4.1 配置模型（持久化：YAML）

```yaml
# 逻辑结构（见 config/collector.yaml）
serial: { device, baudrate, databits?, stopbits?, parity? }
meters: [ { address, name }, ... ]
data_items: [ { di, name, length, decimal, unit }, ... ]
poll_interval_seconds: <int>
```

对应 C++ 类型：`AppConfig`、`MeterConfig`、`SerialPort::Config`、`DataItem`（见 `src/app/config.h`）。

### 4.2 协议模型（运行时）

| 实体 | 类型 | 说明 |
|------|------|------|
| Address | `std::array<uint8_t, 6>` | 6 字节 BCD 地址，低字节在前 |
| Frame | `struct Frame` | `address` + `control` + `data`（已减 0x33） |
| DataItem | `struct DataItem` | DI、名称、长度、小数位、单位；含编解码方法 |

### 4.3 采集结果模型（内存 → 日志）

| 字段 | 类型 | 说明 |
|------|------|------|
| meter_name | `string` | 配置中的电表名称 |
| item_name | `string` | 配置中的数据项名称 |
| code | `MeterReader::ErrorCode` | 成功或失败原因 |
| value | `double` | 解码后的数值（成功时） |
| unit | `string` | 单位，如 kWh、V |

成功时日志示例（由 `ResultReporter` 输出）：

```
[INFO] 采集成功 | 电表=1号电表 | 数据项=正向有功总电能 | 值=1234.56 kWh
```

失败时包含错误码语义，如超时、地址不匹配、异常应答等。

### 4.4 数据标识（DI）参考

常用 DI 与物理量对应关系见 [design.md §3.3](design.md#34-数据域加减-033)。配置中 `di` 为 8 位十六进制字符串（如 `"00010000"`），与协议 4 字节标识一致。

---

## 5. 技术选型

### 5.1 语言与标准

| 项 | 选型 | 理由 |
|----|------|------|
| 语言 | C++17 | 嵌入式资源可控，生态成熟 |
| 构建 | CMake 3.14+ | 跨平台，支持工具链文件交叉编译 |

### 5.2 第三方库

| 库 | 版本 | 用途 | 引入方式 |
|----|------|------|----------|
| GoogleTest | v1.14.0 | 单元测试、Mock | FetchContent（仅宿主机） |
| yaml-cpp | 0.8.0 | YAML 配置解析 | FetchContent |
| spdlog | v1.14.1 | 结构化日志 | FetchContent |

### 5.3 系统 API

| 能力 | 实现 |
|------|------|
| 串口 | Linux `termios`，不引入 libserial 等额外依赖 |
| 信号 | `signal()` 处理 SIGINT/SIGTERM |

### 5.4 工具链

| 场景 | 工具 |
|------|------|
| 本地开发测试 | 系统 GCC/Clang，C++17 |
| 网关部署 | `aarch64-openwrt-linux-musl-gcc` 8.3.0（`openwrt-gcc-8.3.0.tar.gz`） |

### 5.5 选型原则

- 依赖尽量少，便于交叉编译与长期维护。
- 协议与 IO 可测：传输层接口化，避免单测依赖真实硬件。
- 配置外置：现场调整电表与采集项无需重新编译。

---

## 6. 开发计划

### 6.1 方法论

采用 **TDD**：先写失败测试 → 实现最小代码 → 测试通过 → 重构。自底向上按依赖顺序推进。

### 6.2 阶段划分

| 阶段 | 内容 | 验证标准 | 状态 |
|------|------|----------|------|
| P1 | BcdCodec：BCD 编解码、±0x33 | `test_bcd_codec` 通过 | 已完成 |
| P2 | Address：字符串/字节解析、广播地址 | `test_address` 通过 | 已完成 |
| P3 | Frame + FrameCodec：帧编解码、校验和 | `test_frame`、`test_frame_codec` 通过 | 已完成 |
| P4 | DataItem：DI 编码与数值解码 | `test_data_item` 通过 | 已完成 |
| P5 | SerialPort：termios 串口封装 | 实机/集成验证 | 已完成 |
| P6 | FrameTransceiver：帧边界、超时、粘包 | `test_frame_transceiver` 通过 | 已完成 |
| P7 | MeterReader：读表流程与错误码 | `test_meter_reader` 通过 | 已完成 |
| P8 | Config：YAML 加载与校验 | `test_config` 通过 | 已完成 |
| P9 | ResultReporter：日志格式化 | `test_result_reporter` 通过 | 已完成 |
| P10 | Scheduler + main：轮询与信号退出 | `test_scheduler`、交叉编译产物可运行 | 已完成 |

本地验证命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### 6.3 后续可选迭代（未列入当前范围）

| 优先级 | 项 | 说明 |
|--------|-----|------|
| 中 | 日志落盘 | spdlog 配置文件 sink，按日滚动 |
| 中 | 采集指标导出 | Prometheus 文本格式或简单 HTTP 状态页 |
| 低 | 多串口/多通道 | 扩展 Scheduler 支持多 `IChannel` |
| 低 | 写数据、事件上报 | 扩展控制码与帧类型 |

### 6.4 文档维护

- 架构或接口变更时同步更新 `design.md` 与本文档。
- 构建步骤变更时更新 `build.md`。
- 新增对外配置字段时更新本文档第 7 节。

---

## 7. 使用指南（配置与命令行接口）

本项目**不提供 HTTP/gRPC 等网络 API**；对外接口为：**命令行参数**、**YAML 配置文件**和**日志输出**。扩展开发时的 C++ 模块接口见 [design.md §四](design.md#四核心模块设计)。

### 7.1 命令行

```bash
./dlt645_collector [配置文件路径]
```

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 配置文件路径 | `config/collector.yaml` | 第一个命令行参数，指定 YAML 路径 |

退出码：`0` 正常结束；`1` 配置错误或串口打开失败。

### 7.2 配置文件说明

完整示例：`config/collector.yaml`。

#### serial（串口）

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| device | string | 是 | — | 设备路径，如 `/dev/ttyUSB0`、`/dev/ttyS1` |
| baudrate | int | 是 | — | 波特率，常见 2400、9600 |
| databits | int | 否 | 8 | 数据位 |
| stopbits | int | 否 | 1 | 停止位 |
| parity | string | 否 | `E` | `N` 无 / `E` 偶 / `O` 奇校验 |

#### meters（电表列表）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| address | string | 是 | 12 位十六进制通信地址，如 `"000000000001"` |
| name | string | 是 | 显示名称，用于日志 |

#### data_items（采集项）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| di | string | 是 | 8 位十六进制数据标识，如 `"00010000"` |
| name | string | 是 | 数据项名称 |
| length | int | 是 | 数据域字节长度（不含 DI） |
| decimal | int | 是 | 小数位数 |
| unit | string | 是 | 单位，如 `kWh`、`V`、`A` |

#### poll_interval_seconds

两轮完整轮询之间的间隔（秒），整型，建议 ≥ 10。

### 7.3 配置校验规则

`Config::validate()` 在启动时检查，失败则打印错误并退出，常见规则包括：

- 至少一块电表、至少一个数据项；
- 电表地址为 12 位十六进制；
- DI 为 8 位十六进制；
- 波特率在支持范围内。

### 7.4 部署与运行

```bash
# 交叉编译（开发机）
./scripts/build-cross.sh

# 拷贝到网关
scp build-cross/dlt645_collector config/collector.yaml root@<网关IP>:/opt/dlt645/

# 网关运行（按实际串口修改 device）
cd /opt/dlt645 && ./dlt645_collector collector.yaml
```

### 7.5 日志与排错

| 现象 | 可能原因 | 处理建议 |
|------|----------|----------|
| 串口打开失败 | 设备路径错误或权限不足 | 检查 `device`，将用户加入 dialout 或使用 root |
| 接收超时 | 波特率/接线/地址不匹配 | 核对表计通信参数与 `address` |
| 地址不匹配 | 表计实际地址与配置不一致 | 用表计屏显或厂家工具确认地址 |
| 异常应答 0xD1 | DI 不支持或表计忙 | 核对 `di`、`length`、`decimal` 与表计规约 |

### 7.6 开发者接口索引

若需二次开发或编写新测试，请直接查阅 [design.md](design.md) 中的类图与接口定义，重点类型：

- `IChannel` / `SerialPort` — 传输抽象与实现
- `FrameTransceiver` — 帧级收发
- `MeterReader::read_data` — 单次读表
- `Scheduler::poll_once` / `set_result_callback` — 轮询与回调

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [project.md](project.md) | 本文档：需求、架构、计划、使用指南 |
| [design.md](design.md) | 协议要点、模块接口、配置示例 |
| [build.md](build.md) | 本地测试与 OpenWrt 交叉编译部署 |
