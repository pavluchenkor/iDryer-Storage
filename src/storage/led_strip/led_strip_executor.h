#pragma once

#include <ArduinoJson.h>
#include <FastLED.h>

// Выполняет commands/invoke action'ы для адресной LED-ленты Storage Link.
// Контракт payload — см. mqtt_contract.yaml::invoke_actions.storage_link.led.pulse.
//
// Два режима led.pulse:
//
//   1) ZONE PULSE — есть args.ledIndex.
//        ledIndex .. ledIndex+ledCount-1 → одним цветом на durationSec секунд.
//        Маппинг slot → LED-индексы делает портал.
//        ESP не знает про слоты, только про LED-индексы.
//
//   2) GLOBAL ANIMATION OVERRIDE — нет args.ledIndex, есть args.animation.
//        Запустить анимацию на ВСЕЙ ленте (любой #RRGGBB цвет).
//        Реализовано через animationsOverride() — RAM, не persist.
//        "animation":"off" — снять override.
//
// Пользовательские дефолты для зоны (color, durationSec) — из меню,
// инжектятся через setDefaultColor / setDefaultDurationSec.
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

    int32_t  getActiveLed()        const { return activeStart_; }   // legacy: первый LED активной зоны
    int32_t  getActiveStart()      const { return activeStart_; }
    uint16_t getActiveCount()      const { return activeCount_; }
    uint32_t getRemainingSeconds() const;
    bool     isPulseActive()       const { return activeStart_ >= 0; }

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

    // Активная zone-pulse область. -1/0 = нет активной зоны.
    int32_t  activeStart_  = -1;
    uint16_t activeCount_  = 0;
    uint32_t offAt_        = 0;

    // Defaults для led.pulse, если portal не передал args.color / args.durationSec.
    CRGB     defaultColor_     = CRGB::White;
    uint16_t defaultDurationS_ = 10;
};
