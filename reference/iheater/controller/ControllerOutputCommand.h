#pragma once

#include <stdint.h>

namespace iheaterlink {

enum class ControllerOutputMode : uint8_t {
    Off = 0,
    TargetTemperature = 1,
};

struct ControllerOutputCommand {
    ControllerOutputMode mode = ControllerOutputMode::Off;
    float targetTempC = 0.0f;
};

} // namespace iheaterlink
