cmake_minimum_required(VERSION 3.14)
project(dlt645_collector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 第三方依赖通过 FetchContent 管理
include(FetchContent)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

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

# 主程序库（供测试和主程序共用）
add_library(dlt645_lib STATIC
    src/protocol/frame.cpp
    src/protocol/data_item.cpp
    src/transport/serial_port.cpp
    src/app/meter_reader.cpp
    src/app/config.cpp
    src/app/scheduler.cpp
)
target_include_directories(dlt645_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dlt645_lib PUBLIC spdlog::spdlog yaml-cpp::yaml-cpp)

# 主程序
add_executable(dlt645_collector src/main.cpp)
target_link_libraries(dlt645_collector PRIVATE dlt645_lib)

# 测试
enable_testing()
add_subdirectory(tests)
