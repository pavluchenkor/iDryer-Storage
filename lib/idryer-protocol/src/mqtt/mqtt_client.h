/**
 * @file mqtt_client.h
 * @brief MQTT клиент устройства iDryer.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#if MQTT_USE_TLS
#include <WiFiClientSecure.h>
#else
#include <WiFi.h>
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "idryer_topics.h"
#include "../uart/uart_protocol.h"

#define IDRYER_MQTT_KEEPALIVE   60    // Секунд (переименовано чтобы не конфликтовать с PubSubClient)

// Максимальные размеры
#define MQTT_BUFFER_SIZE          16384  // Для больших JSON payload (16KB)
#define TOPIC_BUFFER_SIZE         128    // Для топиков
#define MQTT_CONFIG_CHUNK_SIZE    16000  // Размер данных в одном чанке (влезает полный конфиг)

/// Управляет соединением, publish и обработкой входящих команд.
class MqttClient {
public:
    /// `command` = суффикс после `commands/`.
    using CommandCallback = std::function<void(const char* command, JsonObjectConst data)>;

    /// `serialNumber` используется как username, `token` как password.
    void begin(const char* serialNumber, const char* token);

    void setCommandCallback(CommandCallback callback);

    /// ВАЖНО: внутри connect() всегда `clean_session = false`
    /// (persistent session). Менять категорически нельзя — поломает
    /// доставку команд от портала. Подробности и инцидент — в шапке
    /// MqttClient::connect() в `mqtt_client.cpp`.
    bool connect();

    void disconnect();

    bool isConnected();

    void loop();

    // ========================================================================
    // Публикация топиков согласно API
    // @see docs/03-mqtt/01-mqtt.md
    // Все методы используют константы из idryer_topics.h:
    //   - IDRYER_TOPIC_*      - имена топиков
    //   - IDRYER_QOS_*        - QoS уровни
    //   - IDRYER_RETAINED_*   - Retained флаги
    // ========================================================================

    /// Публикация в `info`.
    /// @param deviceType DeviceType (0 = Unknown → поле в JSON опускается для legacy)
    bool publishInfo(const char* hwVersion, const char* fwVersion, uint32_t workTimeCounter,
                     uint8_t unitsCount, const DryerUart::UnitConfig* units,
                     const char* mcuSerial = nullptr,
                     uint8_t deviceType = 0);

    /// Публикация в `telemetry`.
    bool publishTelemetry(JsonDocument& json);

    /// Публикация в `status`.
    bool publishStatus(JsonDocument& json);

    /// Публикация в `weights`.
    bool publishWeights(JsonDocument& json);

    /// Публикация в `rfid`.
    bool publishRfid(JsonDocument& json);

    /// Публикация в `events`.
    bool publishEvent(JsonDocument& json);

    /// Публикация в `integrations/status` (retained, QoS 1).
    /// Используется `LinkIntegrationsManager` — сводный снимок HA/Bambu/Moonraker.
    /// @see docs/ru/07-features/03-link-integrations-overview.md
    bool publishIntegrationsStatus(JsonDocument& json);

    /// Публикация в `config` без ручной фрагментации.
    bool publishConfig(JsonDocument& json);

    /**
     * @brief Публикация config топика с автоматической фрагментацией
     *
     * Если JSON > MQTT_CONFIG_CHUNK_SIZE — разбивает на чанки:
     * {"tid":N,"idx":0,"total":12345,"last":false,"d":"..."}
     *
     * Если JSON <= MQTT_CONFIG_CHUNK_SIZE — отправляет целиком.
     *
     * @param json Указатель на JSON строку (null-terminated)
     * @param length Длина JSON без \0
     * @return Количество отправленных сообщений (1 для маленького, N для фрагментированного), 0 при ошибке
     *
     * @see docs/10-flows/01-basic-flows.md (Remote Config), docs/03-mqtt/01-mqtt.md
     */
    uint16_t publishConfigRaw(const char* json, size_t length);

    /// Публикация в `config/delta` (без фрагментации).
    bool publishConfigDelta(const char* json, size_t length);

    // ========================================================================
    // Вспомогательные методы
    // ========================================================================

    /// Формат: `YYYY-MM-DDTHH:MM:SSZ`.
    static char* getIsoTimestamp(char* buffer);

    /// Буфер минимум 37 байт.
    static char* generateUuid(char* buffer);

private:
#if MQTT_USE_TLS
    WiFiClientSecure wifiClient_;
#else
    WiFiClient wifiClient_;
#endif
    PubSubClient mqttClient_;
    CommandCallback commandCallback_;

    char serialNumber_[32];
    char token_[512];
    char clientId_[32];

    // Буферы для топиков
    char topicBuffer_[TOPIC_BUFFER_SIZE];

    // Счётчик transferId для фрагментации config
    uint16_t configTransferId_ = 0;

    // Флаг инициализации
    bool initialized_ = false;

    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    void handleMessage(const char* topic, const char* payload, size_t length);

    const char* makeTopic(const char* suffix);

    /// Параметр `qos` здесь интерфейсный: фактическая реализация зависит от PubSubClient.
    bool publishJson(const char* suffix, JsonDocument& json, bool retained = false, int qos = 0);

    // Singleton для callback
    static MqttClient* instance_;
};

#endif // MQTT_CLIENT_H
