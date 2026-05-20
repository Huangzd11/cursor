# OpenWrt aarch64 交叉编译工具链（openwrt-gcc-8.3.0）
# 目标：aarch64-openwrt-linux-musl，运行于 OpenWrt 网关

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 工具链根目录（相对本文件：cmake/ -> 项目根/openwrt-gcc-8.3.0）
get_filename_component(TOOLCHAIN_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../openwrt-gcc-8.3.0"
    ABSOLUTE)

if(NOT EXISTS "${TOOLCHAIN_ROOT}/bin/aarch64-openwrt-linux-musl-gcc")
    message(FATAL_ERROR
        "未找到交叉编译器，请先解压工具链：\n"
        "  tar -xzf openwrt-gcc-8.3.0.tar.gz")
endif()

set(TOOLCHAIN_PREFIX aarch64-openwrt-linux-musl)
set(TOOLCHAIN_BIN "${TOOLCHAIN_ROOT}/bin")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}-g++")
set(CMAKE_AR           "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}-ar")
set(CMAKE_RANLIB       "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}-ranlib")
set(CMAKE_STRIP        "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}-strip")

set(CMAKE_SYSROOT "${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

# 抑制 OpenWrt 工具链关于 STAGING_DIR 的编译警告
set(ENV{STAGING_DIR} "${TOOLCHAIN_ROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
