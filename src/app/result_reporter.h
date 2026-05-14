#pragma once

#include <optional>
#include <string>

#include "app/meter_reader.h"

namespace dlt645 {

// 结果输出：将采集结果格式化并通过日志输出
class ResultReporter {
public:
    void report(const std::string& meter_name,
                const std::string& item_name,
                MeterReader::ErrorCode code,
                std::optional<MeterReader::ReadResult> result);
};

}  // namespace dlt645
