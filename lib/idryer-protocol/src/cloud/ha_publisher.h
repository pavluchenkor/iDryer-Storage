/**
 * @file ha_publisher.h
 * @brief Публикация данных в Home Assistant с MQTT Discovery
 *
 * Поддерживает:
 * - Автоматическое создание сенсоров через Discovery
 * - Публикацию telemetry (температура, влажность, мощность)
 * - Публикацию status (режим работы)
 * - Публикацию events/alerts (ошибки, предупреждения)
 */

#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "../mqtt/ha_mqtt_client.h"
#include "../uart/uart_protocol.h"
#include <functional>

namespace idryer {
namespace ha {

/**
 * @brief Publisher для Home Assistant
 *
 * Создает устройство в HA с сенсорами для каждого юнита:
 * - sensor.idryer_u1_temperature
 * - sensor.idryer_u1_humidity
 * - sensor.idryer_u1_heater_power
 * - sensor.idryer_u1_mode
 * - binary_sensor.idryer_u1_fan
 * - sensor.idryer_alerts (последнее событие/ошибка)
 */
class HaPublisher {
public:
    explicit HaPublisher(HaMqttClient* mqtt);

    /**
     * @brief Публикует MQTT Discovery конфигурацию
     *
     * Создает устройство в HA и регистрирует все сенсоры.
     * Вызывается один раз при подключении к MQTT брокеру.
     *
     * @param deviceId Уникальный ID устройства (serialNumber)
     * @param unitsCount Количество юнитов (1-3)
     * @param hwVersion Версия hardware (например "v3.0")
     * @param fwVersion Версия firmware (например "1.2.3")
     * @return true если все конфиги опубликованы успешно
     */
    bool publishDiscovery(const char* deviceId,
                          uint8_t unitsCount,
                          const char* hwVersion = "unknown",
                          const char* fwVersion = "unknown",
                          int tempMin = 30,
                          int tempMax = 90,
                          int durationMax = 1440);

    /**
     * @brief Публикует телеметрию (температура, влажность, мощность)
     */
    bool publishTelemetry(const DryerUart::TelemetryPayload& data);

    /**
     * @brief Публикует статус (режим работы, таймеры)
     */
    bool publishStatus(const DryerUart::StatusPayload& data);

    /**
     * @brief Публикует вес филамента
     */
    bool publishWeights(const DryerUart::WeightsPayload& data);

    /**
     * @brief Публикует alert/event (ошибки, предупреждения)
     * @param unitId ID юнита (0-2) или 0xFF для общего
     * @param message Текст сообщения
     * @param severity "error", "warning", "info"
     */
    bool publishAlert(uint8_t unitId, const char* message, const char* severity = "error");

    /**
     * @brief Callback для входящих команд из HA
     * @param command "drying", "storage", "stop"
     * @param unitId "U1", "U2", ...
     * @param temperature целевая температура °C (актуально для drying/storage)
     * @param duration длительность в минутах (для drying)
     */
    using HaCommandCallback = std::function<void(const char* command, const char* unitId,
                                                  int temperature, int duration)>;
    void setCommandCallback(HaCommandCallback cb) { commandCallback_ = cb; }

    /**
     * @brief Разрешает повторную публикацию Discovery (например, после переподключения)
     */
    void resetDiscoveryPublished() { discoveryPublished_ = false; }

    bool isDiscoveryPublished() const { return discoveryPublished_; }

private:
    HaMqttClient* mqtt_;
    uint8_t unitsCount_ = 1;
    bool discoveryPublished_ = false;

    char deviceId_[32];
    char hwVersion_[16];
    char fwVersion_[16];
    int tempMin_ = 30;
    int tempMax_ = 90;
    int durationMax_ = 1440;

    // Буферы для формирования топиков и payload
    static constexpr size_t HA_TOPIC_BUF_SIZE = 128;
    static constexpr size_t HA_PAYLOAD_BUF_SIZE = 1024;

    char topicBuf_[HA_TOPIC_BUF_SIZE];
    char payloadBuf_[HA_PAYLOAD_BUF_SIZE];

    HaCommandCallback commandCallback_;

    // Хранение параметров сушки, заданных из HA (по юниту)
    static constexpr uint8_t MAX_UNITS = 4;
    int16_t targetTempC_[MAX_UNITS]   = {55, 55, 55, 55};  // °C
    uint16_t targetDurMin_[MAX_UNITS] = {240, 240, 240, 240}; // минуты

    // Вспомогательные методы для Discovery
    bool publishSelectDiscovery(uint8_t unitId);
    bool publishNumberDiscovery(uint8_t unitId, const char* name, const char* cmdSuffix,
                                const char* stateSuffix, int min, int max,
                                const char* unit, const char* icon);
    bool subscribeToCommands();
    void handleIncomingMessage(const char* topic, const char* payload);

    bool publishSensorDiscovery(uint8_t unitId, const char* sensorName,
                                const char* deviceClass, const char* unit,
                                const char* icon = nullptr);

    bool publishBinarySensorDiscovery(uint8_t unitId, const char* sensorName,
                                       const char* deviceClass = nullptr,
                                       const char* icon = nullptr);

    bool publishAlertDiscovery();

    // Формирование топиков
    void makeStateTopic(char* buf, size_t size, uint8_t unitId, const char* sensorName);
    void makeConfigTopic(char* buf, size_t size, const char* domain,
                         uint8_t unitId, const char* sensorName);

    // Формирование JSON для Discovery
    void makeDeviceJson(char* buf, size_t size);

    // Форматирование значений
    static void formatUnitId(char* buffer, size_t size, uint8_t unitId);
    static const char* modeToString(DryerUart::DryerMode mode);
};

} // namespace ha
} // namespace idryer

#endif // ESP32
