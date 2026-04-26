/**
 * @file ha_publisher.cpp
 * @brief Реализация публикатора для Home Assistant
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_publisher.h"
#include "../hal/hal_types.h"
#include <stdio.h>
#include <string.h>

namespace idryer {
namespace ha {

// =============================================================================
// КОНСТАНТЫ
// =============================================================================

namespace {
    constexpr const char* HA_PREFIX = "homeassistant";
    constexpr const char* DEVICE_NAME = "iDryer";
    constexpr const char* MANUFACTURER = "iDryer";
}

// =============================================================================
// КОНСТРУКТОР
// =============================================================================

HaPublisher::HaPublisher(HaMqttClient* mqtt)
    : mqtt_(mqtt) {
    memset(deviceId_, 0, sizeof(deviceId_));
    memset(hwVersion_, 0, sizeof(hwVersion_));
    memset(fwVersion_, 0, sizeof(fwVersion_));
    memset(topicBuf_, 0, sizeof(topicBuf_));
    memset(payloadBuf_, 0, sizeof(payloadBuf_));

    mqtt_->setMessageCallback([this](const char* topic, const char* payload) {
        handleIncomingMessage(topic, payload);
    });
}

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// =============================================================================

void HaPublisher::formatUnitId(char* buffer, size_t size, uint8_t unitId) {
    snprintf(buffer, size, "U%d", unitId + 1);
}

const char* HaPublisher::modeToString(DryerUart::DryerMode mode) {
    switch (mode) {
        case DryerUart::DryerMode::Idle: return "IDLE";
        case DryerUart::DryerMode::Drying: return "DRYING";
        case DryerUart::DryerMode::Storage: return "STORAGE";
        case DryerUart::DryerMode::Profile: return "PROFILE";
        case DryerUart::DryerMode::Fault: return "FAULT";
        default: return "UNKNOWN";
    }
}

void HaPublisher::makeStateTopic(char* buf, size_t size, uint8_t unitId, const char* sensorName) {
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(buf, size, "idryer/%s/%s/%s", deviceId_, unitStr, sensorName);
}

void HaPublisher::makeConfigTopic(char* buf, size_t size, const char* domain,
                                  uint8_t unitId, const char* sensorName) {
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(buf, size, "%s/%s/idryer_%s_%s_%s/config", HA_PREFIX, domain, deviceId_, unitStr, sensorName);
}

void HaPublisher::makeDeviceJson(char* buf, size_t size) {
    snprintf(buf, size,
        "\"device\":{"
            "\"identifiers\":[\"idryer_%s\"],"
            "\"name\":\"%s %s\","
            "\"model\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"sw_version\":\"%s\""
        "}",
        deviceId_,
        DEVICE_NAME, deviceId_,
        hwVersion_,
        MANUFACTURER,
        fwVersion_
    );
}

// =============================================================================
// DISCOVERY - СЕНСОРЫ
// =============================================================================

bool HaPublisher::publishSensorDiscovery(uint8_t unitId, const char* sensorName,
                                         const char* deviceClass, const char* unit,
                                         const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    // Формируем config топик
    makeConfigTopic(topicBuf_, sizeof(topicBuf_), "sensor", unitId, sensorName);

    // Формируем state топик
    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, sensorName);

    // Формируем unique_id
    char uniqueId[64];
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, sensorName);

    // Формируем name
    char name[64];
    snprintf(name, sizeof(name), "iDryer %s %s", unitStr, sensorName);

    // Формируем device JSON
    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    // Формируем полный payload
    int len = snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"%s\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\"",
        name, uniqueId, stateTopic
    );

    if (deviceClass) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"device_class\":\"%s\"", deviceClass);
    }

    if (unit) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"unit_of_measurement\":\"%s\"", unit);
    }

    if (icon) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"icon\":\"%s\"", icon);
    }

    // Добавляем device
    len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
        ",%s}", deviceJson);

    // Публикуем
    bool success = mqtt_->publish(topicBuf_, payloadBuf_, true);

    if (success) {
        HAL_LOG_DEBUG("HA_PUB", "Discovery sent: %s", sensorName);
    } else {
        HAL_LOG_ERROR("HA_PUB", "Discovery failed: %s", sensorName);
    }

    return success;
}

bool HaPublisher::publishBinarySensorDiscovery(uint8_t unitId, const char* sensorName,
                                                const char* deviceClass,
                                                const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    // Формируем config топик
    makeConfigTopic(topicBuf_, sizeof(topicBuf_), "binary_sensor", unitId, sensorName);

    // Формируем state топик
    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, sensorName);

    // Формируем unique_id
    char uniqueId[64];
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, sensorName);

    // Формируем name
    char name[64];
    snprintf(name, sizeof(name), "iDryer %s %s", unitStr, sensorName);

    // Формируем device JSON
    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    // Формируем полный payload
    int len = snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"%s\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"payload_on\":\"ON\","
            "\"payload_off\":\"OFF\"",
        name, uniqueId, stateTopic
    );

    if (deviceClass) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"device_class\":\"%s\"", deviceClass);
    }

    if (icon) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"icon\":\"%s\"", icon);
    }

    // Добавляем device
    len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
        ",%s}", deviceJson);

    // Публикуем
    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::publishAlertDiscovery() {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    // Config топик для сенсора alerts
    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/sensor/idryer_%s_alerts/config", HA_PREFIX, deviceId_);

    // State топик
    char stateTopic[HA_TOPIC_BUF_SIZE];
    snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/alerts", deviceId_);

    // unique_id
    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_alerts", deviceId_);

    // Формируем device JSON
    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    // Формируем payload
    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s Alerts\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"icon\":\"mdi:alert-circle\","
            "%s"
        "}",
        deviceId_, uniqueId, stateTopic, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

// =============================================================================
// ПУБЛИКАЦИЯ DISCOVERY
// =============================================================================

bool HaPublisher::publishDiscovery(const char* deviceId, uint8_t unitsCount,
                                   const char* hwVersion, const char* fwVersion,
                                   int tempMin, int tempMax, int durationMax) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        HAL_LOG_ERROR("HA_PUB", "Cannot publish Discovery: MQTT not connected");
        return false;
    }

    if (discoveryPublished_) {
        HAL_LOG_WARN("HA_PUB", "Discovery already published");
        return true;
    }

    // Сохраняем параметры
    strncpy(deviceId_, deviceId, sizeof(deviceId_) - 1);
    strncpy(hwVersion_, hwVersion, sizeof(hwVersion_) - 1);
    strncpy(fwVersion_, fwVersion, sizeof(fwVersion_) - 1);
    unitsCount_ = unitsCount;
    tempMin_ = tempMin;
    tempMax_ = tempMax;
    durationMax_ = durationMax;

    HAL_LOG_INFO("HA_PUB", "Publishing Discovery for %s (%d units)", deviceId_, unitsCount_);

    // Публикуем сенсоры для каждого юнита
    for (uint8_t i = 0; i < unitsCount_ && i < 3; i++) {
        // Температура
        if (!publishSensorDiscovery(i, "temperature", "temperature", "°C", "mdi:thermometer")) {
            return false;
        }

        // Влажность
        if (!publishSensorDiscovery(i, "humidity", "humidity", "%", "mdi:water-percent")) {
            return false;
        }

        // Мощность нагревателя
        if (!publishSensorDiscovery(i, "heater_power", "power_factor", "%", "mdi:radiator")) {
            return false;
        }

        // Режим работы
        if (!publishSensorDiscovery(i, "mode", nullptr, nullptr, "mdi:state-machine")) {
            return false;
        }

        // Вентилятор (binary sensor)
        if (!publishBinarySensorDiscovery(i, "fan", nullptr, "mdi:fan")) {
            return false;
        }

        // Вес филамента
        if (!publishSensorDiscovery(i, "weight", "weight", "g", "mdi:weight-gram")) {
            return false;
        }

        // Управление режимом (select)
        if (!publishSelectDiscovery(i)) {
            return false;
        }

        // Целевая температура (number)
        if (!publishNumberDiscovery(i, "target temp", "set_temp", "target_temp",
                                    tempMin_, tempMax_, "°C", "mdi:thermometer-plus")) {
            return false;
        }

        // Длительность сушки (number)
        if (!publishNumberDiscovery(i, "duration", "set_duration", "target_duration",
                                    10, durationMax_, "min", "mdi:timer-outline")) {
            return false;
        }
    }

    // Публикуем сенсор для alerts
    if (!publishAlertDiscovery()) {
        return false;
    }

    // Подписываемся на команды от HA
    subscribeToCommands();

    discoveryPublished_ = true;
    HAL_LOG_INFO("HA_PUB", "✓ Discovery published successfully");

    return true;
}

// =============================================================================
// ПУБЛИКАЦИЯ ДАННЫХ
// =============================================================================

bool HaPublisher::publishTelemetry(const DryerUart::TelemetryPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 3; i++) {
        const auto& entry = data.units[i];

        // Температура
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "temperature");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.temperatureC10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        // Влажность
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "humidity");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.humidityPct10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        // Мощность нагревателя
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "heater_power");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%d", entry.heaterPowerPct);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        // Вентилятор
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "fan");
        mqtt_->publish(topicBuf_, entry.fanOn ? "ON" : "OFF");
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Telemetry published (%d units)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::publishStatus(const DryerUart::StatusPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 3; i++) {
        const auto& entry = data.units[i];

        // Режим работы
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "mode");
        const char* modeStr = modeToString(entry.mode);
        allSuccess &= mqtt_->publish(topicBuf_, modeStr);
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Status published (%d units)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::publishWeights(const DryerUart::WeightsPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 8; i++) {
        const auto& entry = data.weights[i];

        // Публикуем вес для соответствующего юнита (значение в C10 формате)
        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "weight");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.weightGramsC10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Weights published (%d sensors)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::publishSelectDiscovery(uint8_t unitId) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);

    // Config топик
    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/select/idryer_%s_%s_%s/config", HA_PREFIX, deviceId_, unitStr, "mode_control");

    // State топик (тот же что публикует текущий режим)
    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, "mode");

    // Command топик — HA будет слать сюда выбор пользователя
    char cmdTopic[HA_TOPIC_BUF_SIZE];
    snprintf(cmdTopic, sizeof(cmdTopic), "idryer/%s/%s/set_mode", deviceId_, unitStr);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_mode_control", deviceId_, unitStr);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s mode control\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"command_topic\":\"%s\","
            "\"options\":[\"IDLE\",\"DRYING\",\"STORAGE\"],"
            "\"icon\":\"mdi:washing-machine\","
            "%s"
        "}",
        unitStr, uniqueId, stateTopic, cmdTopic, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::publishNumberDiscovery(uint8_t unitId, const char* name,
                                         const char* cmdSuffix, const char* stateSuffix,
                                         int min, int max,
                                         const char* unit, const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);

    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/number/idryer_%s_%s_%s/config", HA_PREFIX, deviceId_, unitStr, cmdSuffix);

    char cmdTopic[HA_TOPIC_BUF_SIZE];
    snprintf(cmdTopic, sizeof(cmdTopic), "idryer/%s/%s/%s", deviceId_, unitStr, cmdSuffix);

    char stateTopic[HA_TOPIC_BUF_SIZE];
    snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/%s", deviceId_, unitStr, stateSuffix);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, cmdSuffix);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s %s\","
            "\"unique_id\":\"%s\","
            "\"command_topic\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"min\":%d,\"max\":%d,\"step\":1,"
            "\"unit_of_measurement\":\"%s\","
            "\"icon\":\"%s\","
            "%s"
        "}",
        unitStr, name, uniqueId, cmdTopic, stateTopic,
        min, max, unit, icon, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::subscribeToCommands() {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char topic[HA_TOPIC_BUF_SIZE];

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_mode", deviceId_);
    mqtt_->subscribe(topic);

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_temp", deviceId_);
    mqtt_->subscribe(topic);

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_duration", deviceId_);
    mqtt_->subscribe(topic);

    return true;
}

void HaPublisher::handleIncomingMessage(const char* topic, const char* payload) {
    if (!topic || !payload) return;

    char prefix[HA_TOPIC_BUF_SIZE];
    snprintf(prefix, sizeof(prefix), "idryer/%s/", deviceId_);
    size_t prefixLen = strlen(prefix);

    if (strncmp(topic, prefix, prefixLen) != 0) return;

    const char* rest = topic + prefixLen; // "U1/set_mode"
    const char* slash = strchr(rest, '/');
    if (!slash) return;

    char unitStr[8];
    size_t unitLen = slash - rest;
    if (unitLen >= sizeof(unitStr)) return;
    memcpy(unitStr, rest, unitLen);
    unitStr[unitLen] = '\0';

    const char* suffix = slash + 1; // "set_mode" / "set_temp" / "set_duration"

    // Определяем индекс юнита (U1->0, U2->1, ...)
    uint8_t unitIdx = 0;
    if (unitStr[0] == 'U' && unitStr[1] >= '1' && unitStr[1] <= '4') {
        unitIdx = unitStr[1] - '1';
    }

    if (strcmp(suffix, "set_temp") == 0) {
        int val = atoi(payload);
        if (val >= 30 && val <= 85) {
            targetTempC_[unitIdx] = val;
            // Публикуем state обратно
            char stateTopic[HA_TOPIC_BUF_SIZE];
            snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/target_temp", deviceId_, unitStr);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", val);
            mqtt_->publish(stateTopic, buf);
            HAL_LOG_INFO("HA_PUB", "Set temp %s: %d°C", unitStr, val);
        }
        return;
    }

    if (strcmp(suffix, "set_duration") == 0) {
        int val = atoi(payload);
        if (val >= 10 && val <= 1440) {
            targetDurMin_[unitIdx] = val;
            char stateTopic[HA_TOPIC_BUF_SIZE];
            snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/target_duration", deviceId_, unitStr);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", val);
            mqtt_->publish(stateTopic, buf);
            HAL_LOG_INFO("HA_PUB", "Set duration %s: %dmin", unitStr, val);
        }
        return;
    }

    if (strcmp(suffix, "set_mode") == 0 && commandCallback_) {
        const char* command = nullptr;
        if (strcmp(payload, "DRYING") == 0)       command = "drying";
        else if (strcmp(payload, "STORAGE") == 0) command = "storage";
        else if (strcmp(payload, "IDLE") == 0)    command = "stop";

        if (!command) {
            HAL_LOG_WARN("HA_PUB", "Unknown mode: %s", payload);
            return;
        }

        HAL_LOG_INFO("HA_PUB", "Command from HA: %s -> %s (temp=%d, dur=%d)",
                     unitStr, command, targetTempC_[unitIdx], targetDurMin_[unitIdx]);
        commandCallback_(command, unitStr, targetTempC_[unitIdx], targetDurMin_[unitIdx]);
    }
}

bool HaPublisher::publishAlert(uint8_t unitId, const char* message, const char* severity) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    if (!message) {
        return false;
    }

    // Топик для alerts
    snprintf(topicBuf_, sizeof(topicBuf_), "idryer/%s/alerts", deviceId_);

    // Формируем payload с severity и unit
    char unitStr[4];
    if (unitId != 0xFF) {
        formatUnitId(unitStr, sizeof(unitStr), unitId);
        snprintf(payloadBuf_, sizeof(payloadBuf_), "[%s] %s: %s", severity, unitStr, message);
    } else {
        snprintf(payloadBuf_, sizeof(payloadBuf_), "[%s] %s", severity, message);
    }

    bool success = mqtt_->publish(topicBuf_, payloadBuf_);

    if (success) {
        HAL_LOG_INFO("HA_PUB", "Alert published: %s", payloadBuf_);
    }

    return success;
}

} // namespace ha
} // namespace idryer

#endif // ESP32
