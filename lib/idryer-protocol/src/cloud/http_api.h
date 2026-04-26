/**
 * @file http_api.h
 * @brief HTTP обёртка backend API (`provision/register/check-claim`).
 */

#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../device/interfaces/IHttpClient.h"

namespace idryer {
namespace cloud {

// Результаты API вызовов
struct ProvisionResult {
    bool success;           // true если запрос успешен
    bool isNew;             // true если устройство новое
    bool isClaimed;         // true если уже привязано
    char token[IDRYER_MAX_TOKEN_LEN];     // JWT токен
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN]; // deviceId если уже привязано

    ProvisionResult() : success(false), isNew(false), isClaimed(false) {
        token[0] = '\0';
        deviceId[0] = '\0';
    }
};

struct RegisterResult {
    bool success;           // true если запрос успешен
    bool alreadyClaimed;    // true если устройство уже привязано (recovery)
    char pin[IDRYER_MAX_PIN_LEN];  // 8-значный PIN
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN]; // deviceId при alreadyClaimed
    uint32_t remainingSeconds;      // время до истечения PIN

    RegisterResult() : success(false), alreadyClaimed(false), remainingSeconds(0) {
        pin[0] = '\0';
        deviceId[0] = '\0';
    }
};

struct ClaimCheckResult {
    bool success;           // true если запрос успешен
    bool claimed;           // true если устройство привязано
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN]; // UUID устройства

    ClaimCheckResult() : success(false), claimed(false) {
        deviceId[0] = '\0';
    }
};

/// Формирует URL/JSON запросы и парсит ответы API.
class HttpApi {
public:
    HttpApi(IHttpClient* http, const char* baseUrl);

    /// POST `/devices/provision`.
    ProvisionResult provision(const char* serialNumber);

    /// POST `/devices/register`.
    RegisterResult registerDevice(const char* token, const char* serialNumber = nullptr);

    /// GET `/devices/check-claim/{token}`.
    ClaimCheckResult checkClaim(const char* token);

private:
    IHttpClient* http_;
    char baseUrl_[IDRYER_MAX_URL_LEN];

    // Helper для составления endpoint URL.
    void buildUrl(char* buffer, size_t bufferSize, const char* path) const;
};

} // namespace cloud
} // namespace idryer
