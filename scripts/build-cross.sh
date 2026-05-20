#!/usr/bin/env bash
# 交叉编译 dlt645_collector，产物部署到 OpenWrt 网关运行
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT}/build-cross"
TOOLCHAIN_TAR="${ROOT}/openwrt-gcc-8.3.0.tar.gz"
TOOLCHAIN_DIR="${ROOT}/openwrt-gcc-8.3.0"

if [[ ! -d "${TOOLCHAIN_DIR}/bin" ]]; then
    echo "解压交叉编译工具链..."
    tar -xzf "${TOOLCHAIN_TAR}" -C "${ROOT}"
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${ROOT}/cmake/openwrt-aarch64.cmake" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ""
echo "交叉编译完成：${BUILD_DIR}/dlt645_collector"
file "${BUILD_DIR}/dlt645_collector"
