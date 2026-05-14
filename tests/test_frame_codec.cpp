#include <gtest/gtest.h>

#include "protocol/frame_codec.h"

using namespace dlt645;

class FrameCodecTest : public ::testing::Test {};

// TODO: 编码请求帧
// TODO: 解码正常应答帧
// TODO: 编码-解码往返一致性
// TODO: 校验和错误返回nullopt
// TODO: 帧过短返回nullopt
// TODO: 跳过前导符
