#include "transport/serial_port.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace dlt645 {

namespace {

speed_t baud_to_speed(int baud) {
    switch (baud) {
        case 1200:
            return B1200;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        default:
            return B0;
    }
}

}  // namespace

SerialPort::SerialPort(const Config& config)
    : config_(config) {}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open() {
    close();

    const speed_t speed = baud_to_speed(config_.baudrate);
    if (speed == B0) {
        SPDLOG_ERROR("不支持的波特率: {}", config_.baudrate);
        return false;
    }

    fd_ = ::open(config_.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        SPDLOG_ERROR("打开串口失败: {} ({})", config_.device, std::strerror(errno));
        return false;
    }

    struct termios tty {};
    if (tcgetattr(fd_, &tty) != 0) {
        SPDLOG_ERROR("tcgetattr 失败: {} ({})", config_.device, std::strerror(errno));
        close();
        return false;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag &= ~CSIZE;
    switch (config_.databits) {
        case 8:
            tty.c_cflag |= CS8;
            break;
        case 7:
            tty.c_cflag |= CS7;
            break;
        default:
            SPDLOG_ERROR("不支持的数据位: {}", config_.databits);
            close();
            return false;
    }

    tty.c_cflag &= ~(PARENB | PARODD);
    if (config_.parity == 'E' || config_.parity == 'e') {
        tty.c_cflag |= PARENB;
    } else if (config_.parity == 'O' || config_.parity == 'o') {
        tty.c_cflag |= PARENB | PARODD;
    } else if (config_.parity != 'N' && config_.parity != 'n') {
        SPDLOG_ERROR("不支持的校验位: {}", config_.parity);
        close();
        return false;
    }

    if (config_.stopbits == 2) {
        tty.c_cflag |= CSTOPB;
    } else if (config_.stopbits != 1) {
        SPDLOG_ERROR("不支持的停止位: {}", config_.stopbits);
        close();
        return false;
    }

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                     IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        SPDLOG_ERROR("tcsetattr 失败: {} ({})", config_.device, std::strerror(errno));
        close();
        return false;
    }

    tcflush(fd_, TCIOFLUSH);
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::send(const std::vector<uint8_t>& data) {
    if (fd_ < 0 || data.empty()) {
        return false;
    }

    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t n =
            ::write(fd_, data.data() + offset, data.size() - offset);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            SPDLOG_ERROR("串口写入失败: {} ({})", config_.device, std::strerror(errno));
            return false;
        }
        offset += static_cast<size_t>(n);
    }
    return true;
}

std::vector<uint8_t> SerialPort::receive(size_t max_bytes, int timeout_ms) {
    std::vector<uint8_t> result;
    if (fd_ < 0 || max_bytes == 0) {
        return result;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);

    struct timeval tv {};
    struct timeval* tv_ptr = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    const int ready = select(fd_ + 1, &read_fds, nullptr, nullptr, tv_ptr);
    if (ready <= 0) {
        return result;
    }

    result.resize(max_bytes);
    const ssize_t n = ::read(fd_, result.data(), max_bytes);
    if (n <= 0) {
        result.clear();
        return result;
    }
    result.resize(static_cast<size_t>(n));
    return result;
}

bool SerialPort::is_open() const {
    return fd_ >= 0;
}

}  // namespace dlt645
