/**
 * @file IHttpClient.h
 * @brief Платформенно-независимый HTTP-клиент.
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>

namespace idryer {

/// Возвращает `true` только при успешном HTTP и валидном JSON-ответе.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// `body` передаётся как `application/json`.
    virtual bool postJson(const char* url, const char* body,
                         JsonDocument& response) = 0;

    virtual bool getJson(const char* url, JsonDocument& response) = 0;

    virtual void setTimeout(uint32_t timeoutMs) = 0;
};

} // namespace idryer
