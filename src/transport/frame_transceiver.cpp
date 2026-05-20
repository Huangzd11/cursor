#include "transport/frame_transceiver.h"

#include <algorithm>
#include <chrono>

#include "protocol/frame_codec.h"

namespace dlt645 {

namespace {

constexpr size_t kReadChunkSize = 256;
constexpr size_t kMaxRecvBufferSize = 1024;

void trim_to_next_frame_start(std::vector<uint8_t>& buffer) {
    if (buffer.size() <= 1) {
        return;
    }
    const auto it =
        std::find(buffer.begin() + 1, buffer.end(), FRAME_START);
    if (it != buffer.end()) {
        buffer.erase(buffer.begin(), it);
    } else {
        buffer.clear();
    }
}

}  // namespace

FrameTransceiver::FrameTransceiver(std::shared_ptr<IChannel> channel)
    : channel_(std::move(channel)) {}

bool FrameTransceiver::send_frame(const Frame& frame) {
    recv_buffer_.clear();
    const auto bytes = FrameCodec::encode(frame);
    return channel_->send(bytes);
}

std::optional<Frame> FrameTransceiver::receive_frame(int timeout_ms) {
    if (!channel_ || !channel_->is_open()) {
        return std::nullopt;
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      deadline - std::chrono::steady_clock::now())
                                      .count();

        auto chunk = channel_->receive(
            kReadChunkSize, static_cast<int>(std::max<int64_t>(remaining_ms, 0)));
        if (!chunk.empty()) {
            recv_buffer_.insert(recv_buffer_.end(), chunk.begin(), chunk.end());

            if (auto decoded = FrameCodec::decode(recv_buffer_)) {
                const size_t consumed = decoded->bytes_consumed;
                if (consumed <= recv_buffer_.size()) {
                    recv_buffer_.erase(recv_buffer_.begin(),
                                       recv_buffer_.begin() +
                                           static_cast<std::ptrdiff_t>(consumed));
                } else {
                    recv_buffer_.clear();
                }
                return std::move(decoded->frame);
            }

            if (recv_buffer_.size() > kMaxRecvBufferSize) {
                trim_to_next_frame_start(recv_buffer_);
            }
        }
    }

    return std::nullopt;
}

}  // namespace dlt645
