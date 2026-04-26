/**
 * @file hal_types.h
 * @brief HAL интерфейсы и макросы доступа к ним.
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

namespace idryer {
namespace hal {

// =============================================================================
// ИНТЕРФЕЙС ВРЕМЕНИ
// =============================================================================

/// Интерфейс времени.
class ITime {
public:
  virtual ~ITime() = default;

  virtual uint32_t millis() = 0;

  virtual uint32_t micros() = 0;

  /// Блокирующая задержка.
  virtual void delayMs(uint32_t ms) = 0;

  virtual void delayUs(uint32_t us) = 0;
};

// =============================================================================
// ИНТЕРФЕЙС UART
// =============================================================================

/// Интерфейс UART.
class ISerial {
public:
  virtual ~ISerial() = default;

  virtual void begin(uint32_t baudRate) = 0;

  virtual int available() = 0;

  virtual int read() = 0;

  virtual size_t readBytes(uint8_t *buffer, size_t length) = 0;

  virtual size_t write(const uint8_t *buffer, size_t length) = 0;

  /// Ждёт фактическое завершение TX.
  virtual void flush() = 0;
};

// =============================================================================
// ИНТЕРФЕЙС ЛОГИРОВАНИЯ
// =============================================================================

/**
 * @brief Уровни логирования
 */
enum class LogLevel : uint8_t {
  Debug = 0,   // Отладочная информация (много вывода)
  Info = 1,    // Информационные сообщения
  Warning = 2, // Предупреждения
  Error = 3,   // Ошибки
  None = 4     // Логирование отключено
};

/// Интерфейс логгера.
class ILogger {
public:
  virtual ~ILogger() = default;

  virtual void log(LogLevel level, const char *tag, const char *format, ...) = 0;

  virtual void setLevel(LogLevel level) = 0;

  virtual LogLevel getLevel() const = 0;
};

// =============================================================================
// ГЛОБАЛЬНЫЙ КОНТЕКСТ HAL
// =============================================================================

/// Контекст HAL, заполняется при инициализации платформы.
struct HalContext {
  ITime *time;     // Работа со временем (обязательно)
  ILogger *logger; // Логирование (может быть nullptr)
};

/// Используется макросами `HAL_*`.
extern HalContext *g_hal;

// =============================================================================
// УДОБНЫЕ МАКРОСЫ
// =============================================================================

#define HAL_MILLIS() (idryer::hal::g_hal->time->millis())

#define HAL_MICROS() (idryer::hal::g_hal->time->micros())

#define HAL_DELAY_MS(ms) (idryer::hal::g_hal->time->delayMs(ms))

#define HAL_DELAY_US(us) (idryer::hal::g_hal->time->delayUs(us))


#ifndef HAL_NO_LOG
// Макросы логирования (если логгер не установлен - игнорируем)
#define HAL_LOG(level, tag, fmt, ...)                                                                                                                                                                  \
  do {                                                                                                                                                                                                 \
    if (idryer::hal::g_hal && idryer::hal::g_hal->logger) idryer::hal::g_hal->logger->log(level, tag, fmt, ##__VA_ARGS__);                                                                             \
  } while (0)

#define HAL_LOG_DEBUG(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Debug, tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_INFO(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Info, tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_WARN(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Warning, tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_ERROR(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Error, tag, fmt, ##__VA_ARGS__)
#else
// Логирование отключено - пустые макросы
#define HAL_LOG(level, tag, fmt, ...)                                                                                                                                                                  \
  do {                                                                                                                                                                                                 \
  } while (0)
#define HAL_LOG_DEBUG(tag, fmt, ...)                                                                                                                                                                   \
  do {                                                                                                                                                                                                 \
  } while (0)
#define HAL_LOG_INFO(tag, fmt, ...)                                                                                                                                                                    \
  do {                                                                                                                                                                                                 \
  } while (0)
#define HAL_LOG_WARN(tag, fmt, ...)                                                                                                                                                                    \
  do {                                                                                                                                                                                                 \
  } while (0)
#define HAL_LOG_ERROR(tag, fmt, ...)                                                                                                                                                                   \
  do {                                                                                                                                                                                                 \
  } while (0)
#endif
} // namespace hal
} // namespace idryer
