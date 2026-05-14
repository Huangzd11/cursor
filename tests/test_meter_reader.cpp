#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "app/meter_reader.h"

using namespace dlt645;

// TODO: 使用 Mock FrameTransceiver 测试读表流程
// TODO: 发送失败返回 kSendFailed
// TODO: 接收超时返回 kTimeout
// TODO: 地址不匹配返回 kAddressMismatch
// TODO: 异常应答返回 kErrorResponse
// TODO: 正常读表返回正确值
