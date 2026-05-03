#if defined(ESP32) || defined(ESP_PLATFORM)

#include "led_strip_executor.h"
#include "led_strip_animations.h"      // animationsOverride / animationsClearOverride
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

// led.pulse — два режима, выбираются по наличию ledIndex.
//   ZONE PULSE   — есть ledIndex: подсветить отрезок [ledIndex .. +ledCount-1].
//   GLOBAL OVERR — нет ledIndex, есть animation: запустить анимацию на всей ленте.
// Контракт payload — mqtt_contract.yaml::invoke_actions.storage_link.led.pulse.
void LedStripExecutor::handlePulse(JsonObjectConst args) {
    if (!args) { HAL_LOG_WARN("LED", "led.pulse: no args"); return; }

    // ── Mode detection ────────────────────────────────────────────────
    int ledIndex = args["ledIndex"] | -1;
    const char* animStr = args["animation"] | (const char*)nullptr;

    // ── Mode 2: GLOBAL ANIMATION OVERRIDE ─────────────────────────────
    // Сработает только если ledIndex отсутствует И есть animation.
    // Если оба есть — ZONE имеет приоритет (animation в зоне игнорируется).
    if (ledIndex < 0 && animStr) {
        if (strcmp(animStr, "off") == 0) {
            animationsOverride(false, AnimKind::Solid, CRGB::Black);
            HAL_LOG_INFO("LED", "animation override: OFF");
            return;
        }
        AnimKind kind;
        if (!parseAnimationName(animStr, kind)) {
            HAL_LOG_WARN("LED", "led.pulse: unknown animation='%s'", animStr);
            return;
        }
        const char* colorStr = args["color"] | "#FFFFFF";
        CRGB color = parseColor(colorStr, CRGB::White);
        animationsOverride(true, kind, color);
        HAL_LOG_INFO("LED", "animation override: %s color=#%02X%02X%02X",
                     animStr, color.r, color.g, color.b);
        return;
    }

    // ── Mode 1: ZONE PULSE ────────────────────────────────────────────
    // ledIndex обязателен (>=0). ledCount default 1 — backward compat.
    // color и durationSec — из payload или из меню (defaults).
    if (ledIndex < 0) {
        HAL_LOG_WARN("LED", "led.pulse: no ledIndex и no animation — пропуск");
        return;
    }

    int ledCount = args["ledCount"] | 1;
    if (ledCount < 1) ledCount = 1;

    int durationSec = defaultDurationS_;
    if (args["durationSec"].is<int>()) durationSec = args["durationSec"].as<int>();

    const char* colorStr = args["color"] | (const char*)nullptr;
    CRGB color = colorStr ? parseColor(colorStr, defaultColor_) : defaultColor_;

    if (ledsCount_ == 0 || (uint16_t)ledIndex >= ledsCount_) {
        HAL_LOG_WARN("LED", "led.pulse: index %d out of range (count=%u)", ledIndex, ledsCount_);
        return;
    }
    // Зажимаем хвост в границы ленты.
    if ((uint16_t)(ledIndex + ledCount) > ledsCount_) {
        ledCount = ledsCount_ - ledIndex;
    }

    HAL_LOG_INFO("LED", "led.pulse: zone=[%d..%d] dur=%ds color=#%02X%02X%02X",
                 ledIndex, ledIndex + ledCount - 1, durationSec,
                 color.r, color.g, color.b);

    // Гасим предыдущую активную зону (если была).
    if (activeStart_ >= 0) {
        for (uint16_t i = 0; i < activeCount_; i++) {
            uint16_t idx = (uint16_t)activeStart_ + i;
            if (idx < ledsCount_) leds_[idx] = CRGB::Black;
        }
    }

    if (durationSec == 0) {
        // Явное выключение зоны.
        activeStart_ = -1;
        activeCount_ = 0;
        offAt_       = 0;
    } else {
        for (int i = 0; i < ledCount; i++) {
            leds_[ledIndex + i] = color;
        }
        activeStart_ = ledIndex;
        activeCount_ = (uint16_t)ledCount;
        offAt_       = HAL_MILLIS() + (uint32_t)durationSec * 1000u;
    }

    FastLED.show();
}

void LedStripExecutor::handleAnimation(JsonObjectConst args) {
    HAL_LOG_INFO("LED", "led.animation: not implemented (use led.pulse with 'animation' field)");
}

void LedStripExecutor::turnOff() {
    if (activeStart_ >= 0) {
        for (uint16_t i = 0; i < activeCount_; i++) {
            uint16_t idx = (uint16_t)activeStart_ + i;
            if (idx < ledsCount_) leds_[idx] = CRGB::Black;
        }
        FastLED.show();
    }
    activeStart_ = -1;
    activeCount_ = 0;
    offAt_       = 0;
}

void LedStripExecutor::loop() {
    if (activeStart_ < 0) return;
    if (offAt_ != 0 && HAL_MILLIS() >= offAt_) {
        HAL_LOG_INFO("LED", "Pulse zone expired [%d..%d] — off",
                     (int)activeStart_, (int)activeStart_ + activeCount_ - 1);
        turnOff();
    }
}

uint32_t LedStripExecutor::getRemainingSeconds() const {
    if (activeStart_ < 0 || offAt_ == 0) return 0;
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
