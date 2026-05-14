#include "protocol/frame.h"

namespace dlt645 {

bool Frame::is_error_response() const {
    return control == CTRL_READ_DATA_ERR;
}

uint8_t Frame::error_code() const {
    if (!data.empty()) {
        return data[0];
    }
    return 0;
}

}  // namespace dlt645
