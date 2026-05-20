#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#include "app/mqtt_query_payload.h"

namespace dlt645 {
namespace {

TEST(MqttQueryPayloadTest, JsonGetString_Compact) {
    const std::string j = R"({"meter":"meter-1","di":"00010000"})";
    EXPECT_EQ(mqtt_query_json_get_string(j, "meter").value_or(""), "meter-1");
    EXPECT_EQ(mqtt_query_json_get_string(j, "di").value_or(""), "00010000");
}

TEST(MqttQueryPayloadTest, JsonGetString_PrettyWithSpaces) {
    const std::string j = R"({
  "meter" : "meter-1" ,
  "di": "00010000"
})";
    EXPECT_EQ(mqtt_query_json_get_string(j, "meter").value_or(""), "meter-1");
    EXPECT_EQ(mqtt_query_json_get_string(j, "di").value_or(""), "00010000");
}

TEST(MqttQueryPayloadTest, ResolvePayload_JsonObject_ReturnsAsIs) {
    const std::string j = "  {\"meter\":\"a\",\"di\":\"b\"}  \n";
    const auto r = resolve_mqtt_query_json_payload(j);
    ASSERT_TRUE(r);
    EXPECT_NE(r->find("\"meter\""), std::string::npos);
}

TEST(MqttQueryPayloadTest, ResolvePayload_FilePath_ReadsContent) {
    char path[256];
    std::snprintf(path, sizeof(path), "/tmp/em645_mqtt_query_test_%d.json", static_cast<int>(getpid()));
    {
        std::ofstream out(path);
        out << R"({"meter":"m-x","di":"01020304"})";
    }
    const auto r = resolve_mqtt_query_json_payload(std::string(path));
    std::remove(path);
    ASSERT_TRUE(r);
    EXPECT_EQ(mqtt_query_json_get_string(*r, "meter").value_or(""), "m-x");
    EXPECT_EQ(mqtt_query_json_get_string(*r, "di").value_or(""), "01020304");
}

TEST(MqttQueryPayloadTest, ResolvePayload_FilePath_Quoted) {
    char path[256];
    std::snprintf(path, sizeof(path), "/tmp/em645_mqtt_query_test2_%d.json", static_cast<int>(getpid()));
    {
        std::ofstream out(path);
        out << "{\"meter\":\"a\",\"di\":\"b\"}";
    }
    const std::string payload = std::string("\"") + path + "\"";
    const auto r = resolve_mqtt_query_json_payload(payload);
    std::remove(path);
    ASSERT_TRUE(r);
    EXPECT_EQ(mqtt_query_json_get_string(*r, "meter").value_or(""), "a");
}

TEST(MqttQueryPayloadTest, ResolvePayload_RejectParentDir) {
    EXPECT_FALSE(resolve_mqtt_query_json_payload("/tmp/../etc/passwd.json"));
}

TEST(MqttQueryPayloadTest, ResolvePayload_MissingFile) {
    EXPECT_FALSE(resolve_mqtt_query_json_payload("/tmp/em645_nonexistent_xyz_12345.json"));
}

}  // namespace
}  // namespace dlt645
