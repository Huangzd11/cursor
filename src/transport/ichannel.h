#pragma once

#include <cstdint>
#include <vector>

namespace dlt645 {

// 通信通道抽象接口，隔离底层通信方式，便于测试时注入Mock
class IChannel {
public:
    virtual ~IChannel() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    // 接收数据，最多等待 timeout_ms 毫秒，返回实际收到的字节
    virtual std::vector<uint8_t> receive(size_t max_bytes, int timeout_ms) = 0;

    virtual bool is_open() const = 0;
};

}  // namespace dlt645
