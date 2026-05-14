#include <gtest/gtest.h>

#include "protocol/frame.h"

using namespace dlt645;

class FrameTest : public ::testing::Test {};

// TODO: 正常应答帧 is_error_response 返回 false
// TODO: 异常应答帧 is_error_response 返回 true
// TODO: 异常应答帧 error_code 返回正确错误码
