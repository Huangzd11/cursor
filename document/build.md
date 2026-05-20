# 构建与部署

## 环境说明


| 环境                  | 用途                         |
| ------------------- | -------------------------- |
| 开发机（x86_64 Linux）   | 编写代码、运行单元测试、交叉编译           |
| OpenWrt 网关（aarch64） | 运行 `dlt645_collector` 采集程序 |


交叉编译工具链：`openwrt-gcc-8.3.0.tar.gz`（OpenWrt GCC 8.3.0，目标 `aarch64-openwrt-linux-musl`）。

## 本地开发与测试（宿主机）

首次 `cmake` 会从 GitHub **下载 Paho MQTT C 源码包**（`FetchContent`），需能访问外网。工程已启用 **C 语言**（与 Paho 子工程一致）。

```bash
# 可选 -Wno-dev：抑制 Paho 子工程 CMP0048 等开发者警告
cmake -Wno-dev -S . -B build -DCMAKE_BUILD_TYPE=Debug
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

手动方式（须先导出 `STAGING_DIR`，否则 gcc 会打印警告）：

```bash
export STAGING_DIR="$(pwd)/openwrt-gcc-8.3.0"
# 可选 -Wno-dev：抑制 Paho 子工程 CMP0048 等开发者警告（与 scripts/build-cross.sh 一致）
cmake -Wno-dev -S . -B build-cross \
    -DCMAKE_TOOLCHAIN_FILE=cmake/openwrt-aarch64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-cross -j$(nproc)
```

## 部署到网关

将可执行文件与配置文件拷贝到网关，例如：

```bash
scp -P 35520 build-cross/dlt645_collector   admin@172.21.9.146:/userdata/admin/huangzd/em645
```

在网关上运行：

```bash
./dlt645_collector config/collector.yaml
```

运行后会在当前工作目录下自动创建 `log/`，并按日期写入 `log/dlt645_YYYY-MM-DD.log`（与终端输出格式一致）；跨日零点由 spdlog 自动切换新文件。

串口设备路径以网关实际为准（如 `/dev/ttyS1`）。