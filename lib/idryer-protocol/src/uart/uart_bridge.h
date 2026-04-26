/**
 * @file uart_bridge.h
 * @brief Двунаправленный UART протокол между Link и Controller.
 */

#pragma once

#include <functional>

#include "../hal/hal_types.h"
#include "uart_protocol.h"

namespace DryerUart
{

    /// Парсинг фреймов, ACK/retry и dispatch в callbacks.
    class UartBridge
    {
    public:
        // Типы callbacks.

        /// Callback для Hello от RP2040 (версия, кол-во камер)
        using HelloHandler = std::function<void(const HelloPayload &, const FrameHeader &)>;

        /// Callback для HelloAck от ESP32 (подтверждение handshake, IP/SSID)
        using HelloAckHandler = std::function<void(const HelloAckPayload &, const FrameHeader &)>;

        /// Callback для телеметрии (температура, влажность, мощность)
        using TelemetryHandler = std::function<void(const TelemetryPayload &, const FrameHeader &)>;

        /// Callback для команд (start, stop, config)
        using CommandHandler = std::function<void(const CommandPayload &, const FrameHeader &)>;

        /// Callback для Profile команд
        using ProfileHandler = std::function<void(const ProfilePayload &, const FrameHeader &)>;

        /// Callback для конфигурации (legacy)
        using ConfigHandler = std::function<void(const ConfigPayload &, const FrameHeader &)>;

        /// Callback для фрагментированного конфига (ConfigPush с JSON)
        /// @param payload Фрагмент конфига
        /// @param dataLen Реальная длина данных (payloadLen - headerSize)
        /// @param header Заголовок фрейма (содержит flags для определения LAST_FRAGMENT)
        using ConfigPushChunkHandler = std::function<void(const ConfigChunkPayload &, uint8_t dataLen, const FrameHeader &)>;

        /// Callback для ACK на команду
        using CommandAckHandler = std::function<void(const AckPayload &, const FrameHeader &)>;

        /// Callback для ACK на конфиг
        using ConfigAckHandler = std::function<void(const AckPayload &, const FrameHeader &)>;

        /// Callback для heartbeat
        using HeartbeatHandler = std::function<void(const HeartbeatPayload &, const FrameHeader &)>;

        /// Callback для ошибок (remote=true если ошибка от другой стороны)
        using ErrorHandler = std::function<void(const ErrorPayload &, bool remote)>;

        /// Callback для логов (произвольные строки)
        using LogHandler = std::function<void(const uint8_t *payload, uint8_t length)>;

        /// Callback для весов филамента
        using WeightsHandler = std::function<void(const WeightsPayload &, const FrameHeader &)>;

        /// Callback для статуса режима работы
        using StatusHandler = std::function<void(const StatusPayload &, const FrameHeader &)>;

        /// Callback для RFID событий
        using RfidHandler = std::function<void(const RfidPayload &, const FrameHeader &)>;

        /// Callback для RFID данных (чтение/запись меток)
        using RfidDataHandler = std::function<void(const RfidDataPayload &, const FrameHeader &)>;

        /// Callback для начала claiming (пустой payload)
        using ClaimStartHandler = std::function<void(const FrameHeader &)>;

        /// Callback для статуса claiming (PIN)
        using ClaimStatusHandler = std::function<void(const ClaimStatusPayload &, const FrameHeader &)>;

        /// Callback для завершения claiming
        using ClaimCompleteHandler = std::function<void(const ClaimCompletePayload &, const FrameHeader &)>;

        /// Callback для WS Enable (включить/выключить WS сервер)
        using WsEnableHandler = std::function<void(const WsEnablePayload &, const FrameHeader &)>;

        /// Callback для WS Status (статус WS сервера)
        using WsStatusHandler = std::function<void(const WsStatusPayload &, const FrameHeader &)>;

        /// Callback для WS Reset Clients (сброс привязок, пустой payload)
        using WsResetClientsHandler = std::function<void(const FrameHeader &)>;

        /// Callback для WS Status Request (запрос статуса, пустой payload)
        using WsStatusRequestHandler = std::function<void(const FrameHeader &)>;

        // Инициализация и цикл.
        /// `serial` — HAL-обёртка, не `HardwareSerial`.
        void begin(idryer::hal::ISerial *serial, uint32_t baudRate = 115200);

        /// Обслуживает RX-парсер и retry pending ACK.
        void loop();

        /// Сбрасывает parser/pending/sequence state.
        void reset();

        // Отправка сообщений.

        /// Отправить Hello (RP2040 -> ESP32)
        bool sendHello(const HelloPayload &payload, bool ackRequired = false);

        /// Отправить HelloAck (ESP32 -> RP2040)
        bool sendHelloAck(const HelloAckPayload &payload);

        /// Отправить телеметрию (RP2040 -> ESP32)
        bool sendTelemetry(const TelemetryPayload &payload, bool ackRequired = false);

        /// Отправить статус (RP2040 -> ESP32)
        bool sendStatus(const StatusPayload &payload, bool ackRequired = false);

        /// Отправить данные весов (RP2040 -> ESP32)
        bool sendWeights(const WeightsPayload &payload, bool ackRequired = false);

        /// Отправить RFID событие (RP2040 -> ESP32)
        bool sendRfid(const RfidPayload &payload, bool ackRequired = false);

        /// Отправить команду (ESP32 -> RP2040)
        bool sendCommand(const CommandPayload &payload, bool ackRequired = true);

        /// Отправить конфигурацию (ESP32 -> RP2040) - legacy
        bool sendConfigPush(const ConfigPayload &payload, bool ackRequired = true);

        /// Отправить фрагмент конфига (для фрагментированной передачи JSON)
        /// @param payload Payload с заголовком и данными
        /// @param payloadLen Реальная длина payload (header + data)
        /// @param protocolFlags Флаги протокола (FLAG_FRAGMENTED, FLAG_LAST_FRAGMENT)
        bool sendConfigPushChunk(const ConfigChunkPayload &payload, uint8_t payloadLen, uint8_t protocolFlags);

        /// Отправить heartbeat (обе стороны)
        bool sendHeartbeat(const HeartbeatPayload &payload);

        /// Отправить ошибку (обе стороны)
        bool sendError(const ErrorPayload &payload);

        /// Блокирующее ожидание ACK на последний отправленный frame с FLAG_ACK_REQUIRED.
        /// Внутри крутит tick() — обрабатывает RX и выполняет retry pending_ при таймаутах.
        /// Используется для stop-and-wait flow control (CAN-TP-style) на фрагментированных
        /// transfer'ах (WriteRfid), чтобы не перегружать RX-буфер приёмника back-to-back.
        /// @return true если ACK получен в пределах timeoutMs, false при таймауте.
        bool waitForAck(uint32_t timeoutMs);

        /// ACK на телеметрию
        bool sendTelemetryAck(uint8_t sequence, ErrorCode status = ErrorCode::None);

        /// ACK на команду
        bool sendCommandAck(uint8_t sequence, ErrorCode status = ErrorCode::None);

        /// ACK на конфиг
        bool sendConfigAck(uint8_t sequence, ErrorCode status = ErrorCode::None);

        /// Отправить лог (произвольные данные)
        bool sendLog(const uint8_t *message, uint8_t length);

        /// Отправить лог (C-строка)
        bool sendLog(const char *cstr);

        // RFID data (чтение/запись меток).

        /// Отправить данные для записи на RFID (ESP32 -> RP2040)
        bool sendRfidWriteData(const RfidDataPayload &payload, uint8_t protocolFlags = 0);

        /// Отправить прочитанные данные с RFID (RP2040 -> ESP32)
        bool sendRfidReadData(const RfidDataPayload &payload, uint8_t protocolFlags = 0);

        // Профильная сушка.

        /// Отправить профиль сушки (ESP32 -> RP2040)
        bool sendProfileCommand(const ProfilePayload &payload, bool ackRequired = true);

        // Device claiming (`0x70-0x72`).

        /// Отправить запрос начала claiming (RP2040 -> ESP32)
        bool sendClaimStart(bool ackRequired = true);

        /// Отправить статус claiming с PIN (ESP32 -> RP2040)
        bool sendClaimStatus(const ClaimStatusPayload &payload);

        /// Отправить завершение claiming (ESP32 -> RP2040)
        bool sendClaimComplete(const ClaimCompletePayload &payload);

        // WebSocket local access (`0x73-0x76`).

        /// Отправить команду включения/выключения WS (RP2040 -> ESP32)
        bool sendWsEnable(const WsEnablePayload &payload);

        /// Отправить статус WS сервера (ESP32 -> RP2040)
        bool sendWsStatus(const WsStatusPayload &payload);

        /// Отправить сброс привязанных клиентов (RP2040 -> ESP32)
        bool sendWsResetClients();

        /// Запросить статус WS (RP2040 -> ESP32)
        bool sendWsStatusRequest();

        // Регистрация callbacks.

        void setHelloHandler(const HelloHandler &handler);
        void setHelloAckHandler(const HelloAckHandler &handler);
        void setTelemetryHandler(const TelemetryHandler &handler);
        void setCommandHandler(const CommandHandler &handler);
        void setProfileHandler(const ProfileHandler &handler);
        void setConfigHandler(const ConfigHandler &handler);
        void setConfigPushChunkHandler(const ConfigPushChunkHandler &handler);
        void setCommandAckHandler(const CommandAckHandler &handler);
        void setConfigAckHandler(const ConfigAckHandler &handler);
        void setHeartbeatHandler(const HeartbeatHandler &handler);
        void setErrorHandler(const ErrorHandler &handler);
        void setLogHandler(const LogHandler &handler);
        void setWeightsHandler(const WeightsHandler &handler);
        void setStatusHandler(const StatusHandler &handler);
        void setRfidHandler(const RfidHandler &handler);
        void setRfidDataHandler(const RfidDataHandler &handler);
        void setClaimStartHandler(const ClaimStartHandler &handler);
        void setClaimStatusHandler(const ClaimStatusHandler &handler);
        void setClaimCompleteHandler(const ClaimCompleteHandler &handler);
        void setWsEnableHandler(const WsEnableHandler &handler);
        void setWsStatusHandler(const WsStatusHandler &handler);
        void setWsResetClientsHandler(const WsResetClientsHandler &handler);
        void setWsStatusRequestHandler(const WsStatusRequestHandler &handler);

    private:
        // Парсер входящих данных.

        /// Состояния конечного автомата парсера
        enum class ParserState : uint8_t
        {
            WaitForSof, // Ждём стартовый байт 0xAA
            Header,     // Читаем заголовок (6 байт)
            Payload,    // Читаем payload (0-200 байт)
            Crc         // Читаем CRC16 (2 байта)
        };

        /// Контекст парсера (текущее состояние разбора)
        struct ParserContext
        {
            ParserState state = ParserState::WaitForSof;
            Frame frame{}; // Собираемый фрейм
            uint8_t headerIndex = 0;
            uint8_t payloadIndex = 0;
            uint8_t crcIndex = 0;
        };

        /// Фрейм, ожидающий ACK (для retry)
        struct PendingFrame
        {
            Frame frame{};
            bool active = false;
            uint8_t retries = 0;
            uint32_t lastSendAt = 0;
        };

        // Внутренние методы.

        /// Обработка одного входящего байта
        void processIncomingByte(uint8_t byte);

        /// Обработка готового фрейма (после проверки CRC)
        void handleFrame(const Frame &frame);

        /// Обработка ACK фрейма
        void handleAckFrame(const Frame &frame);

        /// Отправка фрейма в UART
        bool transmit(MessageKind kind, const uint8_t *payload, uint8_t length,
                      uint8_t flags, int forcedSequence = -1);

        /// Отправка ACK фрейма
        bool sendAckFrame(MessageKind kind, uint8_t sequence, ErrorCode status);

        /// Проверка и повторная отправка pending фрейма
        void resendPendingIfNeeded();

        /// Валидация длины payload для типа сообщения
        bool validateLength(MessageKind kind, uint8_t length) const;

        /// Генерация и отправка ошибки
        void emitError(ErrorCode code, uint8_t sequence, uint16_t detail, bool remote);

        /// Сброс парсера в начальное состояние
        void resetParser();

        /// Получить следующий номер sequence
        uint8_t nextSequence();

        // Данные.

        idryer::hal::ISerial *serial_ = nullptr; // HAL интерфейс UART
        uint32_t baudRate_ = 0;
        ParserContext parser_{};
        PendingFrame pending_{};
        uint8_t sequenceCounter_ = 0;

        // Callbacks
        HelloHandler helloHandler_;
        HelloAckHandler helloAckHandler_;
        TelemetryHandler telemetryHandler_;
        CommandHandler commandHandler_;
        ProfileHandler profileHandler_;
        ConfigHandler configHandler_;
        ConfigPushChunkHandler configPushChunkHandler_;
        CommandAckHandler commandAckHandler_;
        ConfigAckHandler configAckHandler_;
        HeartbeatHandler heartbeatHandler_;
        ErrorHandler errorHandler_;
        LogHandler logHandler_;
        WeightsHandler weightsHandler_;
        StatusHandler statusHandler_;
        RfidHandler rfidHandler_;
        RfidDataHandler rfidDataHandler_;
        ClaimStartHandler claimStartHandler_;
        ClaimStatusHandler claimStatusHandler_;
        ClaimCompleteHandler claimCompleteHandler_;
        WsEnableHandler wsEnableHandler_;
        WsStatusHandler wsStatusHandler_;
        WsResetClientsHandler wsResetClientsHandler_;
        WsStatusRequestHandler wsStatusRequestHandler_;
    };

} // namespace DryerUart
