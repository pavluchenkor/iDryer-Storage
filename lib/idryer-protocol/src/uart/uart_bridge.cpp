/**
 * @file uart_bridge.cpp
 * @brief Реализация UART моста
 *
 * Этот файл содержит реализацию UartBridge - главного класса
 * для обмена сообщениями между ESP32 (Link) и RP2040 (Controller).
 */

#include "uart_bridge.h"
#include <string.h>
#include <cstdio>

namespace DryerUart
{

    // =============================================================================
    // КОНСТАНТЫ
    // =============================================================================

    namespace
    {
        /// Размер заголовка фрейма в байтах
        constexpr uint8_t HEADER_SIZE = sizeof(FrameHeader);
    }

    // =============================================================================
    // ИНИЦИАЛИЗАЦИЯ
    // =============================================================================

    void UartBridge::begin(idryer::hal::ISerial *serial, uint32_t baudRate)
    {
        serial_ = serial;
        baudRate_ = baudRate;

        serial_->begin(baudRate_);

        // Сбрасываем состояние
        reset();
    }

    void UartBridge::reset()
    {
        resetParser();
        pending_.active = false;
        pending_.retries = 0;
        pending_.lastSendAt = 0;
        sequenceCounter_ = 0;
    }

    // =============================================================================
    // ГЛАВНЫЙ ЦИКЛ
    // =============================================================================

    void UartBridge::loop()
    {
        // Проверяем что serial инициализирован
        if (!serial_)
        {
            return;
        }

        // Читаем и обрабатываем все доступные байты
        while (serial_->available() > 0)
        {
            const int byteOrError = serial_->read();
            if (byteOrError >= 0)
            {
                processIncomingByte(static_cast<uint8_t>(byteOrError));
            }
        }

        // Проверяем необходимость retry для pending фрейма
        resendPendingIfNeeded();
    }

    // =============================================================================
    // ОТПРАВКА СООБЩЕНИЙ
    // =============================================================================

    bool UartBridge::sendHello(const HelloPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Hello,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendHelloAck(const HelloAckPayload &payload)
    {
        return transmit(MessageKind::HelloAck,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), FLAG_IS_ACK);
    }

    bool UartBridge::sendTelemetry(const TelemetryPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Telemetry,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendStatus(const StatusPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Status,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendWeights(const WeightsPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Weights,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendRfid(const RfidPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Rfid,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendCommand(const CommandPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Command,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendConfigPush(const ConfigPayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::ConfigPush,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    bool UartBridge::sendConfigPushChunk(const ConfigChunkPayload &payload, uint8_t payloadLen, uint8_t protocolFlags)
    {
        return transmit(MessageKind::ConfigPush,
                        reinterpret_cast<const uint8_t *>(&payload),
                        payloadLen, protocolFlags);
    }

    bool UartBridge::sendHeartbeat(const HeartbeatPayload &payload)
    {
        return transmit(MessageKind::Heartbeat,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), 0);
    }

    bool UartBridge::sendError(const ErrorPayload &payload)
    {
        return transmit(MessageKind::Error,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), FLAG_ERROR);
    }

    bool UartBridge::sendTelemetryAck(uint8_t sequence, ErrorCode status)
    {
        return sendAckFrame(MessageKind::TelemetryAck, sequence, status);
    }

    bool UartBridge::sendCommandAck(uint8_t sequence, ErrorCode status)
    {
        return sendAckFrame(MessageKind::CommandAck, sequence, status);
    }

    bool UartBridge::sendConfigAck(uint8_t sequence, ErrorCode status)
    {
        return sendAckFrame(MessageKind::ConfigAck, sequence, status);
    }

    bool UartBridge::sendLog(const uint8_t *message, uint8_t length)
    {
        if (!message || length > MAX_PAYLOAD_SIZE)
        {
            return false;
        }
        return transmit(MessageKind::Log, message, length, 0);
    }

    bool UartBridge::sendLog(const char *cstr)
    {
        if (!cstr)
        {
            return false;
        }
        size_t len = strlen(cstr);
        if (len > MAX_PAYLOAD_SIZE)
        {
            len = MAX_PAYLOAD_SIZE;
        }
        return sendLog(reinterpret_cast<const uint8_t *>(cstr), static_cast<uint8_t>(len));
    }

    // =============================================================================
    // =============================================================================
    // RFID DATA
    // =============================================================================

    bool UartBridge::sendRfidWriteData(const RfidDataPayload &payload, uint8_t protocolFlags)
    {
        return transmit(MessageKind::RfidWriteData,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), protocolFlags);
    }

    bool UartBridge::sendRfidReadData(const RfidDataPayload &payload, uint8_t protocolFlags)
    {
        return transmit(MessageKind::RfidReadData,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), protocolFlags);
    }

    // =============================================================================
    // PROFILE DRYING
    // =============================================================================

    bool UartBridge::sendProfileCommand(const ProfilePayload &payload, bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::Command,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), flags);
    }

    // =============================================================================
    // DEVICE CLAIMING (новый протокол)
    // =============================================================================

    bool UartBridge::sendClaimStart(bool ackRequired)
    {
        const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
        return transmit(MessageKind::ClaimStart, nullptr, 0, flags);
    }

    bool UartBridge::sendClaimStatus(const ClaimStatusPayload &payload)
    {
        return transmit(MessageKind::ClaimStatus,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), 0);
    }

    bool UartBridge::sendClaimComplete(const ClaimCompletePayload &payload)
    {
        return transmit(MessageKind::ClaimComplete,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), 0);
    }

    // =============================================================================
    // WEBSOCKET LOCAL ACCESS (протокол 0x73-0x76)
    // =============================================================================

    bool UartBridge::sendWsEnable(const WsEnablePayload &payload)
    {
        return transmit(MessageKind::WsEnable,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), 0);
    }

    bool UartBridge::sendWsStatus(const WsStatusPayload &payload)
    {
        return transmit(MessageKind::WsStatus,
                        reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), 0);
    }

    bool UartBridge::sendWsResetClients()
    {
        return transmit(MessageKind::WsResetClients, nullptr, 0, 0);
    }

    bool UartBridge::sendWsStatusRequest()
    {
        return transmit(MessageKind::WsStatusRequest, nullptr, 0, 0);
    }

    // =============================================================================
    // УСТАНОВКА CALLBACK'ОВ
    // =============================================================================

    void UartBridge::setHelloHandler(const HelloHandler &handler)
    {
        helloHandler_ = handler;
    }

    void UartBridge::setHelloAckHandler(const HelloAckHandler &handler)
    {
        helloAckHandler_ = handler;
    }

    void UartBridge::setTelemetryHandler(const TelemetryHandler &handler)
    {
        telemetryHandler_ = handler;
    }

    void UartBridge::setCommandHandler(const CommandHandler &handler)
    {
        commandHandler_ = handler;
    }

    void UartBridge::setProfileHandler(const ProfileHandler &handler)
    {
        profileHandler_ = handler;
    }

    void UartBridge::setConfigHandler(const ConfigHandler &handler)
    {
        configHandler_ = handler;
    }

    void UartBridge::setConfigPushChunkHandler(const ConfigPushChunkHandler &handler)
    {
        configPushChunkHandler_ = handler;
    }

    void UartBridge::setCommandAckHandler(const CommandAckHandler &handler)
    {
        commandAckHandler_ = handler;
    }

    void UartBridge::setConfigAckHandler(const ConfigAckHandler &handler)
    {
        configAckHandler_ = handler;
    }

    void UartBridge::setHeartbeatHandler(const HeartbeatHandler &handler)
    {
        heartbeatHandler_ = handler;
    }

    void UartBridge::setErrorHandler(const ErrorHandler &handler)
    {
        errorHandler_ = handler;
    }

    void UartBridge::setLogHandler(const LogHandler &handler)
    {
        logHandler_ = handler;
    }

    void UartBridge::setWeightsHandler(const WeightsHandler &handler)
    {
        weightsHandler_ = handler;
    }

    void UartBridge::setStatusHandler(const StatusHandler &handler)
    {
        statusHandler_ = handler;
    }

    void UartBridge::setRfidHandler(const RfidHandler &handler)
    {
        rfidHandler_ = handler;
    }

    void UartBridge::setRfidDataHandler(const RfidDataHandler &handler)
    {
        rfidDataHandler_ = handler;
    }

    void UartBridge::setClaimStartHandler(const ClaimStartHandler &handler)
    {
        claimStartHandler_ = handler;
    }

    void UartBridge::setClaimStatusHandler(const ClaimStatusHandler &handler)
    {
        claimStatusHandler_ = handler;
    }

    void UartBridge::setClaimCompleteHandler(const ClaimCompleteHandler &handler)
    {
        claimCompleteHandler_ = handler;
    }

    void UartBridge::setWsEnableHandler(const WsEnableHandler &handler)
    {
        wsEnableHandler_ = handler;
    }

    void UartBridge::setWsStatusHandler(const WsStatusHandler &handler)
    {
        wsStatusHandler_ = handler;
    }

    void UartBridge::setWsResetClientsHandler(const WsResetClientsHandler &handler)
    {
        wsResetClientsHandler_ = handler;
    }

    void UartBridge::setWsStatusRequestHandler(const WsStatusRequestHandler &handler)
    {
        wsStatusRequestHandler_ = handler;
    }

    // =============================================================================
    // ПАРСЕР ВХОДЯЩИХ ДАННЫХ
    // =============================================================================

    void UartBridge::processIncomingByte(uint8_t byte)
    {
        switch (parser_.state)
        {
        // -----------------------------------------------------------------
        // Ждём стартовый байт (SOF = 0xAA)
        // -----------------------------------------------------------------
        case ParserState::WaitForSof:
            if (byte == SOF)
            {
                parser_.frame.header.sof = byte;
                parser_.headerIndex = 1;
                parser_.state = ParserState::Header;
            }
            break;

        // -----------------------------------------------------------------
        // Читаем заголовок (6 байт)
        // -----------------------------------------------------------------
        case ParserState::Header:
        {
            uint8_t *raw = reinterpret_cast<uint8_t *>(&parser_.frame.header);
            raw[parser_.headerIndex++] = byte;

            if (parser_.headerIndex >= HEADER_SIZE)
            {
                // Проверяем длину payload
                if (parser_.frame.header.payloadLength > MAX_PAYLOAD_SIZE)
                {
                    emitError(ErrorCode::InvalidPayload, parser_.frame.header.sequence,
                              parser_.frame.header.payloadLength, false);
                    resetParser();
                    break;
                }

                parser_.payloadIndex = 0;
                // Если payload есть - читаем его, иначе сразу CRC
                parser_.state = (parser_.frame.header.payloadLength > 0)
                                    ? ParserState::Payload
                                    : ParserState::Crc;
            }
            break;
        }

        // -----------------------------------------------------------------
        // Читаем payload (0-200 байт)
        // -----------------------------------------------------------------
        case ParserState::Payload:
            parser_.frame.payload[parser_.payloadIndex++] = byte;
            if (parser_.payloadIndex >= parser_.frame.header.payloadLength)
            {
                // КРИТИЧНО: НЕ логируем здесь - тормозит чтение из RX FIFO!
                // Лог только при ошибках (см. CRC mismatch)
                parser_.state = ParserState::Crc;
                parser_.crcIndex = 0;
            }
            break;

        // -----------------------------------------------------------------
        // Читаем CRC16 (2 байта, little-endian)
        // -----------------------------------------------------------------
        case ParserState::Crc:
            if (parser_.crcIndex == 0)
            {
                // Младший байт CRC
                parser_.frame.crc = byte;
                parser_.crcIndex = 1;
            }
            else
            {
                // Старший байт CRC
                parser_.frame.crc |= static_cast<uint16_t>(byte) << 8;

                // Фрейм собран - проверяем CRC
                Frame frame = parser_.frame;
                resetParser();

                // Собираем данные для расчёта CRC
                uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
                memcpy(buffer, &frame.header, HEADER_SIZE);
                if (frame.header.payloadLength > 0)
                {
                    memcpy(buffer + HEADER_SIZE, frame.payload, frame.header.payloadLength);
                }

                // Расчёт CRC
                const uint16_t computed = calculateCrc(buffer, HEADER_SIZE + frame.header.payloadLength);

                if (computed != frame.crc)
                {
                    // CRC не совпал - простой лог (hex dump тормозит RX!)
                    HAL_LOG_ERROR("UART", "CRC MISMATCH: seq=%u kind=0x%02X len=%u exp=0x%04X got=0x%04X",
                                 frame.header.sequence, static_cast<uint8_t>(frame.header.kind),
                                 frame.header.payloadLength, computed, frame.crc);

                    emitError(ErrorCode::CrcMismatch, frame.header.sequence,
                              frame.header.payloadLength, false);
                    break;
                }

                // CRC OK - обрабатываем фрейм
                handleFrame(frame);
            }
            break;
        }
    }

    // =============================================================================
    // ОБРАБОТКА ГОТОВОГО ФРЕЙМА
    // =============================================================================

    void UartBridge::handleFrame(const Frame &frame)
    {
        // Проверяем версию протокола
        if (frame.header.version != PROTOCOL_VERSION)
        {
            emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                      frame.header.version, false);
            return;
        }

        // Обрабатываем по типу сообщения
        switch (frame.header.kind)
        {
        // -----------------------------------------------------------------
        // Телеметрия от RP2040
        // -----------------------------------------------------------------
        case MessageKind::Telemetry:
        {
            if (!validateLength(MessageKind::Telemetry, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            TelemetryPayload payload{};
            memcpy(&payload, frame.payload, sizeof(payload));
            if (telemetryHandler_)
            {
                telemetryHandler_(payload, frame.header);
            }
            // Отправляем ACK если требуется
            if ((frame.header.flags & FLAG_ACK_REQUIRED) != 0)
            {
                sendTelemetryAck(frame.header.sequence);
            }
            break;
        }

        // -----------------------------------------------------------------
        // Веса филамента
        // -----------------------------------------------------------------
        case MessageKind::Weights:
        {
            if (!validateLength(MessageKind::Weights, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (weightsHandler_)
            {
                WeightsPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                weightsHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // Статус режима работы
        // -----------------------------------------------------------------
        case MessageKind::Status:
        {
            if (!validateLength(MessageKind::Status, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (statusHandler_)
            {
                StatusPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                statusHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // RFID события
        // -----------------------------------------------------------------
        case MessageKind::Rfid:
        {
            if (!validateLength(MessageKind::Rfid, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (rfidHandler_)
            {
                RfidPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                rfidHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // RFID данные (чтение/запись)
        // -----------------------------------------------------------------
        case MessageKind::RfidReadData:
        case MessageKind::RfidWriteData:
        {
            if (!validateLength(frame.header.kind, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (rfidDataHandler_)
            {
                RfidDataPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                rfidDataHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // Hello / HelloAck
        // -----------------------------------------------------------------
        case MessageKind::Hello:
        {
            HAL_LOG_DEBUG("UART", "Hello: expected=%u received=%u",
                         sizeof(HelloPayload), frame.header.payloadLength);

            if (!validateLength(MessageKind::Hello, frame.header.payloadLength))
            {
                HAL_LOG_ERROR("UART", "Hello size mismatch! expected=%u received=%u",
                             sizeof(HelloPayload), frame.header.payloadLength);
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            HelloPayload payload{};
            memcpy(&payload, frame.payload, frame.header.payloadLength);
            if (helloHandler_)
            {
                helloHandler_(payload, frame.header);
            }
            break;
        }

        case MessageKind::HelloAck:
        {
            HAL_LOG_DEBUG("UART", "HelloAck: expected=%u received=%u",
                         sizeof(HelloAckPayload), frame.header.payloadLength);

            if (!validateLength(MessageKind::HelloAck, frame.header.payloadLength))
            {
                HAL_LOG_ERROR("UART", "HelloAck size mismatch! expected=%u received=%u",
                             sizeof(HelloAckPayload), frame.header.payloadLength);
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            HelloAckPayload ackPayload{};
            memcpy(&ackPayload, frame.payload, frame.header.payloadLength);
            if (helloAckHandler_)
            {
                helloAckHandler_(ackPayload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // ACK фреймы
        // -----------------------------------------------------------------
        case MessageKind::TelemetryAck:
        case MessageKind::CommandAck:
        case MessageKind::ConfigAck:
            handleAckFrame(frame);
            break;

        // -----------------------------------------------------------------
        // Команда от ESP32
        // -----------------------------------------------------------------
        case MessageKind::Command:
        {
            if (!validateLength(MessageKind::Command, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }

            // Проверяем тип команды по размеру payload
            if (frame.header.payloadLength == sizeof(ProfilePayload))
            {
                // Profile команда
                ProfilePayload profilePayload{};
                memcpy(&profilePayload, frame.payload, sizeof(profilePayload));
                if (profileHandler_)
                {
                    profileHandler_(profilePayload, frame.header);
                }
            }
            else
            {
                // Обычная команда
                CommandPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                if (commandHandler_)
                {
                    commandHandler_(payload, frame.header);
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // Конфигурация (фрагментированный JSON или legacy ConfigPayload)
        // -----------------------------------------------------------------
        case MessageKind::ConfigPush:
        {
            // Проверяем: это фрагментированный конфиг?
            if (frame.header.flags & (FLAG_FRAGMENTED | FLAG_LAST_FRAGMENT))
            {
                // Фрагментированный JSON конфиг
                if (frame.header.payloadLength < CONFIG_CHUNK_HEADER_SIZE)
                {
                    emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                              frame.header.payloadLength, false);
                    return;
                }
                if (configPushChunkHandler_)
                {
                    ConfigChunkPayload payload{};
                    memcpy(&payload, frame.payload, frame.header.payloadLength);
                    uint8_t dataLen = frame.header.payloadLength - CONFIG_CHUNK_HEADER_SIZE;
                    configPushChunkHandler_(payload, dataLen, frame.header);
                }
                // Отправляем ACK если требуется
                if ((frame.header.flags & FLAG_ACK_REQUIRED) != 0)
                {
                    sendConfigAck(frame.header.sequence);
                }
            }
            else
            {
                // Legacy ConfigPayload
                if (!validateLength(MessageKind::ConfigPush, frame.header.payloadLength))
                {
                    emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                              frame.header.payloadLength, false);
                    return;
                }
                ConfigPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                if (configHandler_)
                {
                    configHandler_(payload, frame.header);
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // Heartbeat
        // -----------------------------------------------------------------
        case MessageKind::Heartbeat:
        {
            if (!validateLength(MessageKind::Heartbeat, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            HeartbeatPayload payload{};
            memcpy(&payload, frame.payload, sizeof(payload));
            if (heartbeatHandler_)
            {
                heartbeatHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // Ошибка от другой стороны
        // -----------------------------------------------------------------
        case MessageKind::Error:
        {
            if (!validateLength(MessageKind::Error, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            ErrorPayload payload{};
            memcpy(&payload, frame.payload, sizeof(payload));
            emitError(payload.code, payload.lastSequence, payload.detail, true);
            break;
        }

        // -----------------------------------------------------------------
        // Лог
        // -----------------------------------------------------------------
        case MessageKind::Log:
            if (logHandler_)
            {
                logHandler_(frame.payload, frame.header.payloadLength);
            }
            break;

        // -----------------------------------------------------------------
        // Device Claiming (новый протокол)
        // -----------------------------------------------------------------
        case MessageKind::ClaimStart:
            // ClaimStart имеет пустой payload
            if (claimStartHandler_)
            {
                claimStartHandler_(frame.header);
            }
            break;

        case MessageKind::ClaimStatus:
        {
            if (!validateLength(MessageKind::ClaimStatus, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (claimStatusHandler_)
            {
                ClaimStatusPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                claimStatusHandler_(payload, frame.header);
            }
            break;
        }

        case MessageKind::ClaimComplete:
        {
            if (!validateLength(MessageKind::ClaimComplete, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (claimCompleteHandler_)
            {
                ClaimCompletePayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                claimCompleteHandler_(payload, frame.header);
            }
            break;
        }

        // -----------------------------------------------------------------
        // WebSocket Local Access
        // -----------------------------------------------------------------
        case MessageKind::WsEnable:
        {
            HAL_LOG_INFO("UART", "Frame WsEnable: seq=%u len=%u flags=0x%02X",
                         frame.header.sequence,
                         frame.header.payloadLength,
                         frame.header.flags);
            if (!validateLength(MessageKind::WsEnable, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (wsEnableHandler_)
            {
                WsEnablePayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                HAL_LOG_INFO("UART", "Dispatch WsEnable: seq=%u enable=%u",
                             frame.header.sequence,
                             static_cast<unsigned>(payload.enable));
                wsEnableHandler_(payload, frame.header);
            }
            else
            {
                HAL_LOG_WARN("UART", "WsEnable dropped: handler is null");
            }
            break;
        }

        case MessageKind::WsStatus:
        {
            if (!validateLength(MessageKind::WsStatus, frame.header.payloadLength))
            {
                emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false);
                return;
            }
            if (wsStatusHandler_)
            {
                WsStatusPayload payload{};
                memcpy(&payload, frame.payload, sizeof(payload));
                wsStatusHandler_(payload, frame.header);
            }
            break;
        }

        case MessageKind::WsResetClients:
            if (wsResetClientsHandler_)
            {
                wsResetClientsHandler_(frame.header);
            }
            break;

        case MessageKind::WsStatusRequest:
            HAL_LOG_INFO("UART", "Frame WsStatusRequest: seq=%u", frame.header.sequence);
            if (wsStatusRequestHandler_)
            {
                wsStatusRequestHandler_(frame.header);
            }
            else
            {
                HAL_LOG_WARN("UART", "WsStatusRequest dropped: handler is null");
            }
            break;

        // -----------------------------------------------------------------
        // Неизвестный тип сообщения
        // -----------------------------------------------------------------
        default:
            emitError(ErrorCode::UnknownMessage, frame.header.sequence,
                      static_cast<uint16_t>(frame.header.kind), false);
            break;
        }
    }

    // =============================================================================
    // ОБРАБОТКА ACK ФРЕЙМОВ
    // =============================================================================

    void UartBridge::handleAckFrame(const Frame &frame)
    {
        if (!validateLength(frame.header.kind, frame.header.payloadLength))
        {
            emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false);
            return;
        }

        AckPayload payload{};
        memcpy(&payload, frame.payload, sizeof(payload));

        // Если это ACK на наш pending фрейм - снимаем его с очереди
        if (pending_.active && pending_.frame.header.sequence == payload.ackSequence)
        {
            pending_.active = false;
        }

        // Вызываем callback в зависимости от типа ACK
        switch (frame.header.kind)
        {
        case MessageKind::TelemetryAck:
            // TelemetryAck обычно не требует обработки
            break;
        case MessageKind::CommandAck:
            if (commandAckHandler_)
            {
                commandAckHandler_(payload, frame.header);
            }
            break;
        case MessageKind::ConfigAck:
            if (configAckHandler_)
            {
                configAckHandler_(payload, frame.header);
            }
            break;
        default:
            break;
        }
    }

    // =============================================================================
    // ОТПРАВКА ФРЕЙМОВ
    // =============================================================================

    bool UartBridge::transmit(MessageKind kind, const uint8_t *payload,
                              uint8_t length, uint8_t flags, int forcedSequence)
    {
        // Проверки
        if (!serial_ || length > MAX_PAYLOAD_SIZE)
        {
            return false;
        }

        // Определяем нужно ли отслеживать ACK
        const bool trackPending = (flags & FLAG_ACK_REQUIRED) != 0;

        // Собираем фрейм
        Frame frame{};
        frame.header.sof = SOF;
        frame.header.version = PROTOCOL_VERSION;
        frame.header.flags = flags;
        frame.header.kind = kind;
        frame.header.sequence = (forcedSequence >= 0)
                                    ? static_cast<uint8_t>(forcedSequence)
                                    : nextSequence();
        frame.header.payloadLength = length;

        if (length > 0 && payload)
        {
            memcpy(frame.payload, payload, length);
        }

        // Расчёт CRC
        uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
        memcpy(buffer, &frame.header, HEADER_SIZE);
        if (length > 0 && payload)
        {
            memcpy(buffer + HEADER_SIZE, frame.payload, length);
        }
        frame.crc = calculateCrc(buffer, HEADER_SIZE + length);

        // Легкое логирование (без hex dump - он тормозит)
        HAL_LOG_DEBUG("UART", "TX: seq=%u kind=0x%02X len=%u crc=0x%04X",
                     frame.header.sequence, static_cast<uint8_t>(frame.header.kind),
                     frame.header.payloadLength, frame.crc);

        // Отправка: заголовок + payload + CRC
        size_t written = serial_->write(
            reinterpret_cast<uint8_t *>(&frame.header), HEADER_SIZE);

        if (length > 0)
        {
            written += serial_->write(frame.payload, length);
        }

        uint8_t crcBytes[2] = {
            static_cast<uint8_t>(frame.crc & 0xFF),
            static_cast<uint8_t>((frame.crc >> 8) & 0xFF)};
        written += serial_->write(crcBytes, sizeof(crcBytes));

        // КРИТИЧЕСКИ ВАЖНО: Ждём пока данные реально уйдут в UART
        // На ESP32 flush() вызывает uart_wait_tx_done() для ожидания опустошения hardware FIFO
        serial_->flush();

        // WORKAROUND: Дополнительная пауза для опустошения RX FIFO на принимающей стороне
        // Консультант: "RX FIFO на RP2040 переполняется, нужна межкадровая задержка"
        HAL_DELAY_MS(2);  // 2мс пауза между фреймами

        // Проверяем что всё отправлено (только при ошибке)
        size_t expected = HEADER_SIZE + length + sizeof(uint16_t);
        if (written != expected)
        {
            HAL_LOG_ERROR("UART", "TX INCOMPLETE! seq=%u written=%zu expected=%zu",
                         frame.header.sequence, written, expected);
            return false;
        }

        // Если требуется ACK - сохраняем для retry
        if (trackPending)
        {
            pending_.frame = frame;
            pending_.active = true;
            pending_.retries = 0;
            pending_.lastSendAt = HAL_MILLIS();
        }

        return true;
    }

    bool UartBridge::sendAckFrame(MessageKind kind, uint8_t sequence, ErrorCode status)
    {
        AckPayload payload{};
        payload.ackSequence = sequence;  // Номер фрейма, на который отвечаем
        payload.status = status;
        // НЕ передаём forcedSequence - пусть ACK получит новый уникальный номер
        return transmit(kind, reinterpret_cast<const uint8_t *>(&payload),
                        sizeof(payload), FLAG_IS_ACK);
    }

    bool UartBridge::waitForAck(uint32_t timeoutMs)
    {
        const uint32_t start = HAL_MILLIS();
        while (pending_.active && (HAL_MILLIS() - start) < timeoutMs)
        {
            loop();          // обрабатывает RX (ACK приходит здесь) и retry по таймауту
            HAL_DELAY_MS(1); // yield: на ESP32 delay() = vTaskDelay(pdMS_TO_TICKS(1))
        }
        return !pending_.active;
    }

    // =============================================================================
    // RETRY МЕХАНИЗМ
    // =============================================================================

    void UartBridge::resendPendingIfNeeded()
    {
        // Если нет pending фрейма - выходим
        if (!pending_.active || !serial_)
        {
            return;
        }

        // Проверяем таймаут
        const uint32_t now = HAL_MILLIS();
        if (now - pending_.lastSendAt < COMMAND_REPLY_TIMEOUT_MS)
        {
            return;
        }

        // Превышено количество попыток?
        if (pending_.retries + 1 >= MAX_RETRIES)
        {
            pending_.active = false;
            emitError(ErrorCode::Timeout, pending_.frame.header.sequence,
                      pending_.retries, false);
            return;
        }

        // Повторная отправка
        pending_.retries++;
        pending_.lastSendAt = now;

        size_t written = serial_->write(
            reinterpret_cast<uint8_t *>(&pending_.frame.header), HEADER_SIZE);

        if (pending_.frame.header.payloadLength > 0)
        {
            written += serial_->write(pending_.frame.payload,
                                      pending_.frame.header.payloadLength);
        }

        uint8_t crcBytes[2] = {
            static_cast<uint8_t>(pending_.frame.crc & 0xFF),
            static_cast<uint8_t>((pending_.frame.crc >> 8) & 0xFF)};
        written += serial_->write(crcBytes, sizeof(crcBytes));

        // Ждём завершения передачи (на ESP32 вызовет uart_wait_tx_done)
        serial_->flush();

        // Логирование через HAL (если доступно)
        HAL_LOG_DEBUG("UART", "Resend seq=%u attempt=%u written=%zu",
                      pending_.frame.header.sequence, pending_.retries, written);
    }

    // =============================================================================
    // ВАЛИДАЦИЯ ДЛИНЫ PAYLOAD
    // =============================================================================

    bool UartBridge::validateLength(MessageKind kind, uint8_t length) const
    {
        switch (kind)
        {
        case MessageKind::Hello:
            return length == sizeof(HelloPayload);
        case MessageKind::HelloAck:
            return length == sizeof(HelloAckPayload);
        case MessageKind::Telemetry:
            return length == sizeof(TelemetryPayload);
        case MessageKind::Weights:
            return length == sizeof(WeightsPayload);
        case MessageKind::Status:
            return length == sizeof(StatusPayload);
        case MessageKind::Rfid:
            return length == sizeof(RfidPayload);
        case MessageKind::RfidReadData:
        case MessageKind::RfidWriteData:
            return length == sizeof(RfidDataPayload);
        case MessageKind::Command:
            // Поддерживаем как обычные команды (CommandPayload), так и Profile команды (ProfilePayload)
            return length == sizeof(CommandPayload) || length == sizeof(ProfilePayload);
        case MessageKind::ConfigPush:
            return length == sizeof(ConfigPayload);
        case MessageKind::Heartbeat:
            return length == sizeof(HeartbeatPayload);
        case MessageKind::TelemetryAck:
        case MessageKind::CommandAck:
        case MessageKind::ConfigAck:
            return length == sizeof(AckPayload);
        case MessageKind::Error:
            return length == sizeof(ErrorPayload);
        case MessageKind::Log:
            return length <= MAX_PAYLOAD_SIZE;
        case MessageKind::ClaimStart:
            return length == 0; // Пустой payload
        case MessageKind::ClaimStatus:
            return length == sizeof(ClaimStatusPayload);
        case MessageKind::ClaimComplete:
            return length == sizeof(ClaimCompletePayload);
        case MessageKind::WsEnable:
            return length == sizeof(WsEnablePayload);
        case MessageKind::WsStatus:
            return length == sizeof(WsStatusPayload);
        case MessageKind::WsResetClients:
        case MessageKind::WsStatusRequest:
            return length == 0;
        default:
            return length <= MAX_PAYLOAD_SIZE;
        }
    }

    // =============================================================================
    // ОБРАБОТКА ОШИБОК
    // =============================================================================

    void UartBridge::emitError(ErrorCode code, uint8_t sequence, uint16_t detail, bool remote)
    {
        ErrorPayload payload{};
        payload.code = code;
        payload.lastSequence = sequence;
        payload.detail = detail;

        // Если ошибка локальная - отправляем на другую сторону
        if (!remote)
        {
            sendError(payload);
        }

        // Вызываем callback
        if (errorHandler_)
        {
            errorHandler_(payload, remote);
        }
    }

    // =============================================================================
    // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    // =============================================================================

    void UartBridge::resetParser()
    {
        parser_.state = ParserState::WaitForSof;
        parser_.headerIndex = 0;
        parser_.payloadIndex = 0;
        parser_.crcIndex = 0;
    }

    uint8_t UartBridge::nextSequence()
    {
        return sequenceCounter_++;
    }

} // namespace DryerUart
