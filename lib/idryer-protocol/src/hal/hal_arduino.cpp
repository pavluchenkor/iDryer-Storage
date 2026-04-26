/**
 * @file hal_arduino.cpp
 * @brief Hardware Abstraction Layer - реализация для Arduino
 */

#include "hal_arduino.h"
#include <stdio.h>

namespace idryer {
namespace hal {

// =============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// =============================================================================

// Глобальный контекст HAL (определён в hal_types.h как extern)
HalContext* g_hal = nullptr;

// Статические экземпляры HAL компонентов
// (создаются при инициализации, живут до завершения программы)
static ArduinoTime s_time;
static ArduinoLogger* s_logger = nullptr;
static HalContext s_context;

// =============================================================================
// ARDUINO LOGGER - РЕАЛИЗАЦИЯ
// =============================================================================

void ArduinoLogger::log(LogLevel level, const char* tag, const char* format, ...) {
    // Пропускаем сообщения ниже установленного уровня
    if (level < level_) {
        return;
    }

    // Буфер для форматированного сообщения
    char buffer[256];

    // Форматируем сообщение
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Выводим с цветами или без
    if (enableColors_) {
        // Формат: [ЦВЕТ][УРОВЕНЬ] TAG: сообщение[СБРОС]
        output_.printf("%s[%s] %s: %s\033[0m\n",
                       levelToColor(level),
                       levelToString(level),
                       tag ? tag : "???",
                       buffer);
    } else {
        // Формат без цветов: [УРОВЕНЬ] TAG: сообщение
        output_.printf("[%s] %s: %s\n",
                       levelToString(level),
                       tag ? tag : "???",
                       buffer);
    }
}

const char* ArduinoLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "?????";
    }
}

const char* ArduinoLogger::levelToColor(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "\033[90m";   // Серый
        case LogLevel::Info:    return "\033[32m";   // Зелёный
        case LogLevel::Warning: return "\033[33m";   // Жёлтый
        case LogLevel::Error:   return "\033[31m";   // Красный
        default:                return "\033[0m";    // Сброс
    }
}

// =============================================================================
// ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
// =============================================================================

void initArduinoHal(Stream* debugStream, bool enableColors) {
    // Устанавливаем компонент времени (всегда доступен)
    s_context.time = &s_time;

    // Создаём логгер если указан поток вывода
    if (debugStream != nullptr) {
        // Удаляем старый логгер если был
        if (s_logger != nullptr) {
            delete s_logger;
        }
        s_logger = new ArduinoLogger(*debugStream, enableColors);
        s_context.logger = s_logger;
    } else {
        s_context.logger = nullptr;
    }

    // Устанавливаем глобальный контекст
    g_hal = &s_context;
}

void deinitArduinoHal() {
    // Удаляем логгер
    if (s_logger != nullptr) {
        delete s_logger;
        s_logger = nullptr;
    }

    // Сбрасываем контекст
    s_context.logger = nullptr;
    g_hal = nullptr;
}

} // namespace hal
} // namespace idryer
