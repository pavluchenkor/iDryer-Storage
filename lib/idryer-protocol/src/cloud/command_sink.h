/**
 * @file command_sink.h
 * @brief Абстракция «куда девать разобранную MQTT-команду».
 *
 * На iDryer реализация (UartCommandSink) шлёт payload в UartBridge,
 * далее по UART в RP2040. На устройствах без MCU (iHeater Link II,
 * модуль телеметрии и т.п.) потребитель библиотеки подставляет свою
 * реализацию и применяет команду локально.
 */

#pragma once

#include "../uart/uart_protocol.h" // CommandPayload, ProfilePayload, ConfigChunkPayload

namespace idryer
{
    namespace cloud
    {

        class ICommandSink
        {
        public:
            virtual ~ICommandSink() = default;

            /// Стандартные команды: start / stop / storage / find / set / invoke /
            /// get_config / read_rfid / write_rfid / clear_errors.
            virtual void sendCommand(const DryerUart::CommandPayload &payload,
                                     bool ackRequired) = 0;

            /// Команда применения профиля (многоэтапная программа сушки).
            virtual void sendProfileCommand(const DryerUart::ProfilePayload &payload,
                                            bool ackRequired) = 0;

            /// Чанк JSON-конфигурации (config push).
            /// @param dataLen размер полезных данных в payload.data[] (без заголовка).
            /// @param flags    протокольные флаги UART (FLAG_FRAGMENTED / FLAG_LAST_FRAGMENT).
            virtual void sendConfigPushChunk(const DryerUart::ConfigChunkPayload &payload,
                                             uint8_t dataLen,
                                             uint8_t flags) = 0;

            /// Фрагмент данных для записи на RFID-метку (RfidWriteData, 0x1B).
            /// Вызывается после того, как `sendCommand(WriteRfid, ...)` армирует staging на MCU.
            /// @param flags протокольные флаги UART (FLAG_FRAGMENTED / FLAG_LAST_FRAGMENT).
            /// Базовая реализация — no-op (для sink'ов без UART/MCU).
            virtual void sendRfidWriteData(const DryerUart::RfidDataPayload & /*payload*/,
                                           uint8_t /*flags*/) {}

            /// Блокирующее ожидание ACK на последний отправленный frame с FLAG_ACK_REQUIRED.
            /// Используется для stop-and-wait flow control на фрагментированных transfer'ах
            /// (WriteRfid). Default-реализация для sink'ов без UART/pending-механики — возвращает
            /// true (считаем что ACK не нужен).
            /// @return true если ACK получен, false при таймауте.
            virtual bool waitForAck(uint32_t /*timeoutMs*/) { return true; }
        };

    } // namespace cloud
} // namespace idryer
