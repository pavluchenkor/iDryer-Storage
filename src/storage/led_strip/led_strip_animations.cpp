#if defined(ESP32) || defined(ESP_PLATFORM)

#include "led_strip_animations.h"
#include "led_strip_executor.h"
#include "led_strip_menu.h"

#include <Arduino.h>
#include <FastLED.h>
#include <string.h>

namespace {

// ── State ────────────────────────────────────────────────────────────
LedStripExecutor* g_exec = nullptr;

// Текущие параметры рендера: override если активен, иначе берётся из меню.
bool      g_enabled = false;
AnimKind  g_anim    = AnimKind::Solid;
CRGB      g_color   = CRGB::White;

// Временный override (не сохраняется в NVS): перекрывает меню пока g_overrideActive.
bool      g_overrideActive  = false;
bool      g_overrideEnabled = false;
AnimKind  g_overrideAnim    = AnimKind::Solid;
CRGB      g_overrideColor   = CRGB::White;

// Render state.
uint32_t  g_lastFrameMs = 0;
uint32_t  g_animStartMs = 0;        // момент начала текущей анимации (для phase)
constexpr uint32_t kFrameIntervalMs = 33;   // ~30 fps

// Плавный переход цвета при смене настроек: nblend на 32/255 за кадр → ~500 мс при 30 fps.
constexpr fract8 kBlendStep = 32;

// Параметры анимаций (захардкожены).
constexpr uint32_t kBreathePeriodMs   = 4000;   // 4 сек период дыхания
constexpr float    kBreatheMin        = 0.20f;  // 20% min яркости
constexpr float    kBreatheMax        = 1.00f;  // 100% max
constexpr uint32_t kWaveSpeedLedPerS  = 30;     // 30 LED/сек
constexpr uint8_t  kWaveTailLen       = 6;      // длина хвоста кометы
constexpr uint32_t kRainbowPeriodMs   = 10000;  // 10 сек на полный круг

// Сигнатура для определения «состояние сменилось» — нужно перезапустить фазу.
struct StateSig { bool enabled; uint8_t anim; uint32_t color; };
StateSig g_lastSig = { false, 0, 0 };

uint32_t crgbKey(const CRGB& c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

// Копирует g_enabled/g_anim/g_color из override (если активен) или из меню; перезапускает фазу при смене.
void recomputeEffective() {
    if (g_overrideActive) {
        g_enabled = g_overrideEnabled;
        g_anim    = g_overrideAnim;
        g_color   = g_overrideColor;
    } else {
        g_enabled = isIdleEnabled();
        g_anim    = (AnimKind)selectedAnimation();
        g_color   = selectedIdleColor();
    }
    StateSig sig = { g_enabled, (uint8_t)g_anim, crgbKey(g_color) };
    if (sig.enabled != g_lastSig.enabled ||
        sig.anim    != g_lastSig.anim    ||
        sig.color   != g_lastSig.color) {
        g_animStartMs = millis();
        g_lastSig     = sig;
    }
}

// ── Renderers ───────────────────────────────────────────────────────
// Каждый рендерер заполняет target[], затем nblend плавно переводит target в leds[].

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
    uint32_t pos = ((uint64_t)phaseMs * kWaveSpeedLedPerS / 1000u) % (uint32_t)n;
    fill_solid(dst, n, CRGB::Black);
    for (uint8_t t = 0; t < kWaveTailLen; t++) {
        int32_t idx = (int32_t)pos - (int32_t)t;
        if (idx < 0) idx += n;
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

// Twinkle: тёмный фон (~10% яркости) + редкие случайные вспышки LED с плавным угасанием за kTwinkleFadeMs.
constexpr uint8_t  kTwinkleBaseScale = 26;     // ~10% яркости (26/255)
constexpr uint16_t kTwinkleSpawnThr  = 100;    // ~0.15% / LED / frame
constexpr uint32_t kTwinkleFadeMs    = 1200;   // fade длится 1.2 сек

uint32_t g_twinkleStartMs[300] = {0};   // STORAGE_MAX_LEDS — TODO унификация

void renderTwinkle(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    // Шумоподобный rand для решения «зажечь ли этот LED в этом кадре».
    // FastLED.random16() — fast, ок для визуала.
    CRGB base = color;
    base.nscale8_video(kTwinkleBaseScale);

    for (uint16_t i = 0; i < n; i++) {
        // Решение spawn'а — только если этот LED сейчас в покое (или fade завершён).
        bool inFade = g_twinkleStartMs[i] != 0 &&
                      (phaseMs - g_twinkleStartMs[i]) < kTwinkleFadeMs;
        if (!inFade) {
            if (random16() < kTwinkleSpawnThr) {
                g_twinkleStartMs[i] = phaseMs == 0 ? 1 : phaseMs;
                inFade = true;
            }
        }

        if (inFade) {
            uint32_t age = phaseMs - g_twinkleStartMs[i];
            // Линейный fade от 255 (пик) до kTwinkleBaseScale (база) за kTwinkleFadeMs.
            uint8_t scale = 255 - (uint8_t)((255 - kTwinkleBaseScale) * age / kTwinkleFadeMs);
            CRGB c = color;
            c.nscale8_video(scale);
            dst[i] = c;
            if (age >= kTwinkleFadeMs) g_twinkleStartMs[i] = 0;  // fade завершён
        } else {
            dst[i] = base;
        }
    }
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────

bool parseAnimationName(const char* name, AnimKind& out) {
    if (!name) return false;
    if (strcmp(name, "solid")   == 0) { out = AnimKind::Solid;   return true; }
    if (strcmp(name, "breathe") == 0) { out = AnimKind::Breathe; return true; }
    if (strcmp(name, "wave")    == 0) { out = AnimKind::Wave;    return true; }
    if (strcmp(name, "rainbow") == 0) { out = AnimKind::Rainbow; return true; }
    if (strcmp(name, "twinkle") == 0) { out = AnimKind::Twinkle; return true; }
    return false;
}

void animationsWire(LedStripExecutor* exec) {
    g_exec = exec;
}

void animationsApply() {
    // Любой menu-update снимает invoke-override (контракт).
    g_overrideActive = false;
    recomputeEffective();
}

void animationsOverride(bool enabled, AnimKind anim, const CRGB& color) {
    g_overrideActive  = true;
    g_overrideEnabled = enabled;
    g_overrideAnim    = anim;
    g_overrideColor   = color;
    recomputeEffective();
}

void animationsClearOverride() {
    g_overrideActive = false;
    recomputeEffective();
}

void animationsLoop(uint32_t nowMs) {
    if (!g_exec) return;

    // Pulse-зона имеет приоритет — пока активна, не трогаем ленту.
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

    static CRGB target[300];               // STORAGE_MAX_LEDS — TODO унификация
    if (n > 300) n = 300;

    if (!g_enabled) {
        fill_solid(target, n, CRGB::Black);
    } else {
        const uint32_t phase = nowMs - g_animStartMs;
        switch (g_anim) {
            case AnimKind::Solid:   renderSolid(target,   n, phase, g_color); break;
            case AnimKind::Breathe: renderBreathe(target, n, phase, g_color); break;
            case AnimKind::Wave:    renderWave(target,    n, phase, g_color); break;
            case AnimKind::Rainbow: renderRainbow(target, n, phase, g_color); break;
            case AnimKind::Twinkle: renderTwinkle(target, n, phase, g_color); break;
        }
    }

    // Плавный кроссфейд из текущего leds[] в target[]. ~500 мс при 30 fps.
    nblend(leds, target, n, kBlendStep);
    FastLED.show();
}

#endif // ESP32 || ESP_PLATFORM
