# 构建与部署

## 环境说明

| 环境 | 用途 |
|------|------|
| 开发机（x86_64 Linux） | 编写代码、运行单元测试、交叉编译 |
| OpenWrt 网关（aarch64） | 运行 `dlt645_collector` 采集程序 |

交叉编译工具链：`openwrt-gcc-8.3.0.tar.gz`（OpenWrt GCC 8.3.0，目标 `aarch64-openwrt-linux-musl`）。

## 本地开发与测试（宿主机）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

单元测试仅在宿主机本地构建运行，不参与交叉编译。

## 交叉编译（网关产物）

首次需解压工具链（若尚未解压）：

```bash
tar -xzf openwrt-gcc-8.3.0.tar.gz
```

或使用脚本一键交叉编译：

```bash
./scripts/build-cross.sh
```

产物路径：`build-cross/dlt645_collector`。

手动方式：

```bash
cmake -S . -B build-cross \
    -DCMAKE_TOOLCHAIN_FILE=cmake/openwrt-aarch64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-cross -j$(nproc)
```

## 部署到网关

将可执行文件与配置文件拷贝到网关，例如：

```bash
scp build-cross/dlt645_collector config.yaml root@<网关IP>:/opt/dlt645/
```

在网关上运行：

```bash
./dlt645_collector config.yaml
```

串口设备路径以网关实际为准（如 `/dev/ttyS1`）。
