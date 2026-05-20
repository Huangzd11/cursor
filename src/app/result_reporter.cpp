#include "app/result_reporter.h"

#include <spdlog/spdlog.h>

namespace dlt645 {

namespace {

const char* error_code_name(MeterReader::ErrorCode code) {
    switch (code) {
        case MeterReader::ErrorCode::kSuccess:
            return "成功";
        case MeterReader::ErrorCode::kSendFailed:
            return "发送失败";
        case MeterReader::ErrorCode::kTimeout:
            return "接收超时";
        case MeterReader::ErrorCode::kAddressMismatch:
            return "地址不匹配";
        case MeterReader::ErrorCode::kErrorResponse:
            return "电表异常应答";
        case MeterReader::ErrorCode::kDataMismatch:
            return "数据标识不匹配";
        case MeterReader::ErrorCode::kDecodeFailed:
            return "数据解码失败";
        default:
            return "未知错误";
    }
}

}  // namespace

void ResultReporter::report(const std::string& meter_name,
                            const std::string& item_name,
                            MeterReader::ErrorCode code,
                            std::optional<MeterReader::ReadResult> result) {
    if (code == MeterReader::ErrorCode::kSuccess && result) {
        SPDLOG_INFO("[{}] {} = {} {}",
                     meter_name, item_name, result->value, result->unit);
    } else {
        SPDLOG_WARN("[{}] {} 采集失败, 错误: {} ({})",
                     meter_name, item_name, error_code_name(code),
                     static_cast<int>(code));
    }
}

}  // namespace dlt645
