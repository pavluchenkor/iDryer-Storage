// ============================================================
// DRYER LINK ENTRYPOINT — boot path для iDryer Dryer Link.
// Стек: DryerMain → DryerDevice → idryer-core (lib/idryer-core/).
// UART: Serial1 на пинах DRYER_UART_RX / DRYER_UART_TX (build_flags).
// Собирается только env esp32c3-dryer-*.
// ============================================================

#if defined(ESP32) || defined(ESP_PLATFORM)
#ifdef BUILD_TARGET_DRYER

#include <Arduino.h>
#include <ImprovWiFiLibrary.h>
#include "DryerDevice.h"

static dryerlink::DryerDevice s_device;
static ImprovWiFi s_improv(&Serial);

static void onImprovWifiConnected(const char* ssid, const char* password) {
    s_device.setWifiCredentials(ssid, password);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    s_improv.setDeviceInfo(
        ImprovTypes::ChipFamily::CF_ESP32_C3,
        "iDryer Dryer Link",
        "1.0.0",
        "DryerLink"
    );
    s_improv.onImprovConnected(onImprovWifiConnected);

    s_device.begin();
}

void loop() {
    s_improv.handleSerial();
    s_device.loop();
}

#endif // BUILD_TARGET_DRYER
#endif // ESP32 || ESP_PLATFORM
