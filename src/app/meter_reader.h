#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "protocol/address.h"
#include "protocol/data_item.h"
#include "protocol/frame.h"
#include "transport/frame_transceiver.h"

namespace dlt645 {

// 读表器：封装单次读表操作
class MeterReader {
public:
    struct ReadResult {
        double value;
        std::string unit;
    };

    enum class ErrorCode {
        kSuccess,
        kSendFailed,
        kTimeout,
        kAddressMismatch,
        kErrorResponse,
        kDataMismatch,
        kDecodeFailed
    };

    explicit MeterReader(std::shared_ptr<FrameTransceiver> transceiver);

    // 读取指定电表的指定数据项（meter_name 用于日志，与 YAML 中 name 一致）
    std::pair<ErrorCode, std::optional<ReadResult>> read_data(
        std::string_view meter_name,
        const Address& address,
        const DataItem& item);

private:
    Frame build_request(const Address& address, const DataItem& item);

    ErrorCode validate_response(const Frame& response,
                                const Address& expected_addr,
                                const DataItem& expected_item);

    std::shared_ptr<FrameTransceiver> transceiver_;
};

}  // namespace dlt645
