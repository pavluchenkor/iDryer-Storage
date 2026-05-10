#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)
#ifdef BUILD_TARGET_DRYER

#include <idryer_core.h>
#include <profiles/dryer/dryer_profile.h>
#include <hal/hal_arduino.h>
#include <Preferences.h>

// DRYER_UART_RX / DRYER_UART_TX must be defined in build_flags.
#ifndef DRYER_UART_RX
#error "DRYER_UART_RX not defined — set it in platformio.ini build_flags"
#endif
#ifndef DRYER_UART_TX
#error "DRYER_UART_TX not defined — set it in platformio.ini build_flags"
#endif

namespace dryerlink {

class DryerDevice {
public:
    DryerDevice();

    void begin();
    void loop();
    bool isOnline() const;
    void setWifiCredentials(const char* ssid, const char* password);

private:
    void saveWifiCredentials(const char* ssid, const char* password);
    bool loadWifiCredentials(char* ssid, size_t ssidLen, char* password, size_t passLen);
    void wireClaimHandlers();

    static void onCloudStateChange(idryer::cloud::CloudState oldState,
                                   idryer::cloud::CloudState newState, void* ctx);

    // Cloud stack
    idryer::ArduinoWifiManager     wifi_;
    idryer::ArduinoHttpClient      http_;
    idryer::ArduinoCredentialStore store_;
    idryer::cloud::HttpApi         api_;
    idryer::MqttClient             mqtt_;
    idryer::cloud::CloudStateMachine cloud_;
    idryer::ActionDispatcher       dispatcher_;

    // UART (Serial1, UART_NUM_1)
    idryer::hal::ArduinoSerial     uartSerial_;
    idryer::UartBridge             uartBridge_;

    // Profile + Runtime
    idryer::DryerProfile*  profile_ = nullptr;
    idryer::IdryerRuntime* runtime_ = nullptr;

    Preferences wifiPrefs_;
};

} // namespace dryerlink

#endif // BUILD_TARGET_DRYER
#endif // ESP32 || ESP_PLATFORM
