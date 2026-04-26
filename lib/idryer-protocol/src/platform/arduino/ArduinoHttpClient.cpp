/**
 * @file ArduinoHttpClient.cpp
 * @brief Реализация HTTP клиента для Arduino Framework (ESP32)
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoHttpClient.h"
#include "../../hal/hal_types.h"
#include "../../mqtt/root_ca.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace idryer {

// =============================================================================
// КОНСТРУКТОР
// =============================================================================

ArduinoHttpClient::ArduinoHttpClient()
{
}

// =============================================================================
// POST ЗАПРОС
// =============================================================================

bool ArduinoHttpClient::postJson(const char* url, const char* body,
                                 JsonDocument& response)
{
    if (!url || !body) {
        return false;
    }

    HAL_LOG_DEBUG("HTTP", "POST %s", url);
    HAL_LOG_DEBUG("HTTP", "Body: %s", body);

    // Создаём безопасный клиент (HTTPS)
    WiFiClientSecure client;
    client.setCACert(ROOT_CA_LETSENCRYPT);

    HTTPClient https;
    https.setTimeout(timeout_);

    // Начинаем соединение
    if (!https.begin(client, url)) {
        HAL_LOG_ERROR("HTTP", "https.begin() FAILED for: %s", url);
        return false;
    }

    // Добавляем заголовок Content-Type
    https.addHeader("Content-Type", "application/json");

    // Отправляем запрос
    int httpCode = https.POST(body);

    HAL_LOG_DEBUG("HTTP", "POST returned code: %d", httpCode);

    // Проверяем код ответа
    if (httpCode < 200 || httpCode >= 300) {
        HAL_LOG_ERROR("HTTP", "POST %s failed: %d", url, httpCode);

        // Диагностика для -1 (connection refused)
        if (httpCode == -1) {
            HAL_LOG_ERROR("HTTP", "Connection refused - check DNS, firewall, or server");
        }

        https.end();
        return false;
    }

    // Получаем и парсим ответ
    String payload = https.getString();
    https.end();

    DeserializationError err = deserializeJson(response, payload);
    if (err) {
        HAL_LOG_ERROR("HTTP", "JSON parse error: %s", err.c_str());
        return false;
    }

    return true;
}

// =============================================================================
// GET ЗАПРОС
// =============================================================================

bool ArduinoHttpClient::getJson(const char* url, JsonDocument& response)
{
    if (!url) {
        return false;
    }

    HAL_LOG_DEBUG("HTTP", "GET %s", url);

    // Создаём безопасный клиент (HTTPS)
    WiFiClientSecure client;
    client.setCACert(ROOT_CA_LETSENCRYPT);

    HTTPClient https;
    https.setTimeout(timeout_);

    // Начинаем соединение
    if (!https.begin(client, url)) {
        HAL_LOG_ERROR("HTTP", "Failed to begin connection to: %s", url);
        return false;
    }

    // Отправляем запрос
    int httpCode = https.GET();

    // По документации API: для /check-claim 404 - валидный ответ с {"claimed": false}
    if (httpCode < 0 || (httpCode >= 300 && httpCode != 404)) {
        HAL_LOG_ERROR("HTTP", "GET %s failed: %d", url, httpCode);
        https.end();
        return false;
    }

    // Получаем и парсим ответ
    String payload = https.getString();
    https.end();

    DeserializationError err = deserializeJson(response, payload);
    if (err) {
        HAL_LOG_ERROR("HTTP", "JSON parse error: %s", err.c_str());
        return false;
    }

    return true;
}

// =============================================================================
// НАСТРОЙКИ
// =============================================================================

void ArduinoHttpClient::setTimeout(uint32_t timeoutMs)
{
    timeout_ = timeoutMs;
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
