// Шаг 2: baseline (s_link) + bootstrapMenu-эмуляция (NVS namespace кепт открытым).
// Storage main.cpp начинает с bootstrapMenu() — это открывает Preferences
// в namespace "iheater-menu" и НЕ зовёт end(). Имитируем здесь.

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <FastLED.h>
#include <SHT31.h>
#include <iDryer.h>

static CRGB g_leds[300];
static SHT31 g_sht(0x44, &Wire);

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

    // Бисект шаг 5: + Wire.begin (I2C SDA=8 SCL=9) — Storage pins.
    Wire.begin(8, 9);

    // Бисект шаг 7: + FastLED.addLeds (WS2812B на GPIO4, как Storage).
    FastLED.addLeds<WS2812B, 4, GRB>(g_leds, 300);

    // Бисект шаг 6: + SHT31.begin (вместо просто ctor).
    g_sht.begin();


    // Бисект шаг 1: + onClaimPin (stateless лямбда → fnptr, без std::function).
    s_link.onClaimPin([](const char* pin, uint32_t expires) {
        Serial.printf("CLAIM_PIN:%s:%lu\n", pin, expires);
        Serial.flush();
    });

    s_link.begin();

    // Бисект шаг 2: + onCommand("get_config") — stateless лямбда.
    s_link.onCommand("get_config", [](JsonObjectConst) {
        Serial.println("[CMD] get_config");
    });

    // Бисект шаг 3: + onCommand("set").
    s_link.onCommand("set", [](JsonObjectConst) {
        Serial.println("[CMD] set");
    });

    // Бисект шаг 4: + onCommand("invoke").
    s_link.onCommand("invoke", [](JsonObjectConst) {
        Serial.println("[CMD] invoke");
    });
}

void loop() {
    s_link.loop();
}
