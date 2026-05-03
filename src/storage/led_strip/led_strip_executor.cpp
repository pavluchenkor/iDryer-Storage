#if defined(ESP32) || defined(ESP_PLATFORM)

#include "led_strip_executor.h"
#include <hal/hal_types.h>
#include <string.h>
#include <stdlib.h>


LedStripExecutor::LedStripExecutor(CRGB* leds, uint16_t maxLeds)
    : leds_(leds), maxLeds_(maxLeds)
{}

void LedStripExecutor::setLedsCount(uint16_t count) {
    ledsCount_ = (count > maxLeds_) ? maxLeds_ : count;
    // Update FastLED controller so it drives exactly ledsCount_ pixels on the wire.
    if (FastLED.count() > 0) {
        FastLED[0].setLeds(leds_, ledsCount_);
    }
    HAL_LOG_INFO("LED", "ledsCount=%u", ledsCount_);
}

void LedStripExecutor::setMaxCurrentMa(uint16_t mA) {
    maxCurrentMa_ = mA;
    FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrentMa_);
    HAL_LOG_INFO("LED", "maxCurrentMa=%u mA", maxCurrentMa_);
}

void LedStripExecutor::setBrightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
    HAL_LOG_INFO("LED", "brightness=%u", (unsigned)brightness);
}

void LedStripExecutor::setDefaultColor(CRGB color) {
    defaultColor_ = color;
}

void LedStripExecutor::setDefaultDurationSec(uint16_t sec) {
    if (sec > 0) defaultDurationS_ = sec;
}

bool LedStripExecutor::execute(const char* action, JsonObjectConst args) {
    if (strcmp(action, "led.pulse") == 0)     { handlePulse(args);     return true; }
    if (strcmp(action, "led.animation") == 0) { handleAnimation(args); return true; }
    return false;
}

void LedStripExecutor::handlePulse(JsonObjectConst args) {
    if (!args) { HAL_LOG_WARN("LED", "led.pulse: no args"); return; }

    // ledIndex обязателен; durationSec и color имеют default из меню (через
    // setDefaultColor / setDefaultDurationSec — main.cpp обновляет их при
    // загрузке меню и при изменении).
    int ledIndex = args["ledIndex"] | -1;

    int durationSec = defaultDurationS_;
    if (args["durationSec"].is<int>()) durationSec = args["durationSec"].as<int>();

    const char* colorStr = args["color"] | (const char*)nullptr;
    CRGB color = colorStr ? parseColor(colorStr, defaultColor_) : defaultColor_;

    if (ledIndex < 0 || ledsCount_ == 0 || (uint16_t)ledIndex >= ledsCount_) {
        HAL_LOG_WARN("LED", "led.pulse: index %d out of range (count=%u)", ledIndex, ledsCount_);
        return;
    }

    HAL_LOG_INFO("LED", "led.pulse: index=%d dur=%ds color=#%02X%02X%02X",
                 ledIndex, durationSec, color.r, color.g, color.b);

    if (activeLed_ >= 0 && (uint16_t)activeLed_ < ledsCount_) leds_[activeLed_] = CRGB::Black;

    if (durationSec == 0) {
        activeLed_ = -1;
        offAt_     = 0;
    } else {
        leds_[ledIndex] = color;
        activeLed_ = ledIndex;
        offAt_     = HAL_MILLIS() + (uint32_t)durationSec * 1000u;
    }

    FastLED.show();
}

void LedStripExecutor::handleAnimation(JsonObjectConst args) {
    HAL_LOG_INFO("LED", "led.animation: not implemented in v1");
}

void LedStripExecutor::turnOff() {
    if (activeLed_ >= 0 && (uint16_t)activeLed_ < ledsCount_) {
        leds_[activeLed_] = CRGB::Black;
        FastLED.show();
    }
    activeLed_ = -1;
    offAt_     = 0;
}

void LedStripExecutor::loop() {
    if (activeLed_ < 0) return;
    if (offAt_ != 0 && HAL_MILLIS() >= offAt_) {
        HAL_LOG_INFO("LED", "Timer expired, turning off LED %d", activeLed_);
        turnOff();
    }
}

uint32_t LedStripExecutor::getRemainingSeconds() const {
    if (activeLed_ < 0 || offAt_ == 0) return 0;
    uint32_t now = HAL_MILLIS();
    return (now >= offAt_) ? 0 : (offAt_ - now) / 1000u;
}

CRGB LedStripExecutor::parseColor(const char* hex, CRGB fallback) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return fallback;
    char buf[7];
    memcpy(buf, hex + 1, 6);
    buf[6] = '\0';
    unsigned long val = strtoul(buf, nullptr, 16);
    return CRGB((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
}


#endif // ESP32 || ESP_PLATFORM
