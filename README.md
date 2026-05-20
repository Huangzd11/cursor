# DL/T 645-2007 电能表数据采集

基于 DL/T 645-2007 协议，通过串口轮询采集电能表数据。

## 构建

- **本地测试**：`cmake -S . -B build && cmake --build build && ctest --test-dir build`
- **交叉编译（OpenWrt 网关）**：`./scripts/build-cross.sh`

详见 [document/build.md](document/build.md)。技术难点与创新点见 [document/technical-highlights.md](document/technical-highlights.md)。

## 运行（网关）

```bash
./dlt645_collector config.yaml
```

- 项目说明：[document/project.md](document/project.md)（需求、架构、数据模型、开发计划、使用指南）
- 模块设计：[document/design.md](document/design.md)
- MQTT 上报规划：[document/mqtt-reporting-plan.md](document/mqtt-reporting-plan.md)
