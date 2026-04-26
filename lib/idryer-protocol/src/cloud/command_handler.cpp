/**
 * @file command_handler.cpp
 * @brief Реализация обработки MQTT команд
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "command_handler.h"
#include "link_integrations_manager.h"
#include "../hal/hal_types.h"
#include <string.h>
#include <mbedtls/base64.h>

namespace idryer
{
    namespace cloud
    {

        // =============================================================================
        // КОНСТРУКТОР
        // =============================================================================

        CommandHandler::CommandHandler(ICommandSink *sink)
            : sink_(sink)
        {
        }

        // =============================================================================
        // ГЛАВНЫЙ ОБРАБОТЧИК
        // =============================================================================

        void CommandHandler::handleMqttCommand(const char *command, JsonObjectConst data)
        {
            if (!sink_ || !command)
            {
                return;
            }

            // Синхронизируем время из timestamp команды
            syncTimeIfPresent(data);

            HAL_LOG_INFO("CMD", "MQTT command: %s", command);

            // Определяем тип команды и вызываем обработчик
            if (strcmp(command, "ping") == 0)
            {
                handlePing(data);
            }
            else if (strcmp(command, "drying") == 0)
            {
                handleStart(data);
            }
            else if (strcmp(command, "storage") == 0)
            {
                handleStorage(data);
            }
            else if (strcmp(command, "profile") == 0)
            {
                handleProfile(data);
            }
            else if (strcmp(command, "stop") == 0)
            {
                handleStop(data);
            }
            else if (strcmp(command, "find") == 0)
            {
                handleFind(data);
            }
            else if (strcmp(command, "set") == 0)
            {
                handleSet(data);
            }
            else if (strcmp(command, "invoke") == 0)
            {
                handleInvoke(data);
            }
            else if (strcmp(command, "get_config") == 0)
            {
                handleGetConfig(data);
            }
            else if (strcmp(command, "read_rfid") == 0)
            {
                handleReadRfid(data);
            }
            else if (strcmp(command, "write_rfid") == 0)
            {
                handleWriteRfid(data);
            }
            else if (strcmp(command, "clear_errors") == 0)
            {
                handleClearErrors(data);
            }
            else if (strcmp(command, "link_integration") == 0)
            {
                if (linkIntegrations_) {
                    linkIntegrations_->handleLinkIntegrationCommand(data);
                } else {
                    HAL_LOG_WARN("CMD", "link_integration: manager not registered, ignoring");
                }
            }
            else if (strcmp(command, "bambu_apply") == 0)
            {
                if (linkIntegrations_) {
                    linkIntegrations_->handleBambuApplyCommand(data);
                } else {
                    HAL_LOG_WARN("CMD", "bambu_apply: manager not registered, ignoring");
                }
            }
            else
            {
                HAL_LOG_WARN("CMD", "Unknown command: %s", command);
            }
        }

        // =============================================================================
        // ОБРАБОТЧИКИ КОМАНД
        // =============================================================================

        void CommandHandler::handlePing(JsonObjectConst data)
        {
            // ping - MQTT-only команда, без UART
            // Просто логируем что получили
            HAL_LOG_INFO("CMD", "Ping received");
        }

        void CommandHandler::handleStart(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::Start;
            cmd.targetState = static_cast<uint8_t>(DryerMode::Drying);

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0xFF; // Все юниты
            }

            // Парсим параметры из params объекта
            JsonObjectConst params = data["params"];
            if (params)
            {
                // Температура в градусах * 10
                if (params["temperature"].is<int>())
                {
                    cmd.arg0 = static_cast<int32_t>(params["temperature"].as<int>() * 10);
                }
                else
                {
                    cmd.arg0 = 550; // По умолчанию 55°C
                }

                // Длительность в минутах
                if (params["duration"].is<int>())
                {
                    cmd.arg1 = static_cast<uint32_t>(params["duration"].as<int>());
                }
                else
                {
                    cmd.arg1 = 240; // По умолчанию 4 часа
                }
            }
            else
            {
                // Fallback: старый формат без params
                if (data["targetTemperature"].is<float>())
                {
                    cmd.arg0 = static_cast<int32_t>(data["targetTemperature"].as<float>() * 10);
                }
                else
                {
                    cmd.arg0 = 550;
                }

                if (data["durationMinutes"].is<int>())
                {
                    cmd.arg1 = static_cast<uint32_t>(data["durationMinutes"].as<int>());
                }
                else
                {
                    cmd.arg1 = 240;
                }
            }

            HAL_LOG_INFO("CMD", "Start: unit=%s temp=%d.%d°C duration=%dmin",
                         cmd.unitId == 0xFF ? "ALL" : "Ux",
                         cmd.arg0 / 10, cmd.arg0 % 10, cmd.arg1);

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleStorage(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::Start;
            cmd.targetState = static_cast<uint8_t>(DryerMode::Storage);

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0xFF; // Все юниты
            }

            // Парсим параметры из params объекта
            JsonObjectConst params = data["params"];
            if (params)
            {
                // Температура в градусах * 10
                if (params["temperature"].is<int>())
                {
                    cmd.arg0 = static_cast<int32_t>(params["temperature"].as<int>() * 10);
                }
                else
                {
                    cmd.arg0 = 400; // По умолчанию 40°C
                }

                // Целевая влажность в процентах
                if (params["humidity"].is<int>())
                {
                    cmd.arg1 = static_cast<uint32_t>(params["humidity"].as<int>());
                }
                else
                {
                    cmd.arg1 = 15; // По умолчанию 15% RH
                }
            }
            else
            {
                cmd.arg0 = 400; // 40°C
                cmd.arg1 = 15;  // 15% RH
            }

            HAL_LOG_INFO("CMD", "Storage: unit=%s temp=%d.%d°C humidity=%d%%",
                         cmd.unitId == 0xFF ? "ALL" : "Ux",
                         cmd.arg0 / 10, cmd.arg0 % 10, cmd.arg1);

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleStop(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::Stop;
            cmd.targetState = static_cast<uint8_t>(DryerMode::Idle);

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0xFF; // Все юниты
            }

            cmd.arg0 = 0;
            cmd.arg1 = 0;

            HAL_LOG_INFO("CMD", "Stop: unit=%s",
                         cmd.unitId == 0xFF ? "ALL" : "Ux");

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleFind(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::Find;

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0xFF; // Все юниты
            }

            cmd.arg0 = 0;
            cmd.arg1 = 0;

            HAL_LOG_INFO("CMD", "Find: unit=%s",
                         cmd.unitId == 0xFF ? "ALL" : "Ux");

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleSet(JsonObjectConst data)
        {
            // set - установка значения настройки
            // Формат: { "cmd": "set", "id": 3, "unit": 0, "val": 55.0 }
            // JSON передаётся в RP2040 через ConfigPush

            if (!data["id"].is<int>())
            {
                HAL_LOG_WARN("CMD", "Set: missing 'id' field");
                return;
            }

            int id = data["id"].as<int>();
            int unit = data["unit"] | 0;

            // Сериализуем JSON для UART
            char jsonBuffer[128];
            StaticJsonDocument<128> doc;
            doc["cmd"] = "set";
            doc["id"] = id;
            doc["unit"] = unit;

            // Копируем значение (может быть число или массив)
            if (data.containsKey("val"))
            {
                doc["val"] = data["val"];
            }

            size_t jsonLen = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

            HAL_LOG_INFO("CMD", "\033[33mSet: id=%d unit=%d -> ConfigPush %d bytes\033[0m", id, unit, jsonLen);

            // Отправляем через ConfigPush (0x30)
            sendConfigPushJson(jsonBuffer, jsonLen);
        }

        void CommandHandler::handleInvoke(JsonObjectConst data)
        {
            // invoke - вызов действия (action)
            // Формат: { "cmd": "invoke", "id": 5 }
            // JSON передаётся в RP2040 через ConfigPush

            if (!data["id"].is<int>())
            {
                HAL_LOG_WARN("CMD", "Invoke: missing 'id' field");
                return;
            }

            int id = data["id"].as<int>();

            // Сериализуем JSON для UART
            char jsonBuffer[64];
            StaticJsonDocument<64> doc;
            doc["cmd"] = "invoke";
            doc["id"] = id;

            size_t jsonLen = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

            HAL_LOG_INFO("CMD", "\033[33mInvoke: id=%d -> ConfigPush %d bytes\033[0m", id, jsonLen);

            // Отправляем через ConfigPush (0x30)
            sendConfigPushJson(jsonBuffer, jsonLen);
        }

        void CommandHandler::handleGetConfig(JsonObjectConst data)
        {
            using namespace DryerUart;

            HAL_LOG_INFO("CMD", "========== GET_CONFIG RECEIVED ==========");

            CommandPayload cmd{};
            cmd.command = CommandCode::GetConfig;

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0; // По умолчанию первый юнит
            }

            HAL_LOG_INFO("CMD", "GetConfig: unit=U%d, sending to MCU...", cmd.unitId + 1);

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleReadRfid(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::ReadRfid;

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0;
            }

            HAL_LOG_INFO("CMD", "ReadRfid: unit=U%d", cmd.unitId + 1);

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleWriteRfid(JsonObjectConst data)
        {
            using namespace DryerUart;

            // Парсим unitId.
            uint8_t unitId = 0;
            if (data["unitId"].is<const char *>())
            {
                unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            if (unitId == 0xFF)
            {
                HAL_LOG_WARN("CMD", "WriteRfid: invalid unitId");
                return;
            }

            // Парсим base64 data.
            const char *b64 = data["data"].is<const char *>() ? data["data"].as<const char *>() : nullptr;
            if (!b64 || !*b64)
            {
                HAL_LOG_WARN("CMD", "WriteRfid: missing or empty 'data'");
                return;
            }

            // Декодируем base64 → сырые байты. Размер RFID_DATA_SIZE=888 — максимум.
            constexpr size_t kMaxRfidBytes = 888;
            uint8_t raw[kMaxRfidBytes] = {};
            size_t rawLen = 0;
            int rc = mbedtls_base64_decode(raw, sizeof(raw), &rawLen,
                                           reinterpret_cast<const unsigned char *>(b64),
                                           strlen(b64));
            if (rc != 0 || rawLen == 0 || rawLen > kMaxRfidBytes)
            {
                HAL_LOG_WARN("CMD", "WriteRfid: base64 decode failed rc=%d len=%u", rc, (unsigned)rawLen);
                return;
            }

            // verifyMode: 0=None, 1=Header32 (по умолчанию), 2=Full.
            uint32_t verify = 1;
            if (data["verify"].is<const char *>())
            {
                const char *v = data["verify"].as<const char *>();
                if (strcmp(v, "none") == 0) verify = 0;
                else if (strcmp(v, "full") == 0) verify = 2;
                else verify = 1;
            }

            HAL_LOG_INFO("CMD", "WriteRfid: unit=U%d size=%u verify=%u",
                         unitId + 1, (unsigned)rawLen, (unsigned)verify);

            // Stop-and-wait flow control (XMODEM + CAN-TP style): каждый кадр идёт
            // с FLAG_ACK_REQUIRED, следующий не шлётся до ACK или таймаута. Это защищает
            // hardware-RX-FIFO приёмника от overflow когда его main loop занят (PN5180 poll).
            constexpr uint32_t kAckTimeoutMs = 200; // с запасом на длинный PN5180-poll
            constexpr size_t kFragSize = 163;

            // 1) Command WriteRfid с ACK required — MCU ответит после armed staging.
            CommandPayload cmd{};
            cmd.command = CommandCode::WriteRfid;
            cmd.unitId = unitId;
            cmd.arg0 = static_cast<uint32_t>(rawLen);
            cmd.arg1 = verify;
            sink_->sendCommand(cmd, true); // ackRequired = true
            if (!sink_->waitForAck(kAckTimeoutMs))
            {
                HAL_LOG_ERROR("CMD", "WriteRfid: Command ACK timeout — aborting");
                return;
            }

            // 2) Фрагменты stop-and-wait. readerId = 0xFF — LINK не знает mapping.
            const size_t fragCount = (rawLen + kFragSize - 1) / kFragSize;
            for (size_t f = 0; f < fragCount; f++)
            {
                RfidDataPayload frag{};
                frag.readerId = 0xFF;
                frag.unitId = unitId;
                // tag[32] оставляем пустым — MCU валидирует по факту наличия метки.

                const size_t srcOff = f * kFragSize;
                size_t copyLen = rawLen - srcOff;
                if (copyLen > kFragSize) copyLen = kFragSize;
                memcpy(frag.fragment, raw + srcOff, copyLen);

                const bool isLast = (f == fragCount - 1);
                uint8_t flags = FLAG_ACK_REQUIRED;
                flags |= isLast ? FLAG_LAST_FRAGMENT : FLAG_FRAGMENTED;
                sink_->sendRfidWriteData(frag, flags);

                if (!sink_->waitForAck(kAckTimeoutMs))
                {
                    HAL_LOG_ERROR("CMD", "WriteRfid: frag[%u] ACK timeout — aborting",
                                  (unsigned)f);
                    return;
                }
            }

            HAL_LOG_INFO("CMD", "WriteRfid: all %u fragments ACK'd", (unsigned)fragCount);
        }

        void CommandHandler::handleClearErrors(JsonObjectConst data)
        {
            using namespace DryerUart;

            CommandPayload cmd{};
            cmd.command = CommandCode::ClearErrors;

            // Парсим unitId
            if (data["unitId"].is<const char *>())
            {
                cmd.unitId = parseUnitId(data["unitId"].as<const char *>());
            }
            else
            {
                cmd.unitId = 0xFF; // Все юниты
            }

            cmd.arg0 = 0;
            cmd.arg1 = 0;

            HAL_LOG_INFO("CMD", "ClearErrors: unit=%s",
                         cmd.unitId == 0xFF ? "ALL" : "Ux");

            sink_->sendCommand(cmd, true);
        }

        void CommandHandler::handleProfile(JsonObjectConst data)
        {
            using namespace DryerUart;

            // Парсим unitId
            uint8_t unitId = 0xFF;
            if (data["unitId"].is<const char *>())
            {
                unitId = parseUnitId(data["unitId"].as<const char *>());
            }

            // Парсим stages массив
            JsonArrayConst stagesArray = data["stages"];
            if (!stagesArray || stagesArray.size() == 0)
            {
                HAL_LOG_ERROR("CMD", "Profile: no stages provided");
                return;
            }

            if (stagesArray.size() > 10)
            {
                HAL_LOG_WARN("CMD", "Profile: too many stages (%d), max 10", stagesArray.size());
            }

            // Формируем ProfilePayload
            ProfilePayload payload{};
            payload.unitId = unitId;
            payload.totalStages = stagesArray.size() > 10 ? 10 : stagesArray.size();
            payload.startStage = data["startStage"] | 0; // По умолчанию 0

            // Парсим этапы
            for (uint8_t i = 0; i < payload.totalStages; i++)
            {
                JsonObjectConst stage = stagesArray[i];

                // temp (в °C, конвертируем в ×10)
                if (stage["temp"].is<int>())
                {
                    payload.stages[i].temp = stage["temp"].as<int>() * 10;
                }
                else if (stage["temp"].is<float>())
                {
                    payload.stages[i].temp = (uint16_t)(stage["temp"].as<float>() * 10);
                }
                else
                {
                    HAL_LOG_ERROR("CMD", "Profile: stage[%d] missing 'temp'", i);
                    return;
                }

                // ramp (в секундах)
                if (stage["ramp"].is<int>())
                {
                    payload.stages[i].ramp = stage["ramp"].as<int>();
                }
                else
                {
                    HAL_LOG_ERROR("CMD", "Profile: stage[%d] missing 'ramp'", i);
                    return;
                }

                // hold (в секундах)
                if (stage["hold"].is<int>())
                {
                    payload.stages[i].hold = stage["hold"].as<int>();
                }
                else
                {
                    HAL_LOG_ERROR("CMD", "Profile: stage[%d] missing 'hold'", i);
                    return;
                }
            }

            HAL_LOG_INFO("CMD", "Profile: unit=%s stages=%d startStage=%d",
                         unitId == 0xFF ? "ALL" : "Ux",
                         payload.totalStages,
                         payload.startStage);

            // Отправляем через sendProfileCommand (который использует MessageKind::Command)
            sink_->sendProfileCommand(payload, true);
        }

        // =============================================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =============================================================================

        uint8_t CommandHandler::parseUnitId(const char *unitIdStr)
        {
            if (!unitIdStr)
            {
                return 0xFF;
            }

            // Формат: "U1", "U2", "U3", "U4"
            if (unitIdStr[0] == 'U' && unitIdStr[1] >= '1' && unitIdStr[1] <= '4')
            {
                return unitIdStr[1] - '1'; // "U1" -> 0, "U2" -> 1, ...
            }

            return 0xFF; // Некорректный формат -> все юниты
        }

        void CommandHandler::setTimeSyncCallback(TimeSyncCallback callback, void *context)
        {
            timeSyncCallback_ = callback;
            timeSyncContext_ = context;
        }

        void CommandHandler::setLinkIntegrationsManager(LinkIntegrationsManager *manager)
        {
            linkIntegrations_ = manager;
        }

        void CommandHandler::syncTimeIfPresent(JsonObjectConst data)
        {
            if (!timeSyncCallback_)
            {
                return;
            }

            if (data["timestamp"].is<const char *>())
            {
                const char *timestamp = data["timestamp"].as<const char *>();
                timeSyncCallback_(timestamp, timeSyncContext_);
            }
        }

        void CommandHandler::sendConfigPushJson(const char *json, size_t length)
        {
            using namespace DryerUart;

            // Для маленького JSON (< 194 байт) отправляем одним фрагментом
            if (length <= CONFIG_CHUNK_DATA_SIZE)
            {
                ConfigChunkPayload payload{};
                payload.header.transferId = ++configTransferId_;
                payload.header.totalSize = (uint16_t)length;
                payload.header.chunkIndex = 0;

                memcpy(payload.data, json, length);

                // Один фрагмент = сразу LAST_FRAGMENT
                uint8_t flags = FLAG_ACK_REQUIRED | FLAG_LAST_FRAGMENT;
                uint8_t payloadLen = CONFIG_CHUNK_HEADER_SIZE + length;

                sink_->sendConfigPushChunk(payload, payloadLen, flags);

                HAL_LOG_DEBUG("CMD", "ConfigPush sent: tid=%d len=%d",
                              payload.header.transferId, length);
            }
            else
            {
                // Фрагментация для большого JSON (не ожидается для set/invoke)
                HAL_LOG_WARN("CMD", "ConfigPush fragmentation not implemented for %d bytes", length);
            }
        }

    } // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
