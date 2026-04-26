/**
 * @file ArduinoWifiManager.h
 * @brief Arduino/ESP32 реализация `IWifiManager`.
 */

#pragma once

#include "../../device/interfaces/IWifiManager.h"
#include "../../core/config.h"
#include <WiFi.h>

namespace idryer {

class ArduinoWifiManager : public IWifiManager {
public:
    ArduinoWifiManager();

    void begin(const char* ssid, const char* password) override;
    bool connect() override;
    bool isConnected() override;
    void disconnect() override;
    void getLocalIP(char* buffer, size_t bufferSize) override;
    void getSSID(char* buffer, size_t bufferSize) override;
    int getRSSI() override;
    void getMacAddress(char* buffer, size_t bufferSize) override;
    void loop() override;

private:
    char ssid_[IDRYER_MAX_SSID_LEN];
    char password_[IDRYER_MAX_PASSWORD_LEN];
    bool scanLogged_ = false;  // Чтобы не повторять диагностический scan в логах.
};

} // namespace idryer
