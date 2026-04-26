/**
 * @file http_api.cpp
 * @brief Реализация HTTP API клиента
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "http_api.h"
#include "../hal/hal_types.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

namespace idryer {
namespace cloud {

// =============================================================================
// КОНСТРУКТОР
// =============================================================================

HttpApi::HttpApi(IHttpClient* http, const char* baseUrl)
    : http_(http)
{
    // Копируем baseUrl
    if (baseUrl) {
        strncpy(baseUrl_, baseUrl, sizeof(baseUrl_) - 1);
        baseUrl_[sizeof(baseUrl_) - 1] = '\0';
    } else {
        baseUrl_[0] = '\0';
    }
}

// =============================================================================
// PROVISION
// =============================================================================

ProvisionResult HttpApi::provision(const char* serialNumber) {
    ProvisionResult result;

    if (!http_ || !serialNumber || serialNumber[0] == '\0') {
        HAL_LOG_ERROR("HTTP", "provision: invalid params");
        return result;
    }

    // Формируем URL
    char url[IDRYER_MAX_URL_LEN];
    buildUrl(url, sizeof(url), "/devices/provision");

    // Формируем тело запроса
    DynamicJsonDocument body(256);
    body["serialNumber"] = serialNumber;

    char payload[256];
    serializeJson(body, payload, sizeof(payload));

    HAL_LOG_INFO("HTTP", "POST %s", url);

    // Выполняем запрос
    DynamicJsonDocument response(512);
    if (!http_->postJson(url, payload, response)) {
        HAL_LOG_ERROR("HTTP", "provision failed");
        return result;
    }

    // Парсим ответ
    result.success = true;

    if (response.containsKey("deviceToken")) {
        const char* token = response["deviceToken"].as<const char*>();
        if (token) {
            strncpy(result.token, token, sizeof(result.token) - 1);
            result.token[sizeof(result.token) - 1] = '\0';
        }
    }

    if (response.containsKey("isNew")) {
        result.isNew = response["isNew"].as<bool>();
    }

    if (response.containsKey("isClaimed")) {
        result.isClaimed = response["isClaimed"].as<bool>();
    }

    if (response.containsKey("deviceId")) {
        const char* deviceId = response["deviceId"].as<const char*>();
        if (deviceId) {
            strncpy(result.deviceId, deviceId, sizeof(result.deviceId) - 1);
            result.deviceId[sizeof(result.deviceId) - 1] = '\0';
        }
    }

    HAL_LOG_INFO("HTTP", "provision OK: isNew=%d isClaimed=%d",
                 result.isNew, result.isClaimed);

    return result;
}

// =============================================================================
// REGISTER
// =============================================================================

RegisterResult HttpApi::registerDevice(const char* token, const char* serialNumber) {
    RegisterResult result;

    if (!http_ || !token || token[0] == '\0') {
        HAL_LOG_ERROR("HTTP", "register: invalid params");
        return result;
    }

    // Формируем URL
    char url[IDRYER_MAX_URL_LEN];
    buildUrl(url, sizeof(url), "/devices/register");

    // Формируем тело запроса
    DynamicJsonDocument body(512);
    body["token"] = token;
    if (serialNumber && serialNumber[0] != '\0') {
        body["serialNumber"] = serialNumber;
    }

    char payload[512];
    serializeJson(body, payload, sizeof(payload));

    HAL_LOG_INFO("HTTP", "POST %s", url);

    // Выполняем запрос
    DynamicJsonDocument response(512);
    if (!http_->postJson(url, payload, response)) {
        HAL_LOG_ERROR("HTTP", "register failed");
        return result;
    }

    // Парсим ответ
    result.success = true;

    // Recovery: устройство уже привязано на сервере, но ESP не знала
    if (response.containsKey("alreadyClaimed") && response["alreadyClaimed"].as<bool>()) {
        result.alreadyClaimed = true;
        if (response.containsKey("deviceId")) {
            const char* id = response["deviceId"].as<const char*>();
            if (id) {
                strncpy(result.deviceId, id, sizeof(result.deviceId) - 1);
                result.deviceId[sizeof(result.deviceId) - 1] = '\0';
            }
        }
        HAL_LOG_INFO("HTTP", "register: already claimed, deviceId=%s", result.deviceId);
        return result;
    }

    if (response.containsKey("pin")) {
        const char* pin = response["pin"].as<const char*>();
        if (pin) {
            strncpy(result.pin, pin, sizeof(result.pin) - 1);
            result.pin[sizeof(result.pin) - 1] = '\0';
        }
    }

    if (response.containsKey("remainingSeconds")) {
        result.remainingSeconds = response["remainingSeconds"].as<uint32_t>();
    }

    HAL_LOG_INFO("HTTP", "register OK: PIN=%s expires=%us",
                 result.pin, result.remainingSeconds);

    return result;
}

// =============================================================================
// CHECK CLAIM
// =============================================================================

ClaimCheckResult HttpApi::checkClaim(const char* token) {
    ClaimCheckResult result;

    if (!http_ || !token || token[0] == '\0') {
        HAL_LOG_ERROR("HTTP", "checkClaim: invalid params");
        return result;
    }

    // Формируем URL: /devices/check-claim/{token}
    char url[IDRYER_MAX_URL_LEN];
    char path[IDRYER_MAX_URL_LEN];
    snprintf(path, sizeof(path), "/devices/check-claim/%s", token);
    buildUrl(url, sizeof(url), path);

    HAL_LOG_DEBUG("HTTP", "GET %s", url);

    // Выполняем запрос
    DynamicJsonDocument response(512);
    if (!http_->getJson(url, response)) {
        // Не логируем ошибку - это нормально пока не привязано
        return result;
    }

    // Парсим ответ
    result.success = true;

    if (response.containsKey("claimed")) {
        result.claimed = response["claimed"].as<bool>();
    }

    if (result.claimed && response.containsKey("deviceId")) {
        const char* deviceId = response["deviceId"].as<const char*>();
        if (deviceId) {
            strncpy(result.deviceId, deviceId, sizeof(result.deviceId) - 1);
            result.deviceId[sizeof(result.deviceId) - 1] = '\0';
        }
    }

    if (result.claimed) {
        HAL_LOG_INFO("HTTP", "checkClaim: CLAIMED deviceId=%s", result.deviceId);
    }

    return result;
}

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// =============================================================================

void HttpApi::buildUrl(char* buffer, size_t bufferSize, const char* path) const {
    snprintf(buffer, bufferSize, "%s%s", baseUrl_, path);
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
