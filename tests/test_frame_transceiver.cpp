#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "transport/frame_transceiver.h"
#include "transport/ichannel.h"

using namespace dlt645;

class MockChannel : public IChannel {
public:
    MOCK_METHOD(bool, open, (), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(bool, send, (const std::vector<uint8_t>&), (override));
    MOCK_METHOD(std::vector<uint8_t>, receive, (size_t, int), (override));
    MOCK_METHOD(bool, is_open, (), (const, override));
};

class FrameTransceiverTest : public ::testing::Test {
protected:
    std::shared_ptr<MockChannel> mock_channel_ = std::make_shared<MockChannel>();
};

// TODO: send_frame 调用 channel->send
// TODO: receive_frame 超时返回 nullopt
// TODO: receive_frame 收到完整帧正确解析
