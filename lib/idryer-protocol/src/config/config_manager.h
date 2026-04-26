/**
 * @file config_manager.h
 * @brief Менеджер Remote Config для сборки/отправки фрагментированных конфигов
 *
 * ConfigManager реализует протокол фрагментированной передачи JSON конфигурации
 * между ESP32 и RP2040 по UART.
 *
 * @see docs/10-flows/01-basic-flows.md (Remote Config), docs/02-uart/01-uart.md (ConfigPush)
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>

#include "../uart/uart_protocol.h"

namespace DryerUart
{

    /// Максимальный размер буфера для сборки конфига (16KB для полного меню ~12-13KB)
    constexpr uint16_t CONFIG_BUFFER_SIZE = 16384;

    /// Результат обработки фрагмента
    enum class ConfigFragmentResult : uint8_t
    {
        Ok = 0,           // Фрагмент принят, ожидаем следующий
        Complete = 1,     // Последний фрагмент, конфиг готов
        ErrorSequence = 2,    // Неправильный порядок фрагментов
        ErrorOverflow = 3,    // Превышен размер буфера
        ErrorTransferId = 4,  // Неверный transferId (другая передача)
    };

    /**
     * @brief Менеджер сборки входящих фрагментов конфигурации
     *
     * Используется на стороне приёмника (ESP32 для config от RP2040,
     * или RP2040 для команды set от Backend).
     */
    class ConfigReceiver
    {
    public:
        /**
         * @brief Callback при завершении сборки конфига
         * @param json Указатель на собранный JSON (null-terminated)
         * @param length Длина JSON без \0
         * @param isLast Это последний фрагмент (FLAG_LAST_FRAGMENT)
         */
        using CompleteCallback = std::function<void(const char *json, uint16_t length, bool isLast)>;

        ConfigReceiver() = default;

        /**
         * @brief Сброс состояния приёмника
         */
        void reset()
        {
            m_transferId = 0;
            m_totalSize = 0;
            m_receivedSize = 0;
            m_nextExpectedChunk = 0;
            m_active = false;
        }

        /**
         * @brief Обработка входящего фрагмента
         * @param payload Payload из ConfigPush фрейма
         * @param dataLen Реальная длина данных (payloadLen - headerSize)
         * @param flags Флаги из заголовка фрейма
         * @return Результат обработки
         */
        ConfigFragmentResult processFragment(const ConfigChunkPayload &payload,
                                              uint8_t dataLen,
                                              uint8_t flags)
        {
            const auto &hdr = payload.header;

            // Первый фрагмент — инициализация
            if (hdr.chunkIndex == 0)
            {
                m_transferId = hdr.transferId;
                m_totalSize = hdr.totalSize;
                m_receivedSize = 0;
                m_nextExpectedChunk = 0;
                m_active = true;

                // Проверка размера
                if (m_totalSize > CONFIG_BUFFER_SIZE)
                {
                    reset();
                    return ConfigFragmentResult::ErrorOverflow;
                }
            }
            else
            {
                // Проверка transferId
                if (!m_active || hdr.transferId != m_transferId)
                {
                    return ConfigFragmentResult::ErrorTransferId;
                }
            }

            // Проверка порядка
            if (hdr.chunkIndex != m_nextExpectedChunk)
            {
                return ConfigFragmentResult::ErrorSequence;
            }

            // Проверка границ буфера
            if (m_receivedSize + dataLen > CONFIG_BUFFER_SIZE)
            {
                reset();
                return ConfigFragmentResult::ErrorOverflow;
            }

            // Копируем данные
            memcpy(m_buffer + m_receivedSize, payload.data, dataLen);
            m_receivedSize += dataLen;
            m_nextExpectedChunk++;

            // Проверяем флаг последнего фрагмента.
            // Текущее поведение: финал определяется только по FLAG_LAST_FRAGMENT,
            // без строгой сверки m_receivedSize == m_totalSize.
            if (flags & FLAG_LAST_FRAGMENT)
            {
                // Добавляем null-terminator
                if (m_receivedSize < CONFIG_BUFFER_SIZE)
                {
                    m_buffer[m_receivedSize] = '\0';
                }
                return ConfigFragmentResult::Complete;
            }

            return ConfigFragmentResult::Ok;
        }

        /**
         * @brief Получить собранный JSON
         * @return Указатель на буфер (null-terminated)
         */
        const char *getJson() const { return reinterpret_cast<const char *>(m_buffer); }

        /**
         * @brief Получить размер собранного JSON
         */
        uint16_t getJsonLength() const { return m_receivedSize; }

        /**
         * @brief Активна ли сборка
         */
        bool isActive() const { return m_active; }

        /**
         * @brief Получить текущий transferId
         */
        uint16_t getTransferId() const { return m_transferId; }

    private:
        uint8_t m_buffer[CONFIG_BUFFER_SIZE]{};
        uint16_t m_transferId = 0;
        uint16_t m_totalSize = 0;
        uint16_t m_receivedSize = 0;
        uint16_t m_nextExpectedChunk = 0;
        bool m_active = false;
    };

    /**
     * @brief Менеджер отправки фрагментированного конфига
     *
     * Используется на стороне отправителя (RP2040 для config к ESP32,
     * или ESP32 для команды set к RP2040).
     */
    class ConfigSender
    {
    public:
        /**
         * @brief Callback для отправки одного фрагмента
         * @param payload Payload для отправки
         * @param payloadLen Длина payload (header + data)
         * @param flags Флаги (FLAG_FRAGMENTED и/или FLAG_LAST_FRAGMENT)
         * @return true если фрагмент отправлен успешно
         */
        using SendCallback = std::function<bool(const ConfigChunkPayload &payload,
                                                 uint8_t payloadLen,
                                                 uint8_t flags)>;

        ConfigSender() = default;

        /**
         * @brief Отправить JSON с автоматической фрагментацией
         * @param json Указатель на JSON строку
         * @param length Длина JSON (без \0)
         * @param transferId ID передачи (уникальный)
         * @param sendFn Callback для отправки каждого фрагмента
         * @return Количество отправленных фрагментов, или 0 при ошибке
         */
        uint16_t send(const char *json, uint16_t length, uint16_t transferId, SendCallback sendFn)
        {
            if (!json || length == 0 || !sendFn)
                return 0;

            uint16_t offset = 0;
            uint16_t chunkIndex = 0;
            uint16_t sentCount = 0;

            while (offset < length)
            {
                ConfigChunkPayload payload{};
                payload.header.transferId = transferId;
                payload.header.totalSize = (chunkIndex == 0) ? length : 0;
                payload.header.chunkIndex = chunkIndex;

                // Вычисляем размер данных для этого фрагмента
                uint16_t remaining = length - offset;
                uint8_t dataLen = (remaining > CONFIG_CHUNK_DATA_SIZE)
                                      ? CONFIG_CHUNK_DATA_SIZE
                                      : static_cast<uint8_t>(remaining);

                memcpy(payload.data, json + offset, dataLen);

                // Определяем флаги
                uint8_t flags = FLAG_FRAGMENTED;
                if (offset + dataLen >= length)
                {
                    flags |= FLAG_LAST_FRAGMENT;
                }

                // Отправляем
                uint8_t payloadLen = CONFIG_CHUNK_HEADER_SIZE + dataLen;
                if (!sendFn(payload, payloadLen, flags))
                {
                    return 0; // Ошибка отправки
                }

                offset += dataLen;
                chunkIndex++;
                sentCount++;
            }

            return sentCount;
        }

        /**
         * @brief Генерация нового transferId
         * @return Уникальный ID для передачи
         */
        static uint16_t generateTransferId()
        {
            static uint16_t counter = 0;
            return ++counter;
        }
    };

} // namespace DryerUart
