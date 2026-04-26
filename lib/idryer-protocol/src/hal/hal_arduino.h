/**
 * @file hal_arduino.h
 * @brief Arduino-реализация HAL (`ITime`, `ISerial`, `ILogger`).
 */

#pragma once

#include "hal_types.h"
#include <Arduino.h>

#if defined(ARDUINO_ARCH_RP2040)
#include <SerialUART.h> // Для доступа к overflow() и setFIFOSize()
#endif

#if defined(ESP32) || defined(ESP_PLATFORM)
#include "driver/uart.h"
#endif

namespace idryer {
namespace hal {

// =============================================================================
// ARDUINO TIME - РЕАЛИЗАЦИЯ ИНТЕРФЕЙСА ITime
// =============================================================================

/// Обёртка над `millis/micros/delay`.
class ArduinoTime : public ITime {
public:
    uint32_t millis() override {
        return ::millis();
    }

    uint32_t micros() override {
        return ::micros();
    }

    void delayMs(uint32_t ms) override {
        ::delay(ms);
    }

    void delayUs(uint32_t us) override {
        ::delayMicroseconds(us);
    }
};

// =============================================================================
// ARDUINO SERIAL - РЕАЛИЗАЦИЯ ИНТЕРФЕЙСА ISerial
// =============================================================================

/// Обёртка над `HardwareSerial`.
class ArduinoSerial : public ISerial {
public:
    /// `uartNum` нужен только на ESP32 для `uart_wait_tx_done`.
    explicit ArduinoSerial(HardwareSerial& serial, int uartNum = -1)
        : serial_(serial)
#if defined(ESP32) || defined(ESP_PLATFORM)
        , uartNum_(uartNum)
#endif
    {}

    void begin(uint32_t baudRate) override {
        serial_.begin(baudRate);
    }

    int available() override {
        return serial_.available();
    }

    int read() override {
        return serial_.read();
    }

    size_t readBytes(uint8_t* buffer, size_t length) override {
        return serial_.readBytes(buffer, length);
    }

    size_t write(const uint8_t* buffer, size_t length) override {
        return serial_.write(buffer, length);
    }

    void flush() override {
        serial_.flush();

#if defined(ESP32) || defined(ESP_PLATFORM)
        // На ESP32 flush() не ждёт hardware FIFO!
        // Используем ESP-IDF функцию для ожидания реального окончания TX
        if (uartNum_ >= 0 && uartNum_ < UART_NUM_MAX) {
            uart_wait_tx_done((uart_port_t)uartNum_, portMAX_DELAY);
        } else {
            // ВРЕМЕННЫЙ ЛОГ: если uart_wait_tx_done не вызвался
            HAL_LOG_WARN("UART", "flush() called but uartNum_=%d (no wait!)", uartNum_);
        }
#endif
    }

    /// Для RP2040 читает флаг `overflow()` из `SerialUART`.
    bool checkOverflow() {
#if defined(ARDUINO_ARCH_RP2040)
        // На RP2040 HardwareSerial представлен классом SerialUART
        auto *uartPtr = static_cast<SerialUART*>(&serial_);
        return uartPtr->overflow();
#else
        // ESP32/другие: overflow() может не быть реализован
        return false;
#endif
    }

private:
    HardwareSerial& serial_;  // Ссылка на аппаратный UART
#if defined(ESP32) || defined(ESP_PLATFORM)
    int uartNum_;  // Номер UART для uart_wait_tx_done()
#endif
};

// =============================================================================
// ARDUINO LOGGER - РЕАЛИЗАЦИЯ ИНТЕРФЕЙСА ILogger
// =============================================================================

/// Логгер в `Stream` с опциональными ANSI-цветами.
class ArduinoLogger : public ILogger {
public:
    explicit ArduinoLogger(Stream& output, bool enableColors = true)
        : output_(output)
        , level_(LogLevel::Debug)
        , enableColors_(enableColors) {}

    void log(LogLevel level, const char* tag, const char* format, ...) override;

    void setLevel(LogLevel level) override {
        level_ = level;
    }

    LogLevel getLevel() const override {
        return level_;
    }

    void setColors(bool enable) {
        enableColors_ = enable;
    }

private:
    Stream& output_;        // Поток вывода
    LogLevel level_;        // Минимальный уровень
    bool enableColors_;     // ANSI цвета

    // Возвращает строковое представление уровня
    const char* levelToString(LogLevel level) const;

    // Возвращает ANSI код цвета для уровня
    const char* levelToColor(LogLevel level) const;
};

// =============================================================================
// ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
// =============================================================================

/// Инициализация HAL. `debugStream=nullptr` отключает логи.
void initArduinoHal(Stream* debugStream = nullptr, bool enableColors = true);

/// Освобождает ресурсы логгера.
void deinitArduinoHal();

} // namespace hal
} // namespace idryer
