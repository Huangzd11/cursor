#pragma once

#include <string>

#include "transport/ichannel.h"

namespace dlt645 {

// Linux串口实现，基于termios API
class SerialPort : public IChannel {
public:
    struct Config {
        std::string device;     // 设备路径，如 "/dev/ttyUSB0"
        int baudrate = 2400;    // 波特率
        int databits = 8;       // 数据位
        int stopbits = 1;       // 停止位
        char parity = 'E';      // 校验：'N'无/'E'偶/'O'奇
    };

    explicit SerialPort(const Config& config);
    ~SerialPort() override;

    bool open() override;
    void close() override;
    bool send(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receive(size_t max_bytes, int timeout_ms) override;
    bool is_open() const override;

private:
    Config config_;
    int fd_ = -1;
};

}  // namespace dlt645
