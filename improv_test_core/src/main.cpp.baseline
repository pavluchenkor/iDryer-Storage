// Шаг 2: baseline (s_link) + bootstrapMenu-эмуляция (NVS namespace кепт открытым).
// Storage main.cpp начинает с bootstrapMenu() — это открывает Preferences
// в namespace "iheater-menu" и НЕ зовёт end(). Имитируем здесь.

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <iDryer.h>

static Preferences g_menu_prefs;

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasHeaterPower    = true,
    .hasFanStatus      = false,
    .hasLed            = false,
    .hasScales         = false,
    .hasRfid           = false,
    .hasAirTemp        = false,
    .hasAirHumidity    = false,
    .hasHeaterTemp     = false,
    .allowHa           = true,
    .allowBambu        = true,
    .allowMoonraker    = true,
    .telemetryPeriodMs = 0,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
    .model             = "iDryer Bisect",
};
static iDryer::Link s_link(CFG);

void setup() {
    // ВАЖНО — порядок: WiFi.persistent(false) ДО любых Preferences/NVS-операций
    // продукта (bootstrapMenu и т.д.). Иначе Arduino пишет WiFi-config в NVS
    // partition внутри WiFi.begin() (вызываемого позже из Improv tryConnectToWifi),
    // конфликтует с открытым menu-write-handle и Improv таймаутит на PROVISIONING.
    WiFi.persistent(false);

    g_menu_prefs.begin("iheater-menu", /*readOnly=*/false);
    s_link.begin();
}

void loop() {
    s_link.loop();
}
