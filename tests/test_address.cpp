#include <gtest/gtest.h>

#include "protocol/address.h"

using namespace dlt645;

class AddressTest : public ::testing::Test {};

TEST(AddressTest, FromString_ParsesLowByteFirst) {
    auto a = Address::from_string("000000000005");
    ASSERT_TRUE(a);
    const auto& b = a->bytes();
    EXPECT_EQ(b[0], 0x05);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 0x00);
    EXPECT_EQ(b[4], 0x00);
    EXPECT_EQ(b[5], 0x00);
}

TEST(AddressTest, FromString_AnotherMeter) {
    auto a = Address::from_string("000000000002");
    ASSERT_TRUE(a);
    EXPECT_EQ(a->bytes()[0], 0x02);
}

TEST(AddressTest, FromString_InvalidLength_ReturnsNullopt) {
    EXPECT_FALSE(Address::from_string("00000000005"));
    EXPECT_FALSE(Address::from_string("0000000000050"));
}

TEST(AddressTest, FromString_InvalidHex_ReturnsNullopt) {
    EXPECT_FALSE(Address::from_string("00000000000G"));
}

TEST(AddressTest, FromString_InvalidBcd_ReturnsNullopt) {
    // 0x1A 不是合法 BCD
    EXPECT_FALSE(Address::from_string("00000000001A"));
}

TEST(AddressTest, ToString_RoundTrip) {
    auto a = Address::from_string("000000000005");
    ASSERT_TRUE(a);
    EXPECT_EQ(a->to_string(), "000000000005");
}

TEST(AddressTest, IsBroadcast) {
    auto b = Address::from_string("999999999999");
    ASSERT_TRUE(b);
    EXPECT_TRUE(b->is_broadcast());

    auto n = Address::from_string("000000000005");
    ASSERT_TRUE(n);
    EXPECT_FALSE(n->is_broadcast());
}

TEST(AddressTest, Equality) {
    auto a = Address::from_string("000000000005");
    auto b = Address::from_string("000000000005");
    auto c = Address::from_string("000000000002");
    ASSERT_TRUE(a && b && c);
    EXPECT_EQ(*a, *b);
    EXPECT_NE(*a, *c);
}
