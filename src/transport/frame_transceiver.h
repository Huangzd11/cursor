#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "protocol/frame.h"
#include "transport/ichannel.h"

namespace dlt645 {

// 帧收发器：在IChannel之上封装帧级别的发送/接收逻辑
class FrameTransceiver {
public:
    explicit FrameTransceiver(std::shared_ptr<IChannel> channel);

    // 发送一帧（自动编码为字节流）
    bool send_frame(const Frame& frame);

    // 接收一帧完整应答，timeout_ms为整体超时
    std::optional<Frame> receive_frame(int timeout_ms);

private:
    std::shared_ptr<IChannel> channel_;
    std::vector<uint8_t> recv_buffer_;
};

}  // namespace dlt645
