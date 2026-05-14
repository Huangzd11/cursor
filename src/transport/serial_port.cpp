#include "transport/serial_port.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>

namespace dlt645 {

SerialPort::SerialPort(const Config& config)
    : config_(config) {}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open() {
    // TODO: 实现串口打开和termios配置
    return false;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::send(const std::vector<uint8_t>& data) {
    // TODO: 实现串口数据发送
    return false;
}

std::vector<uint8_t> SerialPort::receive(size_t max_bytes, int timeout_ms) {
    // TODO: 实现串口数据接收（含超时处理）
    return {};
}

bool SerialPort::is_open() const {
    return fd_ >= 0;
}

}  // namespace dlt645
