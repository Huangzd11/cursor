#pragma once

#include <optional>
#include <string>

namespace dlt645 {

// 从 cmd/query 原始 payload 得到待解析的 JSON 文本：
// - 若以 `{` 开头（可带首尾空白/BOM），视为内联 JSON；
// - 否则若为一行（或首行）以 .json 结尾的本地路径，则读取该文件内容（用于 payload 传文件路径）。
std::optional<std::string> resolve_mqtt_query_json_payload(const std::string& raw_payload);

// 从 JSON 文本中提取字符串字段（支持 key/value 周围空白；value 支持常见转义）。
std::optional<std::string> mqtt_query_json_get_string(const std::string& json, const char* key);

}  // namespace dlt645
