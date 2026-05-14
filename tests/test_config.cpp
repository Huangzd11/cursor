#include <gtest/gtest.h>

#include "app/config.h"

using namespace dlt645;

class ConfigTest : public ::testing::Test {};

// TODO: 加载有效YAML配置
// TODO: 无效文件路径返回nullopt
// TODO: validate 空电表列表返回错误
// TODO: validate 正常配置通过
