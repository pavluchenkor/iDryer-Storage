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

// Пульсация активной pulse-зоны (led.pulse): синус яркости между
// Min и Max за период. Подбираются по вкусу.
constexpr uint32_t kZonePulsePeriodMs = 1200;   // период полного цикла, мс
constexpr uint8_t  kZonePulseMin      = 70;     // нижний уровень яркости (0-255)
constexpr uint8_t  kZonePulseMax      = 255;    // верхний уровень яркости (0-255)

constexpr uint32_t kBreathePeriodMs   = 4000;   // 4 сек период дыхания
constexpr float    kBreatheMin        = 0.20f;  // 20% min яркости
constexpr float    kBreatheMax        = 1.00f;  // 100% max
constexpr uint32_t kWaveSpeedLedPerS  = 30;     // 30 LED/сек
constexpr uint8_t  kWaveTailDiv       = 4;      // хвост кометы — четверть ленты
constexpr uint32_t kRainbowPeriodMs   = 120000; // 2 мин на полный круг
constexpr uint32_t kRainbowSpan16     = 65536 / 3;  // вся лента — треть кольца hue
constexpr uint32_t kCyclePeriodMs     = 60000;  // 60 сек на полный круг цвета

// Aurora: пятна близких к выбранному цвету оттенков медленно плывут по ленте.
constexpr uint16_t kAuroraScale       = 12;     // шаг шума на LED: пятно ~20 LED
constexpr uint32_t kAuroraCellMs      = 8000;   // скорость перетекания пятен
constexpr uint8_t  kAuroraHueSpread   = 24;     // отклонение hue от выбранного, ±
constexpr uint8_t  kAuroraMinV        = 40;     // яркость самых тёмных мест (0-255)

// Candle: тёплый свет, яркость и оттенок неравномерно колеблются.
constexpr uint16_t kCandleScale       = 48;     // шаг шума на LED: «язычок» ~5 LED
constexpr uint32_t kCandleCellMs      = 700;    // скорость мерцания
constexpr uint8_t  kCandleHueLow      = 12;     // тусклее — краснее
constexpr uint8_t  kCandleHueHigh     = 28;     // ярче — желтее
constexpr uint8_t  kCandleSat         = 220;
constexpr uint8_t  kCandleMinV        = 110;

// Ocean / Lava / Forest: палитра FastLED, раскинутая по ленте шумом.
constexpr uint16_t kPaletteScale      = 10;     // шаг шума на LED: пятно ~25 LED
constexpr uint32_t kPaletteCellMs     = 10000;  // скорость перетекания пятен
constexpr uint32_t kPaletteDriftMs    = 120000; // сдвиг по всей палитре за 2 мин

// Swell: по ленте выбранного цвета бежит синус яркости.
constexpr uint8_t  kSwellWaves        = 4;      // волн на ленту
constexpr uint32_t kSwellPeriodMs     = 9000;   // волна сдвигается на свою длину
constexpr uint8_t  kSwellMinV         = 100;    // яркость во впадине, ~40%

// Ripple: две встречные синусоиды яркости разной длины.
constexpr uint8_t  kRippleWavesA      = 3;
constexpr uint32_t kRipplePeriodAMs   = 11000;
constexpr uint8_t  kRippleWavesB      = 5;
constexpr uint32_t kRipplePeriodBMs   = 7000;
constexpr uint8_t  kRippleMinV        = 60;

// Spotlight: светлое окно ходит туда-обратно по тусклому фону.
constexpr uint8_t  kSpotWidthDiv      = 4;      // ширина окна — четверть ленты
constexpr uint32_t kSpotPeriodMs      = 20000;  // туда и обратно
constexpr uint8_t  kSpotBaseV         = 40;     // яркость фона, ~15%

// Duo: градиент от выбранного цвета к соседнему по кольцу плывёт по ленте.
constexpr uint8_t  kDuoHueShift       = 30;     // второй цвет: +30 из 256 (~40°)
constexpr uint32_t kDuoPeriodMs       = 20000;  // градиент проходит всю ленту

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
    uint32_t pos  = ((uint64_t)phaseMs * kWaveSpeedLedPerS / 1000u) % (uint32_t)n;
    uint16_t tail = n / kWaveTailDiv;
    if (tail == 0) tail = 1;
    fill_solid(dst, n, CRGB::Black);
    for (uint16_t t = 0; t < tail; t++) {
        int32_t idx = (int32_t)pos - (int32_t)t;
        if (idx < 0) idx += n;
        uint8_t scale = (uint8_t)(255 - (255 * t / tail));
        CRGB c = color;
        c.nscale8_video(scale);
        dst[idx] = c;
    }
}

// Положение внутри периода: 0..65535.
uint16_t phase16(uint32_t phaseMs, uint32_t periodMs) {
    return (uint16_t)((uint64_t)(phaseMs % periodMs) * 65536u / periodMs);
}

// Координата времени для inoise8: одна ячейка шума (256) за cellMs.
// Переполнение uint16 скачка не даёт: шум FastLED периодичен по 65536.
uint16_t noiseTime(uint32_t phaseMs, uint32_t cellMs) {
    return (uint16_t)((uint64_t)phaseMs * 256u / cellMs);
}

// Угол синуса (0..255) для LED i: waves волн на ленту, сдвинутых на shift.
uint8_t waveAngle(uint16_t i, uint16_t n, uint8_t waves, uint16_t shift) {
    return (uint16_t)((uint32_t)i * 65536u * waves / n - shift) >> 8;
}

// Цвет по 16-битному hue. При медленной смене шаг 8-битного hue виден
// ступенькой, поэтому соседние hue смешиваются по младшему байту.
CRGB hueColor(uint16_t hue16) {
    uint8_t hue = hue16 >> 8;
    return blend(CRGB(CHSV(hue, 255, 255)), CRGB(CHSV((uint8_t)(hue + 1), 255, 255)), hue16 & 0xFF);
}

void renderRainbow(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB /*color*/) {
    uint16_t offset = phase16(phaseMs, kRainbowPeriodMs);
    for (uint16_t i = 0; i < n; i++) {
        dst[i] = hueColor(offset + (uint16_t)((uint32_t)i * kRainbowSpan16 / n));
    }
}

// Cycle: вся лента одним цветом, hue медленно идёт по кругу.
void renderCycle(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB /*color*/) {
    fill_solid(dst, n, hueColor(phase16(phaseMs, kCyclePeriodMs)));
}

// Aurora: один слой шума сдвигает hue вокруг выбранного цвета, второй —
// яркость. Белый цвет даёт только игру яркости (насыщенность 0).
void renderAurora(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    CHSV     base = rgb2hsv_approximate(color);
    uint16_t t    = noiseTime(phaseMs, kAuroraCellMs);
    for (uint16_t i = 0; i < n; i++) {
        uint16_t x  = i * kAuroraScale;
        int16_t  dh = ((int16_t)inoise8(x, t) - 128) * kAuroraHueSpread / 128;
        uint8_t  v  = lerp8by8(kAuroraMinV, 255, inoise8(x + 20000, t + 10000));
        dst[i] = CHSV((uint8_t)(base.hue + dh), base.sat, v);
    }
}

void renderCandle(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB /*color*/) {
    uint16_t t = noiseTime(phaseMs, kCandleCellMs);
    for (uint16_t i = 0; i < n; i++) {
        uint8_t f = inoise8(i * kCandleScale, t);
        dst[i] = CHSV(lerp8by8(kCandleHueLow, kCandleHueHigh, f), kCandleSat,
                      lerp8by8(kCandleMinV, 255, f));
    }
}

void renderSwell(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    uint16_t shift = phase16(phaseMs, kSwellPeriodMs);
    for (uint16_t i = 0; i < n; i++) {
        CRGB c = color;
        c.nscale8_video(lerp8by8(kSwellMinV, 255, sin8(waveAngle(i, n, kSwellWaves, shift))));
        dst[i] = c;
    }
}

void renderRipple(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    uint16_t shiftA = phase16(phaseMs, kRipplePeriodAMs);
    uint16_t shiftB = (uint16_t)(0u - phase16(phaseMs, kRipplePeriodBMs));  // встречная
    for (uint16_t i = 0; i < n; i++) {
        uint8_t a = sin8(waveAngle(i, n, kRippleWavesA, shiftA));
        uint8_t b = sin8(waveAngle(i, n, kRippleWavesB, shiftB));
        CRGB c = color;
        c.nscale8_video(lerp8by8(kRippleMinV, 255, avg8(a, b)));
        dst[i] = c;
    }
}

// Центр окна движется по синусу: у краёв замедляется, разворот плавный.
// Координаты в 1/256 LED — окно смещается без рывков на шаг диода.
void renderSpotlight(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    uint32_t center = ((uint32_t)(sin16(phase16(phaseMs, kSpotPeriodMs)) + 32768) * (n - 1)) >> 8;
    uint32_t half   = (uint32_t)n * 256u / (kSpotWidthDiv * 2);
    for (uint16_t i = 0; i < n; i++) {
        uint32_t x = (uint32_t)i * 256u;
        uint32_t d = x > center ? x - center : center - x;
        uint8_t  v = kSpotBaseV;
        if (d < half) v = lerp8by8(kSpotBaseV, 255, cos8((uint8_t)(d * 128u / half)));
        CRGB c = color;
        c.nscale8_video(v);
        dst[i] = c;
    }
}

// Второй цвет — соседний по кольцу. У белого насыщенность 0, соседа нет:
// лента горит ровным белым.
void renderDuo(CRGB* dst, uint16_t n, uint32_t phaseMs, CRGB color) {
    CHSV     hsv   = rgb2hsv_approximate(color);
    CRGB     pair  = CHSV((uint8_t)(hsv.hue + kDuoHueShift), hsv.sat, hsv.val);
    uint16_t shift = phase16(phaseMs, kDuoPeriodMs);
    for (uint16_t i = 0; i < n; i++) {
        dst[i] = blend(color, pair, sin8(waveAngle(i, n, 1, shift)));
    }
}

void renderPalette(CRGB* dst, uint16_t n, uint32_t phaseMs, const CRGBPalette16& pal) {
    uint16_t t     = noiseTime(phaseMs, kPaletteCellMs);
    uint8_t  drift = phase16(phaseMs, kPaletteDriftMs) >> 8;
    for (uint16_t i = 0; i < n; i++) {
        uint8_t idx = inoise8(i * kPaletteScale, t) + drift;
        dst[i] = ColorFromPalette(pal, idx, 255, LINEARBLEND);
    }
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
    if (strcmp(name, "cycle")   == 0) { out = AnimKind::Cycle;   return true; }
    if (strcmp(name, "aurora")  == 0) { out = AnimKind::Aurora;  return true; }
    if (strcmp(name, "candle")  == 0) { out = AnimKind::Candle;  return true; }
    if (strcmp(name, "ocean")   == 0) { out = AnimKind::Ocean;   return true; }
    if (strcmp(name, "lava")    == 0) { out = AnimKind::Lava;    return true; }
    if (strcmp(name, "forest")  == 0) { out = AnimKind::Forest;  return true; }
    if (strcmp(name, "swell")     == 0) { out = AnimKind::Swell;     return true; }
    if (strcmp(name, "ripple")    == 0) { out = AnimKind::Ripple;    return true; }
    if (strcmp(name, "spotlight") == 0) { out = AnimKind::Spotlight; return true; }
    if (strcmp(name, "duo")       == 0) { out = AnimKind::Duo;       return true; }
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

bool animationsIsOverrideActive() {
    return g_overrideActive && g_overrideEnabled;
}

// Лента заморожена (идёт обновление прошивки): кадры не считаем, show() не
// зовём. Снимается только animationsResume().
static bool g_frozen = false;

void animationsHoldStatic(const CRGB& color) {
    g_frozen = true;
    if (!g_exec) return;
    CRGB*    leds = g_exec->leds();
    uint16_t n    = g_exec->ledsCount();
    if (!leds || n == 0) return;
    fill_solid(leds, n, color);
    FastLED.show();   // единственный вызов за всё время обновления
}

void animationsResume() {
    g_frozen = false;
    animationsApply();
}

void animationsLoop(uint32_t nowMs) {
    if (g_frozen) return;
    if (!g_exec) return;

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
            case AnimKind::Cycle:   renderCycle(target,   n, phase, g_color); break;
            case AnimKind::Aurora:  renderAurora(target,  n, phase, g_color); break;
            case AnimKind::Candle:  renderCandle(target,  n, phase, g_color); break;
            case AnimKind::Ocean:   renderPalette(target, n, phase, OceanColors_p);  break;
            case AnimKind::Lava:    renderPalette(target, n, phase, LavaColors_p);   break;
            case AnimKind::Forest:  renderPalette(target, n, phase, ForestColors_p); break;
            case AnimKind::Swell:     renderSwell(target,     n, phase, g_color); break;
            case AnimKind::Ripple:    renderRipple(target,    n, phase, g_color); break;
            case AnimKind::Spotlight: renderSpotlight(target, n, phase, g_color); break;
            case AnimKind::Duo:       renderDuo(target,       n, phase, g_color); break;
        }
    }

    // Плавный кроссфейд из текущего leds[] в target[]. ~500 мс при 30 fps.
    // Вытесненная или истёкшая pulse-зона растворяется в фон этим же nblend.
    nblend(leds, target, n, kBlendStep);

    // Активная pulse-зона рисуется поверх напрямую (мимо nblend, иначе он
    // сгладил бы пульсацию): яркость дышит синусом Min..Max за PeriodMs.
    if (g_exec->isPulseActive()) {
        int32_t  start = g_exec->getActiveStart();
        uint16_t count = g_exec->getActiveCount();
        CRGB     color = g_exec->getActiveColor();
        uint8_t  s     = sin8((uint16_t)((uint64_t)nowMs * 256u / kZonePulsePeriodMs));
        uint8_t  scale = kZonePulseMin +
                         (uint8_t)(((uint16_t)(kZonePulseMax - kZonePulseMin) * s) / 255);
        color.nscale8_video(scale);
        for (uint16_t i = 0; i < count; i++) {
            uint32_t idx = (uint32_t)start + i;
            if (idx < n) leds[idx] = color;
        }
    }

    FastLED.show();
}

#endif // ESP32 || ESP_PLATFORM
