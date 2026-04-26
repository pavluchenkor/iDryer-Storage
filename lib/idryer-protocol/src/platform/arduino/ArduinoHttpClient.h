/**
 * @file ArduinoHttpClient.h
 * @brief Arduino/ESP32 реализация `IHttpClient`.
 */

#pragma once

#include "../../device/interfaces/IHttpClient.h"

namespace idryer {

/// TLS работает через `WiFiClientSecure::setInsecure()` (без проверки CA).
class ArduinoHttpClient : public IHttpClient {
public:
    ArduinoHttpClient();

    bool postJson(const char* url, const char* body,
                 JsonDocument& response) override;

    bool getJson(const char* url, JsonDocument& response) override;

    void setTimeout(uint32_t timeoutMs) override;

private:
    uint32_t timeout_ = 10000;
};

} // namespace idryer
