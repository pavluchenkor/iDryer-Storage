#include "iheater/LinkCommandSink.h"

#include <Arduino.h>
#include <hal/hal_types.h>

namespace iheaterlink {

void LinkCommandSink::sendCommand(const DryerUart::CommandPayload& payload, bool /*ackRequired*/)
{
    using CC = DryerUart::CommandCode;

    if (!output_) {
        HAL_LOG_WARN("LINKSINK", "No controller output, cmd=0x%02X ignored",
                     static_cast<unsigned>(payload.command));
        return;
    }

    switch (payload.command) {
    case CC::Start: {
        const float targetTempC = static_cast<float>(payload.arg0) / 10.0f;

        ControllerOutputCommand cmd{};
        cmd.mode = ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = targetTempC;
        output_->apply(cmd);

        // arg1 = duration в минутах. 0 = бесконечно (таймер выключен).
        if (payload.arg1 > 0) {
            deadlineMs_ = millis() + static_cast<uint32_t>(payload.arg1) * 60UL * 1000UL;
            HAL_LOG_INFO("LINKSINK", "Start applied: target=%.1fC duration=%umin",
                         targetTempC, (unsigned)payload.arg1);
        } else {
            deadlineMs_ = 0;
            HAL_LOG_INFO("LINKSINK", "Start applied: target=%.1fC duration=infinite",
                         targetTempC);
        }
        break;
    }

    case CC::Stop: {
        ControllerOutputCommand cmd{};
        cmd.mode = ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        output_->apply(cmd);
        deadlineMs_ = 0;

        HAL_LOG_INFO("LINKSINK", "Stop applied: output = Off");
        break;
    }

    default:
        HAL_LOG_DEBUG("LINKSINK", "Cmd 0x%02X: not applicable on IHeaterLink",
                      static_cast<unsigned>(payload.command));
        break;
    }
}

// Обязательный метод ICommandSink — на iHeater не применим.
void LinkCommandSink::sendProfileCommand(const DryerUart::ProfilePayload&, bool){
    HAL_LOG_DEBUG("LINKSINK", "Profile cmd: not applicable on IHeaterLink");
}

// Обязательный метод ICommandSink — на iHeater не применим.
void LinkCommandSink::sendConfigPushChunk(const DryerUart::ConfigChunkPayload&, uint8_t, uint8_t)
{
    HAL_LOG_DEBUG("LINKSINK", "Config push chunk: not applicable on IHeaterLink");
}

void LinkCommandSink::tick()
{
    if (deadlineMs_ == 0 || output_ == nullptr) return;

    // Сравнение со знаком: корректно при wrap'е millis() (49.7 дней).
    if (static_cast<int32_t>(millis() - deadlineMs_) < 0) return;

    ControllerOutputCommand cmd{};
    cmd.mode = ControllerOutputMode::Off;
    cmd.targetTempC = 0.0f;
    output_->apply(cmd);
    deadlineMs_ = 0;

    HAL_LOG_INFO("LINKSINK", "Duration expired: output = Off");
}

} // namespace iheaterlink
