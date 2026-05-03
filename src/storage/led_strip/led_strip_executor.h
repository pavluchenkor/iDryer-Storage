#pragma once

#include <ArduinoJson.h>
#include <FastLED.h>

// Executes commands/invoke actions for the addressable LED strip (WS2812B / APA102).
// Called by ActionDispatcher via the onInvoke handler in main.cpp.
//
// The CRGB array must be allocated externally (static or global) and passed to
// the constructor — required by FastLED's addLeds<>() API.
//
// Supported actions:
//   "led.pulse"     — light one LED for a duration
//     args: ledIndex (int), durationSec (int, 0=off), color (hex string, e.g. "#FF0000")
//   "led.animation" — run a strip animation (TBD)
class LedStripExecutor {
public:
    LedStripExecutor(CRGB* leds, uint16_t maxLeds);

    bool execute(const char* action, JsonObjectConst args);
    void loop();

    void setLedsCount(uint16_t count);
    void setMaxCurrentMa(uint16_t mA);
    void setBrightness(uint8_t brightness);   // верхний потолок; psu_ma режет дополнительно

    // Дефолты для led.pulse если args.color / args.durationSec не пришли.
    void setDefaultColor(CRGB color);
    void setDefaultDurationSec(uint16_t sec);

    int32_t  getActiveLed() const        { return activeLed_; }
    uint32_t getRemainingSeconds() const;
    bool     isPulseActive() const       { return activeLed_ >= 0; }

    // Доступ для анимационного движка (он пишет в leds[] когда нет активного pulse).
    CRGB*    leds()        const         { return leds_; }
    uint16_t ledsCount()   const         { return ledsCount_; }

private:
    void handlePulse(JsonObjectConst args);
    void handleAnimation(JsonObjectConst args);
    void turnOff();

    static CRGB parseColor(const char* hex, CRGB fallback = CRGB::White);

    CRGB*    leds_;
    uint16_t maxLeds_;
    uint16_t ledsCount_    = 0;
    uint16_t maxCurrentMa_ = 500;

    int32_t  activeLed_    = -1;
    uint32_t offAt_        = 0;

    // Defaults для led.pulse, если portal не передал args.color / args.durationSec.
    CRGB     defaultColor_     = CRGB::White;
    uint16_t defaultDurationS_ = 10;
};

