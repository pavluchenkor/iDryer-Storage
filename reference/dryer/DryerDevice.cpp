#if defined(ESP32) || defined(ESP_PLATFORM)
#ifdef BUILD_TARGET_DRYER

#include "DryerDevice.h"
#include <secrets.h>
#include <hal/hal_arduino.h>
#include <hal/hal_types.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>

namespace dryerlink {

DryerDevice::DryerDevice()
    : api_(&http_, IDRYER_API_BASE)
    , cloud_(&wifi_, &store_, &api_, &mqtt_)
    , uartSerial_(Serial1, 1)   // UART_NUM_1
{
    profile_ = new idryer::DryerProfile(&uartBridge_, &mqtt_, &cloud_);
    runtime_ = new idryer::IdryerRuntime(&cloud_, &dispatcher_, profile_, &mqtt_);
}

void DryerDevice::begin() {
    idryer::hal::initArduinoHal(&Serial);
    HAL_LOG_INFO("DRYER", "DryerDevice begin (UART RX=%d TX=%d)", DRYER_UART_RX, DRYER_UART_TX);

    // Two-MCU bridge: wait for RP2040 Hello before proceeding to Provisioning
    cloud_.setWaitForMcuSerial(true);

    // UART1 with build-configured pins
    Serial1.begin(115200, SERIAL_8N1, DRYER_UART_RX, DRYER_UART_TX);
    uartBridge_.begin(&uartSerial_, 115200);
    profile_->wireHandlers();
    wireClaimHandlers();

    // WiFi credentials
    {
        char ssid[64], pass[64];
        if (loadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
            HAL_LOG_INFO("DRYER", "WiFi from NVS: %s", ssid);
            wifi_.begin(ssid, pass);
        }
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
        else {
            saveWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
            wifi_.begin(WIFI_SSID, WIFI_PASSWORD);
        }
#endif
    }

    // NTP when WiFi connects; no setSerialFromMac() — serial comes from RP2040 Hello
    cloud_.setStateChangeCallback(onCloudStateChange, nullptr);

    cloud_.setUnclaimedCallback([](void* ctx) {
        static_cast<DryerDevice*>(ctx)->cloud_.requestClaim();
    }, this);

    runtime_->begin();
}

void DryerDevice::loop() {
    runtime_->loop();
}

bool DryerDevice::isOnline() const {
    return runtime_->isOnline();
}

void DryerDevice::setWifiCredentials(const char* ssid, const char* password) {
    saveWifiCredentials(ssid, password);
    wifi_.begin(ssid, password);
}

// ============================================================================
// Claim flow: RP2040 sends ClaimStart → ESP requests claim → sends PIN back
// ============================================================================

void DryerDevice::wireClaimHandlers() {
    uartBridge_.setClaimStartHandler([this](const idryer::UartFrameHeader&) {
        HAL_LOG_INFO("DRYER", "ClaimStart from MCU");
        cloud_.requestClaim();
    });

    cloud_.setClaimPinCallback([](const char* pin, uint32_t expiresIn, void* ctx) {
        auto* self = static_cast<DryerDevice*>(ctx);
        idryer::UartClaimStatusPayload p{};
        p.status           = idryer::UartClaimStatus::WaitingClaim;
        p.remainingSeconds = expiresIn;
        strncpy(p.pin, pin, sizeof(p.pin) - 1);
        self->uartBridge_.sendClaimStatus(p);
        HAL_LOG_INFO("DRYER", "Claim PIN forwarded to MCU: %s", pin);
    }, this);

    cloud_.setClaimCompleteCallback([](const char* deviceId, void* ctx) {
        auto* self = static_cast<DryerDevice*>(ctx);
        idryer::UartClaimCompletePayload p{};
        p.success = 1;
        strncpy(p.deviceId, deviceId, sizeof(p.deviceId) - 1);
        self->uartBridge_.sendClaimComplete(p);
        HAL_LOG_INFO("DRYER", "Claim complete, deviceId forwarded to MCU");
    }, this);
}

// static
void DryerDevice::onCloudStateChange(idryer::cloud::CloudState oldState,
                                      idryer::cloud::CloudState /*newState*/, void* /*ctx*/) {
    if (oldState == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        HAL_LOG_INFO("DRYER", "NTP sync started");
    }
}

// ============================================================================
// WiFi credentials NVS
// ============================================================================

void DryerDevice::saveWifiCredentials(const char* ssid, const char* password) {
    wifiPrefs_.begin("wifi", false);
    wifiPrefs_.putString("ssid",     ssid     ? ssid     : "");
    wifiPrefs_.putString("password", password ? password : "");
    wifiPrefs_.end();
}

bool DryerDevice::loadWifiCredentials(char* ssid, size_t ssidLen, char* password, size_t passLen) {
    wifiPrefs_.begin("wifi", true);
    size_t sLen = wifiPrefs_.getString("ssid",     ssid,     ssidLen);
    wifiPrefs_.getString("password", password, passLen);
    wifiPrefs_.end();
    if (sLen == 0) { ssid[0] = '\0'; password[0] = '\0'; return false; }
    return true;
}

} // namespace dryerlink

#endif // BUILD_TARGET_DRYER
#endif // ESP32 || ESP_PLATFORM
