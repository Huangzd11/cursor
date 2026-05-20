#include <gtest/gtest.h>

#include "app/config.h"

using namespace dlt645;

class ConfigTest : public ::testing::Test {};

TEST(ConfigTest, ValidateRequiresEnabledMeterWithSerial) {
    AppConfig c;
    MeterConfig m;
    m.name = "m1";
    m.enabled = true;
    m.serial.device = "";
    c.meters.push_back(m);

    DataItem di;
    di.enabled = true;
    c.data_items.push_back(di);
    c.poll_interval_seconds = 60;

    auto err = Config::validate(c);
    ASSERT_FALSE(err.empty());
}

TEST(ConfigTest, ValidateDisabledMeterAllowsEmptySerial) {
    AppConfig c;
    MeterConfig m;
    m.name = "m1";
    m.enabled = false;
    m.serial.device = "";
    c.meters.push_back(m);

    MeterConfig m2;
    m2.name = "m2";
    m2.enabled = true;
    m2.serial.device = "/dev/ttyUSB0";
    c.meters.push_back(m2);

    DataItem di;
    di.enabled = true;
    c.data_items.push_back(di);
    c.poll_interval_seconds = 60;

    auto err = Config::validate(c);
    EXPECT_TRUE(err.empty());
}

TEST(ConfigTest, ValidateRequiresAtLeastOneEnabledDataItem) {
    AppConfig c;
    MeterConfig m;
    m.name = "m1";
    m.enabled = true;
    m.serial.device = "/dev/ttyUSB0";
    c.meters.push_back(m);

    DataItem di;
    di.enabled = false;
    c.data_items.push_back(di);
    c.poll_interval_seconds = 60;

    auto err = Config::validate(c);
    ASSERT_FALSE(err.empty());
}

TEST(ConfigTest, ValidateMqttEnabledRequiresBrokerAndGateway) {
    AppConfig c;
    MeterConfig m;
    m.name = "m1";
    m.enabled = true;
    m.serial.device = "/dev/ttyUSB0";
    c.meters.push_back(m);
    DataItem di;
    di.enabled = true;
    c.data_items.push_back(di);
    c.poll_interval_seconds = 60;

    c.mqtt.enabled = true;
    c.mqtt.tls = false;
    c.mqtt.gateway_id = "";
    c.mqtt.broker = "broker.local";
    ASSERT_FALSE(Config::validate(c).empty());

    c.mqtt.gateway_id = "gw1";
    c.mqtt.broker = "";
    ASSERT_FALSE(Config::validate(c).empty());
}

TEST(ConfigTest, ValidateMqttTlsRejectedForNow) {
    AppConfig c;
    MeterConfig m;
    m.name = "m1";
    m.enabled = true;
    m.serial.device = "/dev/ttyUSB0";
    c.meters.push_back(m);
    DataItem di;
    di.enabled = true;
    c.data_items.push_back(di);
    c.poll_interval_seconds = 60;
    c.mqtt.enabled = true;
    c.mqtt.gateway_id = "gw1";
    c.mqtt.broker = "b";
    c.mqtt.tls = true;
    ASSERT_FALSE(Config::validate(c).empty());
}
