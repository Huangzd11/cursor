#include <gtest/gtest.h>

#include "protocol/bcd_codec.h"

using namespace dlt645;

class BcdCodecTest : public ::testing::Test {};

TEST(BcdCodecTest, Add33) {
    std::vector<uint8_t> in{0x00, 0x01, 0xFF};
    auto out = BcdCodec::add33(in);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 0x33);
    EXPECT_EQ(out[1], 0x34);
    EXPECT_EQ(out[2], 0x32);  // 0xFF + 0x33 溢出为 0x32
}

TEST(BcdCodecTest, Sub33) {
    std::vector<uint8_t> in{0x33, 0x34, 0x32};
    auto out = BcdCodec::sub33(in);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 0x01);
    EXPECT_EQ(out[2], 0xFF);
}

TEST(BcdCodecTest, Add33Sub33_RoundTrip) {
    std::vector<uint8_t> orig{0x00, 0x01, 0x00, 0x00};
    auto mid = BcdCodec::add33(orig);
    auto back = BcdCodec::sub33(mid);
    EXPECT_EQ(back, orig);
}

TEST(BcdCodecTest, DecodeFloat_LowByteFirst) {
    // 123456.78，小数 2 位：BCD 低位在前 78 56 34 12
    uint8_t raw[] = {0x78, 0x56, 0x34, 0x12};
    double v = BcdCodec::decode_float(raw, 4, 2);
    EXPECT_DOUBLE_EQ(v, 123456.78);
}
