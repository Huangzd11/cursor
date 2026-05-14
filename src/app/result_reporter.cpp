#include "app/result_reporter.h"

#include <spdlog/spdlog.h>

namespace dlt645 {

void ResultReporter::report(const std::string& meter_name,
                            const std::string& item_name,
                            MeterReader::ErrorCode code,
                            std::optional<MeterReader::ReadResult> result) {
    if (code == MeterReader::ErrorCode::kSuccess && result) {
        spdlog::info("[{}] {} = {} {}",
                     meter_name, item_name, result->value, result->unit);
    } else {
        spdlog::warn("[{}] {} 采集失败, 错误码: {}",
                     meter_name, item_name, static_cast<int>(code));
    }
}

}  // namespace dlt645
