/**
 * @file config.h
 * @brief Константы конфигурации библиотеки idryer-protocol
 *
 * Этот файл содержит все константы размеров буферов и таймингов.
 * Можно переопределить через -D флаги компилятора.
 *
 * Пример переопределения в platformio.ini:
 * @code
 * build_flags = -DIDRYER_MAX_TOKEN_LEN=256
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// РАЗМЕРЫ БУФЕРОВ
// =============================================================================
// Эти значения можно переопределить через -D флаги компилятора

#ifndef IDRYER_MAX_SERIAL_NUMBER_LEN
#define IDRYER_MAX_SERIAL_NUMBER_LEN 32   // Серийный номер устройства
#endif

#ifndef IDRYER_MAX_TOKEN_LEN
#define IDRYER_MAX_TOKEN_LEN 512          // JWT токен от backend (может быть длинным)
#endif

#ifndef IDRYER_MAX_DEVICE_ID_LEN
#define IDRYER_MAX_DEVICE_ID_LEN 40       // UUID устройства + запас
#endif

#ifndef IDRYER_MAX_SSID_LEN
#define IDRYER_MAX_SSID_LEN 33            // WiFi SSID (32 символа + \0)
#endif

#ifndef IDRYER_MAX_PASSWORD_LEN
#define IDRYER_MAX_PASSWORD_LEN 64        // WiFi пароль
#endif

#ifndef IDRYER_MAX_IP_LEN
#define IDRYER_MAX_IP_LEN 16              // IPv4 адрес "xxx.xxx.xxx.xxx"
#endif

#ifndef IDRYER_MAX_MAC_LEN
#define IDRYER_MAX_MAC_LEN 18             // MAC адрес "AA:BB:CC:DD:EE:FF"
#endif

#ifndef IDRYER_MAX_URL_LEN
#define IDRYER_MAX_URL_LEN 256            // URL для HTTP запросов
#endif

#ifndef IDRYER_MAX_PIN_LEN
#define IDRYER_MAX_PIN_LEN 9              // PIN код (8 цифр + \0)
#endif

// =============================================================================
// ТАЙМИНГИ (в миллисекундах)
// =============================================================================

#ifndef IDRYER_WIFI_RETRY_INTERVAL_MS
#define IDRYER_WIFI_RETRY_INTERVAL_MS 5000    // Интервал повторной попытки WiFi
#endif

#ifndef IDRYER_PROVISION_RETRY_MS
#define IDRYER_PROVISION_RETRY_MS 10000       // Интервал повторной попытки provision
#endif

#ifndef IDRYER_CLAIM_POLL_INTERVAL_MS
#define IDRYER_CLAIM_POLL_INTERVAL_MS 5000    // Интервал опроса статуса claiming
#endif

#ifndef IDRYER_MQTT_RETRY_INTERVAL_MS
#define IDRYER_MQTT_RETRY_INTERVAL_MS 5000    // Интервал повторной попытки MQTT
#endif

// =============================================================================
// РЕЖИМЫ КОМПИЛЯЦИИ
// =============================================================================
// Определяются через -D флаги в platformio.ini

// IDRYER_LINK_MODE - включает cloud/, mqtt/ (для ESP32 Link)
// IDRYER_CONTROLLER_MODE - только uart/ (для RP2040 Controller)
// IDRYER_HAL_ARDUINO - использовать Arduino HAL реализацию
// IDRYER_HAL_ESPIDF - использовать ESP-IDF HAL реализацию

// =============================================================================
// ПРОВЕРКИ
// =============================================================================

static_assert(IDRYER_MAX_SERIAL_NUMBER_LEN >= 16, "Serial number buffer too small");
static_assert(IDRYER_MAX_TOKEN_LEN >= 128, "Token buffer too small for JWT");
static_assert(IDRYER_MAX_DEVICE_ID_LEN >= 36, "Device ID buffer too small for UUID");
