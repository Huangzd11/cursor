#include "app/meter_reader.h"

#include <spdlog/spdlog.h>

namespace dlt645 {

MeterReader::MeterReader(std::shared_ptr<FrameTransceiver> transceiver)
    : transceiver_(std::move(transceiver)) {}

std::pair<MeterReader::ErrorCode, std::optional<MeterReader::ReadResult>>
MeterReader::read_data(const Address& address, const DataItem& item) {
    // 构造请求帧
    auto request = build_request(address, item);

    // 发送
    if (!transceiver_->send_frame(request)) {
        return {ErrorCode::kSendFailed, std::nullopt};
    }

    // 接收应答
    auto response = transceiver_->receive_frame(3000);
    if (!response) {
        return {ErrorCode::kTimeout, std::nullopt};
    }

    // 校验应答
    auto err = validate_response(*response, address, item);
    if (err != ErrorCode::kSuccess) {
        return {err, std::nullopt};
    }

    // 提取数据（跳过前4字节DI）
    if (response->data.size() < static_cast<size_t>(4 + item.data_length)) {
        return {ErrorCode::kDecodeFailed, std::nullopt};
    }

    std::vector<uint8_t> value_bytes(
        response->data.begin() + 4,
        response->data.begin() + 4 + item.data_length);

    double value = item.decode_value(value_bytes);

    return {ErrorCode::kSuccess, ReadResult{value, item.unit}};
}

Frame MeterReader::build_request(const Address& address, const DataItem& item) {
    Frame frame;
    frame.address = address;
    frame.control = CTRL_READ_DATA;
    frame.data = item.encode_di();
    return frame;
}

MeterReader::ErrorCode MeterReader::validate_response(
    const Frame& response,
    const Address& expected_addr,
    const DataItem& expected_item) {

    if (response.address != expected_addr) {
        return ErrorCode::kAddressMismatch;
    }

    if (response.is_error_response()) {
        return ErrorCode::kErrorResponse;
    }

    if (response.control != CTRL_READ_DATA_OK) {
        return ErrorCode::kErrorResponse;
    }

    // 验证应答中的DI是否匹配
    if (response.data.size() < 4) {
        return ErrorCode::kDataMismatch;
    }
    auto expected_di = expected_item.encode_di();
    for (int i = 0; i < 4; ++i) {
        if (response.data[i] != expected_di[i]) {
            return ErrorCode::kDataMismatch;
        }
    }

    return ErrorCode::kSuccess;
}

}  // namespace dlt645
