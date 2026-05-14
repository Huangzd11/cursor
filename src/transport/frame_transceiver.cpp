#include "transport/frame_transceiver.h"
#include "protocol/frame_codec.h"

namespace dlt645 {

FrameTransceiver::FrameTransceiver(std::shared_ptr<IChannel> channel)
    : channel_(std::move(channel)) {}

bool FrameTransceiver::send_frame(const Frame& frame) {
    auto bytes = FrameCodec::encode(frame);
    return channel_->send(bytes);
}

std::optional<Frame> FrameTransceiver::receive_frame(int timeout_ms) {
    // TODO: 从channel循环读取字节，拼接到recv_buffer_，
    //       尝试用FrameCodec::decode解析完整帧，处理超时
    return std::nullopt;
}

}  // namespace dlt645
