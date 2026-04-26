/**
 * @file telemetry_publisher.h
 * @brief Преобразование UART payload в MQTT JSON.
 */

#pragma once

#include "../uart/uart_protocol.h"
#include "../mqtt/mqtt_client.h"

namespace idryer {
namespace cloud {

/// Публикует `telemetry/status/weights/rfid/info/config`.
class TelemetryPublisher {
public:
    explicit TelemetryPublisher(MqttClient* mqtt);

    bool publishTelemetry(const DryerUart::TelemetryPayload& data);

    bool publishStatus(const DryerUart::StatusPayload& data);

    bool publishWeights(const DryerUart::WeightsPayload& data);

    bool publishRfid(const DryerUart::RfidPayload& data);

    /// Публикует 888 байт данных метки в топик rfid (base64, формат RfidDataPayload).
    bool publishRfidData(const uint8_t* data, size_t len, const DryerUart::RfidDataPayload& meta);

    /// Обычно публикуется один раз после выхода онлайн.
    /// @param deviceType DeviceType из HelloPayload (0 = Unknown → legacy, поле в JSON опускается).
    bool publishInfo(uint8_t unitsCount, const DryerUart::UnitConfig* units,
                     const char* hwVersion, const char* fwVersion, uint32_t workTime,
                     const char* mcuSerial = nullptr,
                     uint8_t deviceType = 0);

    /// Возвращает число отправленных MQTT сообщений (0 при ошибке).
    /// Большой JSON дробится на фрагменты `{tid, idx, total, last, d}`.
    uint16_t publishConfig(const char* json, uint16_t length);

    void setUnitsCount(uint8_t count) { unitsCount_ = count; }

    /// Разрешает повторную публикацию `info`.
    void resetInfoPublished() { infoPublished_ = false; }

    bool isInfoPublished() const { return infoPublished_; }

private:
    MqttClient* mqtt_;
    uint8_t unitsCount_ = 1;
    bool infoPublished_ = false;

    // Вспомогательные форматтеры идентификаторов.
    static void formatUnitId(char* buffer, size_t size, uint8_t unitId);
    static void formatSensorId(char* buffer, size_t size, uint8_t sensorId);
    static const char* modeToString(DryerUart::DryerMode mode);
    static const char* rfidEventToString(DryerUart::RfidEvent event);
};

} // namespace cloud
} // namespace idryer
