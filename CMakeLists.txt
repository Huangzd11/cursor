cmake_minimum_required(VERSION 3.14)

# 子工程（如 Paho）使用无 VERSION 的 project() 时，显式采用 NEW 策略可避免 CMP0048 开发者警告
if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()

project(dlt645_collector VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

# 第三方依赖通过 FetchContent 管理
include(FetchContent)

# 交叉编译时仅构建主程序，单元测试在宿主机本地运行
if(NOT CMAKE_CROSSCOMPILING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# Eclipse Paho MQTT C（主程序静态链接；首版仅 tcp 明文，TLS 见 mqtt 配置校验）
set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(PAHO_WITH_SSL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    paho_mqtt_c
    URL https://github.com/eclipse/paho.mqtt.c/archive/refs/tags/v1.3.13.tar.gz
)
FetchContent_MakeAvailable(paho_mqtt_c)

# 主程序库（供测试和主程序共用）
add_library(dlt645_lib STATIC
    # 协议层
    src/protocol/bcd_codec.cpp
    src/protocol/address.cpp
    src/protocol/frame.cpp
    src/protocol/frame_codec.cpp
    src/protocol/data_item.cpp
    # 传输层
    src/transport/serial_port.cpp
    src/transport/frame_transceiver.cpp
    # 应用层
    src/app/config.cpp
    src/app/logging.cpp
    src/app/meter_reader.cpp
    src/app/result_reporter.cpp
    src/app/scheduler.cpp
    src/app/mqtt_query_payload.cpp
)
target_include_directories(dlt645_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dlt645_lib PUBLIC spdlog::spdlog yaml-cpp::yaml-cpp)

# 主程序（MQTT 仅在此目标链接 Paho C）
add_executable(dlt645_collector
    src/main.cpp
    src/app/mqtt_reporter.cpp
)
target_link_libraries(dlt645_collector PRIVATE
    dlt645_lib
    paho-mqtt3c-static
    Threads::Threads
)

# 测试（仅宿主机本地构建）
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()
