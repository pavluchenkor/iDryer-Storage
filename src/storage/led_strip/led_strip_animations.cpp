#if defined(ESP32) || defined(ESP_PLATFORM)

#include "led_strip_animations.h"
#include "led_strip_executor.h"
#include "led_strip_menu.h"

#include <Arduino.h>
#include <FastLED.h>

namespace {

// Тип анимации, синхронизированный с selectedAnimation() из led_strip_menu.cpp.
enum class Anim : uint8_t { Solid = 0, Breathe = 1, Wave = 2, Rainbow = 3 };

// ── State ────────────────────────────────────────────────────────────
LedStripExecutor* g_exec    = nullptr;
bool              g_enabled = false;
Anim              g_anim    = Anim::Solid;
CRGB              g_color   = CRGB::White;

// Render state.
uint32_t  g_lastFrameMs   = 0;
uint32_t  g_animStartMs   = 0;        // момент начала текущей анимации (для phase)
constexpr uint32_t kFrameIntervalMs = 33;   // ~30 fps

// Smooth crossfade при смене настроек: каждый кадр nblend от текущего к target.
// 8/255 ≈ ~32 кадра до полного слияния, что даёт ~500 мс при 30 fps.
constexpr fract8 kBlendStep = 32;

// Параметры анимаций (захардкожены, как договорились).
constexpr uint32_t kBreathePeriodMs   = 4000;   // 4 сек период дыхания
constexpr float    kBreatheMin        = 0.20f;  // 20% min яркости
constexpr float    kBreatheMax        = 1.00f;  // 100% max
constexpr uint32_t kWaveSpeedLedPerS  = 30;     // 30 LED/сек
constexpr uint8_t  kWaveTailLen       = 6;      // длина хвоста кометы
constexpr uint32_t kRainbowPeriodMs   = 10000;  // 10 сек на полный круг

// Сигнатура изменения состояния — чтобы решить, нужна ли смена animStartMs.
struct StateSig { bool enabled; uint8_t anim; uint32_t color; };
StateSig g_lastSig = { false, 0, 0 };

uint32_t crgbKey(const CRGB& c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

// ── Renderers ───────────────────────────────────────────────────────
// Каждый рендерер пишет в g_target[] (через локальный буфер) и main blend-loop
// плавно сливает g_target в реальный leds[].

void renderSolid(CRGB* dst, uint16_t n, uint32_t /*phaseMs*/, CRGB color) {
    fill_solid(dst, n, color);
}

void renderBreathe(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    // sin8 даёт 0..255 от phaseMs / period. Затем линейно перемасштабируем
    // в [kBreatheMin..kBreatheMax].
    uint8_t s = sin8((uint16_t)((uint64_t)phaseMs * 256u / kBreathePeriodMs));  // 0..255
    float amp = kBreatheMin + (kBreatheMax - kBreatheMin) * (s / 255.0f);
    CRGB c = color;
    c.nscale8_video((uint8_t)(amp * 255.0f));
    fill_solid(dst, n, c);
}

void renderWave(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    // Положение «головы» кометы вдоль ленты (модуль ledsCount).
    uint32_t pos = ((uint64_t)phaseMs * kWaveSpeedLedPerS / 1000u) % (uint32_t)n;
    fill_solid(dst, n, CRGB::Black);
    for (uint8_t t = 0; t < kWaveTailLen; t++) {
        int32_t idx = (int32_t)pos - (int32_t)t;
        if (idx < 0) idx += n;
        // Затухание: голова на 100%, хвост линейно к 0.
        uint8_t scale = (uint8_t)(255 - (255 * t / kWaveTailLen));
        CRGB c = color;
        c.nscale8_video(scale);
        dst[idx] = c;
    }
}

void renderRainbow(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB /*color*/) {
    uint8_t hueOffset = (uint8_t)((uint64_t)phaseMs * 256u / kRainbowPeriodMs);
    fill_rainbow(dst, n, hueOffset, 256 / (n > 0 ? n : 1));
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────

void animationsWire(LedStripExecutor* exec) {
    g_exec = exec;
}

void animationsApply() {
    g_enabled = isIdleEnabled();
    g_anim    = (Anim)selectedAnimation();
    g_color   = selectedIdleColor();

    StateSig sig = { g_enabled, (uint8_t)g_anim, crgbKey(g_color) };
    bool changed = (sig.enabled != g_lastSig.enabled) ||
                   (sig.anim    != g_lastSig.anim) ||
                   (sig.color   != g_lastSig.color);
    if (changed) {
        g_animStartMs = millis();          // перезапустить фазу анимации
        g_lastSig     = sig;
    }
}

void animationsLoop(uint32_t nowMs) {
    if (!g_exec) return;

    // Pulse имеет приоритет — пока активен, не трогаем ленту.
    if (g_exec->isPulseActive()) {
        g_lastFrameMs = 0;                 // при возврате в idle сразу нарисуем
        return;
    }

    // Дросселируем кадры до ~30 fps.
    if (nowMs - g_lastFrameMs < kFrameIntervalMs) return;
    g_lastFrameMs = nowMs;

    CRGB*    leds = g_exec->leds();
    uint16_t n    = g_exec->ledsCount();
    if (!leds || n == 0) return;

    // Если анимация выключена — целевая чёрная (плавно гаснем).
    static CRGB target[300];               // STORAGE_MAX_LEDS — TODO унификация
    if (n > 300) n = 300;

    if (!g_enabled) {
        fill_solid(target, n, CRGB::Black);
    } else {
        const uint32_t phase = nowMs - g_animStartMs;
        switch (g_anim) {
            case Anim::Solid:   renderSolid(target,   n, phase, g_color); break;
            case Anim::Breathe: renderBreathe(target, n, phase, g_color); break;
            case Anim::Wave:    renderWave(target,    n, phase, g_color); break;
            case Anim::Rainbow: renderRainbow(target, n, phase, g_color); break;
        }
    }

    // Плавный кроссфейд из текущего leds[] в target[]. ~500 мс при 30 fps.
    nblend(leds, target, n, kBlendStep);
    FastLED.show();
}

#endif // ESP32 || ESP_PLATFORM
