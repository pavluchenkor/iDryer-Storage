/**
 * @file uart_command_sink.h
 * @brief Реализация ICommandSink, транслирующая команды в UART (UartBridge).
 *
 * Используется на устройствах с UART-линком к MCU (iDryer, iHeater через
 * UART-дочерний контроллер и т.п.).
 */

#pragma once

#include "command_sink.h"
#include "../uart/uart_bridge.h"

namespace idryer
{
    namespace cloud
    {

        class UartCommandSink : public ICommandSink
        {
        public:
            explicit UartCommandSink(DryerUart::UartBridge *uart) : uart_(uart) {}

            void sendCommand(const DryerUart::CommandPayload &p, bool ack) override
            {
                if (uart_)
                    uart_->sendCommand(p, ack);
            }

            void sendProfileCommand(const DryerUart::ProfilePayload &p, bool ack) override
            {
                if (uart_)
                    uart_->sendProfileCommand(p, ack);
            }

            void sendConfigPushChunk(const DryerUart::ConfigChunkPayload &p,
                                     uint8_t dataLen, uint8_t flags) override
            {
                if (uart_)
                    uart_->sendConfigPushChunk(p, dataLen, flags);
            }

            void sendRfidWriteData(const DryerUart::RfidDataPayload &p,
                                   uint8_t flags) override
            {
                if (uart_)
                    uart_->sendRfidWriteData(p, flags);
            }

            bool waitForAck(uint32_t timeoutMs) override
            {
                return uart_ && uart_->waitForAck(timeoutMs);
            }

        private:
            DryerUart::UartBridge *uart_;
        };

    } // namespace cloud
} // namespace idryer
