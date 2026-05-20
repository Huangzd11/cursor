# MQTT 上报数据规划

> 本文档描述在现有 **DL/T 645 串口采集** 能力之上，增加 **MQTT 上报** 的目标、接口落点、Topic/Payload 约定、配置草案与分阶段实施建议。  
> **实现状态（2026-05）**：已落地首版——`MqttReporter`（Eclipse Paho C、`tcp://` 明文）、`collector.yaml` 中 `mqtt` 配置、`Scheduler` 回调携带 `meter_address_hex` 与 `di_hex`；**TLS 仍未支持**（校验会拒绝 `mqtt.tls: true`）。

---

## 1. 背景与目标

| 维度 | 说明 |
|------|------|
| 动机 | 网关采集结果除本地日志外，需接入物联网平台、时序库或上层 SCADA，MQTT 为边缘侧常见上行通道。 |
| 目标 | 在采集完成（成功/失败）后，将结构化数据发布到 MQTT Broker；与现有日志解耦、可开关、可配置。 |
| 非目标（首版可不做） | 下行控制订阅、OTA、复杂规则引擎；替代现有日志。 |

---

## 2. 与现有架构的关系

当前数据流：`Scheduler` 轮询 → `MeterReader::read_data` → 通过 `set_result_callback` 回调 → `ResultReporter` 打日志；若启用 MQTT → 同一回调内 **`MqttReporter::report`** 入队异步发布。

**已实现落点**：

1. **`MqttReporter`**（`src/app/mqtt_reporter.*`）：独立线程 + 有界队列，使用 Paho **同步客户端**在工作线程内 `connect`/`publish`。
2. **不侵入协议层/传输层**。
3. **`mqtt.enabled: false`**（默认）时不启动 MQTT 工作线程、不连接 Broker。
4. **上送日志**：`MQTTClient_publish` 成功后输出 `INFO`：`MQTT 上送 | topic=... | qos=... | bytes=... | payload={...}`，与终端及 `log/dlt645_*.log` 同源。

---

## 3. Topic 规划

采用 **层次化 Topic**，便于平台按网关 / 电表 / 数据项订阅与 ACL 控制。

**建议前缀**：`dlt645/{gateway_id}`，其中 `gateway_id` 为配置项（主机名、资产编号或人工指定字符串，需唯一且合法字符）。

| 场景 | Topic 示例 | 说明 |
|------|------------|------|
| 单点上报（推荐默认） | `dlt645/{gateway_id}/meter/{meter_name}/item/{item_key}` | `item_key` 可用配置中的 `name` 或 `di` 的十六进制，避免中文与空格问题。 |
| 按表聚合（可选） | `dlt645/{gateway_id}/meter/{meter_name}/readings` | 每条消息为 JSON 数组或多字段对象，减少 Topic 数量。 |
| 状态/心跳（可选） | `dlt645/{gateway_id}/status/online` | Retain=true，LWT 与上线遗嘱配合。 |

**命名约束**：`meter_name`、`item_key` 在配置侧建议 ASCII（如 `meter-1`、`FwdActE`），若使用中文需在实现层做 **URL 安全编码或哈希**，避免 Broker 与下游解析问题。

---

## 4. Payload 规划（JSON）

首版推荐 **UTF-8 JSON**，便于云平台与调试。

**单条读数上报（成功）示例**：

```json
{
  "schema_version": 1,
  "gateway_id": "gw-001",
  "meter": "meter-1",
  "meter_address": "000000000005",
  "item": "FwdActE",
  "di": "00010000",
  "success": true,
  "value": 12345.67,
  "unit": "kWh",
  "ts_ms": 1716189048123,
  "timestamp": "2024-05-20T12:30:48.123Z"
}
```

**失败示例**：

```json
{
  "schema_version": 1,
  "gateway_id": "gw-001",
  "meter": "meter-2",
  "meter_address": "000000000002",
  "item": "curU",
  "di": "02010100",
  "success": false,
  "error_code": 2,
  "error_name": "接收超时",
  "ts_ms": 1716189048123,
  "timestamp": "2024-05-20T12:30:48.123Z"
}
```

- `ts_ms`：Unix 毫秒时间戳（整型）。
- `timestamp`：与 `ts_ms` 同一时刻的 **UTC** ISO8601 字符串，格式 `YYYY-MM-DDTHH:MM:SS.mmmZ`（毫秒三位）。

**按表聚合上报**（`.../meter/{meter}/readings`）：根对象同样包含 `ts_ms` 与 `timestamp`（整包生成时刻）；`items[]` 内各条仍可通过单点 topic 缓存查询到带时间戳的单条 JSON。

字段与现有 `MeterReader::ErrorCode`、`ResultReporter` 中错误名映射表保持一致，便于运维对照。

---

## 5. 连接、QoS 与安全

| 项 | 建议 |
|----|------|
| 协议 | MQTT 3.1.1；若平台要求再考虑 MQTT 5。 |
| QoS | 读数上报默认 **QoS 1**（至少一次）；状态类可 QoS 0。 |
| Retain | 读数一般 **Retain=false**；在线状态 Topic 可用 Retain=true。 |
| TLS | 生产环境 **MQTTS（TLS）**；根证书可配置路径或内嵌指纹校验策略（按平台要求）。 |
| 认证 | 用户名密码，或 **ClientId + 证书**（双向 TLS）；ClientId 建议含 `gateway_id`。 |
| 遗嘱 LWT | 可选：`dlt645/{gateway_id}/status/online` payload `offline`，与开机首包 `online` 配对。 |

---

## 6. 配置项草案（YAML）

在现有 `collector.yaml` 旁新增独立文件 **`mqtt.yaml`** 或在同一文件增加 `mqtt:` 根节点（二选一，实现阶段再定）。草案字段如下：

```yaml
mqtt:
  enabled: false
  gateway_id: "gw-openwrt-01"
  broker: "mqtt.example.com"
  port: 8883
  tls: true
  ca_file: "/etc/ssl/certs/ca.crt"
  username: "device001"
  password: "******"          # 生产环境建议环境变量或独立密钥文件，不入库明文
  client_id: "dlt645-gw-01"   # 可选，默认由 gateway_id 派生
  topic_prefix: "dlt645"      # 可选，默认 dlt645
  qos: 1
  keepalive_sec: 60
```

**与采集配置的关系**：`gateway_id`、`meter` 名称等仅用于上报标识，不改变 DL/T 645 通信参数。

### 6.1 MQTT 调试（Mosquitto）

本项目实现了一个简单的 **查询指令订阅**：网关订阅 `dlt645/{gateway_id}/cmd/query`，收到 **内联 JSON**、**本机 `.json` 文件路径**（读取文件后再解析）后，从内存缓存中取最近一次该表该 DI 的上报码，并回发。

#### 6.1.1 Topic（按默认/示例配置）

- **下发查询指令**：`{topic_prefix}/{gateway_id}/cmd/query`
- **命中缓存回包**：`{topic_prefix}/{gateway_id}/meter/{meter}/item/{di}`
- **未命中缓存回包**：`{topic_prefix}/{gateway_id}/cmd/query/resp`（payload `message: "no_cache"`）

例如你的配置：

- `topic_prefix: "dlt645"`
- `gateway_id: "gw-openwrt-01"`

则下发 topic 为：`dlt645/gw-openwrt-01/cmd/query`

#### 6.1.2 查询指令 JSON（payload）

保存为 `mqtt_query.json`：

```json
{"meter":"meter-1","di":"00010000"}
```

字段说明：

- `meter`：电表配置名（`meters[].name`），例如 `meter-1`
- `di`：数据标识（8 位十六进制字符串），例如 `00010000`

另外也支持 **payload 为网关本机 JSON 文件路径**（整段 payload 或首行），路径需以 `.json` 结尾（大小写不敏感），程序会读取文件内容再解析 `meter`/`di` 并做同样的缓存查询响应。出于安全考虑，路径中 **不得包含 `..`**；单文件读取上限 **64KB**。格式化 JSON（`"meter" : "meter-1"` 这类带空格）同样支持。

> 与 `mosquitto_pub -f mqtt_query.json` 的区别：`-f` 会把 **文件内容** 作为 MQTT payload（通常以 `{` 开头）；而 `-m '/path/to/mqtt_query.json'` 时 payload 是 **路径字符串**，由网关在本地打开该文件。

#### 6.1.3 使用 `mosquitto_pub` 下发查询指令

注意：`-t` 是 **topic**，JSON 必须作为 **payload**（`-m` 或 `-f`），不要把 JSON 放到 `-t`。

直接发送（`-m`）：

```bash
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -t "dlt645/gw-openwrt-01/cmd/query" \
  -m '{"meter":"meter-1","di":"00010000"}'
```

从文件发送（`-f`）：

```bash
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -t "dlt645/gw-openwrt-01/cmd/query" \
  -f mqtt_query.json
```

由网关在本地 **按路径读取** JSON 文件（payload 为路径字符串，与 `-f` 不同）：

```bash
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -t "dlt645/gw-openwrt-01/cmd/query" \
  -m '/etc/mqtt_query.json'
```

#### 6.1.4 使用 `mosquitto_sub` 观察回包

订阅该网关下所有 topic：

```bash
mosquitto_sub -h 127.0.0.1 -p 1883 -v -t "dlt645/gw-openwrt-01/#"
```

---

## 7. 可靠性与性能

| 项 | 说明 |
|----|------|
| 异步发送 | 发布在独立线程或 **非阻塞队列 + 后台线程**，避免阻塞 `Scheduler` 串口轮询。 |
| 断线重连 | 指数退避重连；重连期间读数可 **丢弃或落盘队列**（首版可丢弃并打日志；二期再做轻量持久化队列）。 |
| 背压 | 队列长度上限 + 丢弃最旧或最新策略需可配置，防止内存爆。 |
| 频率 | 与 `poll_interval_seconds` 及表项数量乘积相关；大表项时需评估 Broker 与带宽。 |

---

## 8. 依赖与 OpenWrt 选型（实现阶段）

| 方案 | 优点 | 缺点 |
|------|------|------|
| **Paho MQTT C/C++** 静态链接 | 可控、无外部进程 | 交叉编译与 TLS 需对齐工具链 |
| **libmosquitto** | 生态成熟 | 同样需处理 musl/TLS |
| **mosquitto_pub 子进程** | 集成快 | 进程开销大、难做精细重连与队列，不推荐长期方案 |

交叉编译目标为 **aarch64-openwrt-linux-musl** 时，须在规划阶段锁定：**TLS 由谁提供（mbedtls/openssl）**、是否与现有 `FetchContent` 依赖并存。

---

## 9. 分阶段实施建议

| 阶段 | 内容 | 验收 |
|------|------|------|
| P1 | 本文档评审定稿；冻结 Topic/Payload 与错误码字段 | 评审通过 |
| P2 | 增加 `mqtt` 配置解析与校验单元测试；`enabled: false` 默认无行为 | 测试通过 |
| P3 | 实现最小 MQTT 发布客户端（连接、发布、断线重连）；对接 `Scheduler` 回调 | 本地 Mosquitto 订阅可见消息 |
| P4 | TLS、遗嘱、队列与指标日志；网关实机压测 | 实机 24h 稳定 |

---

## 10. 文档维护

- 与 [project.md](project.md) 数据模型、部署章节交叉引用：对外接口在「日志」之外增加「MQTT 上报」。
- 实现完成后，将本规划中的 **Topic/Payload 终稿** 同步回 [design.md](design.md) 应用层小节，并补充 `CMake` 依赖说明到 [build.md](build.md)。
