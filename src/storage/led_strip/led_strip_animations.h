#pragma once

// Фоновые анимации Storage Link.
//
// ИСТОЧНИКИ СОСТОЯНИЯ — два:
//
//   1) Меню (NVS): idle_enabled + anim_* toggle group + idle_*color toggle group.
//      Применяется через animationsApply() — на boot и по событию menu changed.
//      Цвет — из 8-цветной палитры меню.
//
//   2) Override (RAM): через animationsOverride(), не persist.
//      Срабатывает по invoke `led.pulse {animation:...}` от портала.
//      Цвет — произвольный #RRGGBB.
//      Снимается:
//        • явно: animationsClearOverride() либо animationsOverride(false,...).
//        • неявно: при следующем animationsApply() (т.е. при правке меню).
//        • при reboot — состояние теряется (RAM).
//
//   При активном override анимация рендерится по override-state, иначе — по меню.
//
// PULSE-зона (executor.isPulseActive()) имеет приоритет над всем фоном —
// пока активна, движок не пишет в leds[] и не вызывает FastLED.show().
//
// Анимации:
//   Solid    — постоянный цвет.
//   Breathe  — синусоидное дыхание яркости 20%↔100%, период 4 сек.
//   Wave     — бегущая «комета» цвета, ~30 LED/сек, fade-tail.
//   Rainbow  — HSV-проход всего hue, период 10 сек (color игнорируется).
//   Twinkle  — мягкие случайные «звёзды» базовым цветом — спокойный фон.
//
// При смене состояния (анимация, цвет, on/off) — плавный crossfade ~500 мс.

#include <stdint.h>

class LedStripExecutor;
class CRGB;

// Тип анимации. Соответствует пунктам toggle-группы в меню (anim_*) +
// дополнительным значениям доступным через invoke (twinkle).
enum class AnimKind : uint8_t {
    Solid   = 0,
    Breathe = 1,
    Wave    = 2,
    Rainbow = 3,
    Twinkle = 4,
};

// Парсер строкового имени из payload (`"solid"`/`"wave"` etc).
// Возвращает true если имя распознано — out содержит соответствующий enum.
// Имя `"off"` обрабатывается отдельно (см. animationsOverride(false,...)).
bool parseAnimationName(const char* name, AnimKind& out);

// ── Wiring ──────────────────────────────────────────────────────────

// Привязать движок к executor (берёт у него leds[], ledsCount, isPulseActive).
// Один раз в setup().
void animationsWire(LedStripExecutor* exec);

// ── Menu-based state ────────────────────────────────────────────────

// Перечитать настройки из меню. Вызывать в setup() ПОСЛЕ menu load и из
// onMenuChanged() при любом изменении меню.
//
// ВАЖНО: автоматически снимает invoke-override (см. animationsOverride).
// Это согласуется с контрактом: «любой commands/set по меню → override снимается».
void animationsApply();

// ── Override (invoke-driven, RAM) ───────────────────────────────────

// Установить runtime-override анимации.
//   enabled = false → лента гасится плавно (override активен, но idle off).
//   enabled = true  → запустить anim+color на всю ленту.
// Вызывается из LedStripExecutor::handlePulse при payload без ledIndex.
void animationsOverride(bool enabled, AnimKind anim, const CRGB& color);

// Явно сбросить override (вернуться к menu-state).
// Эквивалент тому что делает animationsApply при онменю-change.
void animationsClearOverride();

// ── Loop ────────────────────────────────────────────────────────────

// Один тик. Вызывать в каждой итерации loop(). Сам дросселирует частоту кадров.
// Если pulse активен — движок не пишет в leds[] и не вызывает FastLED.show().
void animationsLoop(uint32_t nowMs);
