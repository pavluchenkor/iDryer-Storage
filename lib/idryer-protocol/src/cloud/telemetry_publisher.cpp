/**
 * @file telemetry_publisher.cpp
 * @brief Реализация публикации телеметрии в MQTT
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "telemetry_publisher.h"
#include "../hal/hal_types.h"
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <stdio.h>

namespace idryer
{
    namespace cloud
    {

        // =============================================================================
        // КОНСТРУКТОР
        // =============================================================================

        TelemetryPublisher::TelemetryPublisher(MqttClient *mqtt)
            : mqtt_(mqtt)
        {
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ ТЕЛЕМЕТРИИ
        // =============================================================================

        bool TelemetryPublisher::publishTelemetry(const DryerUart::TelemetryPayload &data)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return false;
            }

            // Формируем JSON
            DynamicJsonDocument doc(1024);
            JsonArray units = doc.createNestedArray("units");

            for (uint8_t i = 0; i < data.count && i < 4; ++i)
            {
                const auto &entry = data.units[i];
                JsonObject unit = units.createNestedObject();

                // unitId как "U1", "U2", ...
                char unitIdStr[4];
                formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
                unit["unitId"] = unitIdStr;

                // Температура в градусах (из int16 * 10)
                unit["temperature"] = entry.temperatureC10 / 10.0f;

                // Влажность в процентах (из uint16 * 10)
                unit["humidity"] = entry.humidityPct10 / 10.0f;

                // Мощность нагревателя
                unit["heaterPower"] = entry.heaterPowerPct;

                // Статус вентилятора
                unit["fanStatus"] = (entry.fanOn == 1);
            }

            // Публикуем
            if (mqtt_->publishTelemetry(doc))
            {
                HAL_LOG_DEBUG("TELEM", "Published telemetry (%d units)", data.count);
                return true;
            }

            return false;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ СТАТУСА
        // =============================================================================

        bool TelemetryPublisher::publishStatus(const DryerUart::StatusPayload &data)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return false;
            }

            // Формируем JSON
            DynamicJsonDocument doc(2048);
            JsonArray units = doc.createNestedArray("units");

            for (uint8_t i = 0; i < data.count && i < 4; ++i)
            {
                const auto &entry = data.units[i];
                JsonObject unit = units.createNestedObject();

                // unitId
                char unitIdStr[4];
                formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
                unit["unitId"] = unitIdStr;

                // Режим работы
                unit["mode"] = modeToString(entry.mode);

                // Для активных режимов добавляем детали
                if (entry.mode != DryerUart::DryerMode::Idle &&
                    entry.mode != DryerUart::DryerMode::Fault)
                {

                    unit["sessionNum"] = entry.sessionNum;

                    // Целевые параметры
                    JsonObject target = unit.createNestedObject("target");
                    target["temperature"] = entry.targetTempC10 / 10.0f;
                    target["duration"] = entry.durationMinutes;
                    if (entry.targetHumidityPct > 0)
                    {
                        target["humidity"] = entry.targetHumidityPct;
                    }

                    // Таймеры
                    unit["totalElapsed"] = entry.elapsedSeconds;
                    unit["totalRemaining"] = entry.totalRemainingSeconds;

                    // Для профильного режима - этапы
                    if (entry.mode == DryerUart::DryerMode::Profile)
                    {
                        unit["currentStage"] = entry.currentStage;
                        unit["totalStages"] = entry.totalStages;
                        unit["stageElapsed"] = entry.stageElapsedSeconds;
                        unit["stageRemaining"] = entry.stageRemainingSeconds;
                        unit["stagePhase"] = (entry.stagePhase == DryerUart::StagePhase::Ramp) ? "RAMP" : "HOLD";
                    }
                }
            }

            // Uptime устройства
            doc["uptime"] = data.uptime;

            // Публикуем
            if (mqtt_->publishStatus(doc))
            {
                HAL_LOG_DEBUG("TELEM", "Published status (%d units)", data.count);
                return true;
            }

            return false;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ ВЕСОВ
        // =============================================================================

        bool TelemetryPublisher::publishWeights(const DryerUart::WeightsPayload &data)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return false;
            }

            // Формируем JSON
            DynamicJsonDocument doc(512);
            JsonArray weights = doc.createNestedArray("weights");

            for (uint8_t i = 0; i < data.count && i < 4; ++i)
            {
                const auto &entry = data.weights[i];
                JsonObject w = weights.createNestedObject();

                // sensorId как "W1", "W2", ...
                char sensorIdStr[4];
                formatSensorId(sensorIdStr, sizeof(sensorIdStr), entry.sensorId);
                w["sensorId"] = sensorIdStr;

                // Вес в граммах
                w["value"] = static_cast<float>(entry.weightGramsC10) / 10.0f;

                // unitId
                char unitIdStr[4];
                formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
                w["unitId"] = unitIdStr;
            }

            // Публикуем
            if (mqtt_->publishWeights(doc))
            {
                HAL_LOG_DEBUG("TELEM", "Published weights (%d sensors)", data.count);
                return true;
            }

            return false;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ RFID
        // =============================================================================

        bool TelemetryPublisher::publishRfid(const DryerUart::RfidPayload &data)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return false;
            }

            // Формируем JSON
            DynamicJsonDocument doc(512);

            // unitId
            char unitIdStr[4];
            formatUnitId(unitIdStr, sizeof(unitIdStr), data.unitId);
            doc["unitId"] = unitIdStr;

            // Тип события
            doc["event"] = rfidEventToString(data.event);

            // RFID метка (копируем с null-termination)
            char tag[33];
            strncpy(tag, data.tag, sizeof(tag) - 1);
            tag[sizeof(tag) - 1] = '\0';
            doc["tag"] = tag;

            // readerId
            doc["readerId"] = data.readerId;

            // Публикуем
            if (mqtt_->publishRfid(doc))
            {
                HAL_LOG_INFO("TELEM", "Published RFID: %s tag=%s",
                             rfidEventToString(data.event), tag);
                return true;
            }

            return false;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ RFID DATA (888 байт, base64)
        // =============================================================================

        // Простой поиск байтовой подстроки в буфере (без std::search и memmem —
        // memmem GNU-extension, не всегда есть в ESP-IDF newlib).
        static bool rfidBufferContains(const uint8_t *data, size_t dataLen,
                                       const char *pattern, size_t patternLen)
        {
            if (patternLen == 0 || patternLen > dataLen) return false;
            for (size_t i = 0; i + patternLen <= dataLen; i++)
            {
                if (memcmp(data + i, pattern, patternLen) == 0) return true;
            }
            return false;
        }

        // Определяет формат по MIME-сигнатуре в первых байтах буфера.
        // OpenPrintTag (Prusa) и OpenSpool — известные стандарты с явным MIME-типом
        // в NDEF-сообщении. Для остальных форматов — "unknown" (парсит портал).
        static const char *detectRfidFormat(const uint8_t *data, size_t len, bool allZero)
        {
            if (allZero) return "empty";

            // MIME-тип лежит в NDEF-заголовке, обычно в первых 40-50 байтах.
            // Проверяем первые 128 байт с запасом.
            const size_t scanLen = len < 128 ? len : 128;

            static const char kOpenPrintTag[] = "application/vnd.openprinttag";
            static const char kOpenSpool[]    = "application/vnd.openspool";

            if (rfidBufferContains(data, scanLen, kOpenPrintTag, sizeof(kOpenPrintTag) - 1))
                return "openprinttag";
            if (rfidBufferContains(data, scanLen, kOpenSpool, sizeof(kOpenSpool) - 1))
                return "openspool";
            return "unknown";
        }

        bool TelemetryPublisher::publishRfidData(const uint8_t *data, size_t len,
                                                 const DryerUart::RfidDataPayload &meta)
        {
            if (!mqtt_ || !mqtt_->isConnected())
                return false;

            // Base64 encode (mbedtls доступен на ESP32)
            size_t b64Len = 0;
            const size_t b64BufSize = ((len + 2) / 3) * 4 + 1; // 1184 + 1
            char b64[1200] = {};
            mbedtls_base64_encode(
                reinterpret_cast<unsigned char *>(b64), sizeof(b64), &b64Len,
                data, len);
            b64[b64Len] = '\0';

            // Определяем формат: пусто → empty, OpenPrintTag/OpenSpool по MIME-сигнатуре,
            // иначе → unknown (сырой парсинг на портале).
            bool allZero = true;
            for (size_t i = 0; i < len && allZero; i++)
                if (data[i] != 0) allZero = false;

            const char *format = detectRfidFormat(data, len, allZero);

            DynamicJsonDocument doc(1700);
            doc["readerId"] = meta.readerId;

            char unitIdStr[4];
            formatUnitId(unitIdStr, sizeof(unitIdStr), meta.unitId);
            doc["unitId"] = unitIdStr;

            doc["tagId"]  = meta.tag;
            doc["format"] = format;
            doc["data"]   = allZero ? nullptr : (const char *)b64;

            if (mqtt_->publishRfid(doc))
            {
                HAL_LOG_INFO("TELEM", "Published RFID data: %u bytes format=%s",
                             (unsigned)len, format);
                return true;
            }
            return false;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ INFO
        // =============================================================================

        bool TelemetryPublisher::publishInfo(uint8_t unitsCount, const DryerUart::UnitConfig *units,
                                             const char *hwVersion, const char *fwVersion, uint32_t workTime,
                                             const char *mcuSerial, uint8_t deviceType)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return false;
            }

            // Публикуем только один раз
            if (infoPublished_)
            {
                return true;
            }

            mqtt_->publishInfo(hwVersion, fwVersion, workTime, unitsCount, units, mcuSerial, deviceType);
            infoPublished_ = true;

            HAL_LOG_INFO("TELEM", "Published info: hw=%s fw=%s units=%d deviceType=%u",
                         hwVersion, fwVersion, unitsCount, (unsigned)deviceType);

            return true;
        }

        // =============================================================================
        // ПУБЛИКАЦИЯ CONFIG
        // =============================================================================

        uint16_t TelemetryPublisher::publishConfig(const char *json, uint16_t length)
        {
            if (!mqtt_ || !mqtt_->isConnected())
            {
                return 0;
            }

            uint16_t result = mqtt_->publishConfigRaw(json, length);

            if (result > 0)
            {
                HAL_LOG_INFO("TELEM", "Published config: %u bytes in %u message(s)", length, result);
            }
            else
            {
                HAL_LOG_ERROR("TELEM", "Failed to publish config");
            }

            return result;
        }

        // =============================================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =============================================================================

        void TelemetryPublisher::formatUnitId(char *buffer, size_t size, uint8_t unitId)
        {
            snprintf(buffer, size, "U%d", unitId + 1);
        }

        void TelemetryPublisher::formatSensorId(char *buffer, size_t size, uint8_t sensorId)
        {
            snprintf(buffer, size, "W%d", sensorId + 1);
        }

        const char *TelemetryPublisher::modeToString(DryerUart::DryerMode mode)
        {
            switch (mode)
            {
            case DryerUart::DryerMode::Idle:
                return "IDLE";
            case DryerUart::DryerMode::Drying:
                return "DRYING";
            case DryerUart::DryerMode::Storage:
                return "STORAGE";
            case DryerUart::DryerMode::Profile:
                return "PROFILE";
            case DryerUart::DryerMode::Fault:
                return "FAULT";
            default:
                return "UNKNOWN";
            }
        }

        const char *TelemetryPublisher::rfidEventToString(DryerUart::RfidEvent event)
        {
            switch (event)
            {
            case DryerUart::RfidEvent::TagDetected:
                return "tag_detected";
            case DryerUart::RfidEvent::TagRemoved:
                return "tag_removed";
            default:
                return "unknown";
            }
        }

    } // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
