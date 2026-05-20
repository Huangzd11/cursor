#include "app/mqtt_query_payload.h"

#include <cctype>
#include <cstddef>
#include <fstream>
#include <spdlog/spdlog.h>

namespace dlt645 {
namespace {

// 查询指令 JSON 文件大小上限（防止异常大文件占用内存）
constexpr std::size_t kMaxQueryFileBytes = 65536;

void skip_ws(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
}

std::string trim_copy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

bool ends_with_json_ci(const std::string& p) {
    if (p.size() < 5) {
        return false;
    }
    static const char suf[] = ".json";
    for (std::size_t i = 0; i < 5; ++i) {
        if (std::tolower(static_cast<unsigned char>(p[p.size() - 5 + i])) !=
            static_cast<unsigned char>(suf[i])) {
            return false;
        }
    }
    return true;
}

bool contains_parent_dir(const std::string& p) {
    return p.find("..") != std::string::npos;
}

std::optional<std::string> read_file_limited(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SPDLOG_WARN("MQTT 查询指令：无法打开 JSON 文件 {}", path);
        return std::nullopt;
    }
    in.seekg(0, std::ios::end);
    const auto sz64 = in.tellg();
    if (sz64 < 0) {
        SPDLOG_WARN("MQTT 查询指令：无法获取文件大小 {}", path);
        return std::nullopt;
    }
    const auto sz = static_cast<std::size_t>(sz64);
    if (sz > kMaxQueryFileBytes) {
        SPDLOG_WARN("MQTT 查询指令：JSON 文件过大 (>{}) {}", kMaxQueryFileBytes, path);
        return std::nullopt;
    }
    in.seekg(0, std::ios::beg);
    std::string buf(sz, '\0');
    if (sz > 0) {
        in.read(buf.data(), static_cast<std::streamsize>(sz));
        if (!in) {
            SPDLOG_WARN("MQTT 查询指令：读取 JSON 文件失败 {}", path);
            return std::nullopt;
        }
    }
    return buf;
}

}  // namespace

std::optional<std::string> mqtt_query_json_get_string(const std::string& json, const char* key) {
    const std::string quoted = std::string("\"") + key + "\"";
    std::size_t pos = 0;
    while (pos < json.size()) {
        pos = json.find(quoted, pos);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        std::size_t i = pos + quoted.size();
        skip_ws(json, i);
        if (i >= json.size() || json[i] != ':') {
            pos += quoted.size();
            continue;
        }
        ++i;
        skip_ws(json, i);
        if (i >= json.size() || json[i] != '"') {
            return std::nullopt;
        }
        ++i;
        std::string out;
        while (i < json.size()) {
            const char c = json[i++];
            if (c == '"') {
                return out;
            }
            if (c == '\\' && i < json.size()) {
                const char e = json[i++];
                switch (e) {
                    case '"':
                    case '\\':
                    case '/':
                        out += e;
                        break;
                    case 'b':
                        out += '\b';
                        break;
                    case 'f':
                        out += '\f';
                        break;
                    case 'n':
                        out += '\n';
                        break;
                    case 'r':
                        out += '\r';
                        break;
                    case 't':
                        out += '\t';
                        break;
                    case 'u':
                        for (int k = 0; k < 4 && i < json.size(); ++k) {
                            ++i;
                        }
                        break;
                    default:
                        out += e;
                        break;
                }
            } else {
                out += c;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> resolve_mqtt_query_json_payload(const std::string& raw_payload) {
    std::string s = trim_copy(raw_payload);
    if (s.empty()) {
        return std::nullopt;
    }
    // UTF-8 BOM
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
        s = trim_copy(s);
    }
    if (!s.empty() && s.front() == '{') {
        return s;
    }

    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = trim_copy(s.substr(1, s.size() - 2));
    }

    const auto nl = s.find_first_of("\r\n");
    if (nl != std::string::npos) {
        s = trim_copy(s.substr(0, nl));
    }

    if (s.empty() || !ends_with_json_ci(s)) {
        return std::nullopt;
    }
    if (contains_parent_dir(s)) {
        SPDLOG_WARN("MQTT 查询指令：拒绝包含 .. 的路径 {}", s);
        return std::nullopt;
    }

    return read_file_limited(s);
}

}  // namespace dlt645
