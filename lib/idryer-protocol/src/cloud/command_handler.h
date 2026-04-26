/**
 * @file command_handler.h
 * @brief Парсинг и валидация входящих MQTT-команд.
 *
 * Применение разобранной команды делегируется через ICommandSink —
 * на iDryer это UartCommandSink (шлёт в UartBridge), на устройствах
 * без MCU — своя реализация потребителя.
 */

#pragma once

#include "command_sink.h"
#include "../uart/uart_protocol.h"
#include <ArduinoJson.h>

namespace idryer {
namespace cloud {

class LinkIntegrationsManager;  // forward-declare, не тянем тяжёлый заголовок

/// `isoTimestamp` ожидается в ISO 8601 (UTC).
typedef void (*TimeSyncCallback)(const char* isoTimestamp, void* context);

class CommandHandler {
public:
    explicit CommandHandler(ICommandSink* sink);

    /// `command` — суффикс из MQTT `commands/<command>`.
    void handleMqttCommand(const char* command, JsonObjectConst data);

    void setTimeSyncCallback(TimeSyncCallback callback, void* context);

    /// Регистрирует оркестратор LINK-интеграций. Если не задан, команды
    /// `link_integration` и `bambu_apply` логируются и игнорируются.
    /// @see docs/ru/07-features/03-link-integrations-overview.md
    void setLinkIntegrationsManager(LinkIntegrationsManager* manager);

private:
    // Обработчики поддерживаемых команд.
    void handlePing(JsonObjectConst data);
    void handleStart(JsonObjectConst data);
    void handleStorage(JsonObjectConst data);
    void handleProfile(JsonObjectConst data);
    void handleStop(JsonObjectConst data);
    void handleFind(JsonObjectConst data);
    void handleSet(JsonObjectConst data);
    void handleInvoke(JsonObjectConst data);
    void handleGetConfig(JsonObjectConst data);
    void handleReadRfid(JsonObjectConst data);
    void handleWriteRfid(JsonObjectConst data);
    void handleClearErrors(JsonObjectConst data);

    /// `"U1" -> 0`, ошибочный/пустой ввод -> `0xFF`.
    uint8_t parseUnitId(const char* unitIdStr);

    void syncTimeIfPresent(JsonObjectConst data);

    /// Отправка JSON в MCU через ConfigPush (`0x30`).
    void sendConfigPushJson(const char* json, size_t length);

    ICommandSink* sink_;
    uint16_t configTransferId_ = 0;  // Локальный счётчик transferId для ConfigPush.
    TimeSyncCallback timeSyncCallback_ = nullptr;
    void* timeSyncContext_ = nullptr;
    LinkIntegrationsManager* linkIntegrations_ = nullptr;  // опционально
};

} // namespace cloud
} // namespace idryer
