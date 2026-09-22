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
//   Wave     — бегущая «комета» цвета, ~30 LED/сек, хвост — четверть ленты.
//   Rainbow  — по ленте треть цветового кольца, медленно сдвигается,
//              полный круг за 2 мин (color игнорируется).
//   Twinkle  — мягкие случайные «звёзды» базовым цветом — спокойный фон.
//   Cycle    — вся лента одним цветом медленно проходит цветовое кольцо,
//              период 60 сек (color игнорируется).
//   Aurora   — пятна оттенков вокруг color медленно плывут по ленте.   (отключена)
//   Candle   — тёплый неровно мерцающий свет (color игнорируется).      (отключена)
//   Ocean / Lava / Forest — палитры FastLED медленно перетекают по ленте
//              (color игнорируется).                                   (отключены)
//   Swell    — по ленте бежит синус яркости, 4 волны на ленту.
//   Ripple   — две встречные синусоиды яркости разной длины.
//   Spotlight— светлое окно в четверть ленты ходит туда-обратно по тусклому фону.
//   Duo      — градиент от color к соседнему по кольцу оттенку плывёт по ленте.
//
// При смене состояния (анимация, цвет, on/off) — плавный crossfade ~500 мс.

#include <stdint.h>

class LedStripExecutor;
class CRGB;

// Тип анимации. Значение = индекс пункта в toggle-группе меню (anim_*,
// kAnimBinds в led_strip_menu.cpp): порядок в обоих местах один.
enum class AnimKind : uint8_t {
    Solid   = 0,
    Breathe,
    Wave,
    Rainbow,
    Twinkle,
    Cycle,
    // Отключено (место на флеше): aurora, candle, ocean, lava, forest.
    // Aurora,
    // Candle,
    // Ocean,
    // Lava,
    // Forest,
    Swell,
    Ripple,
    Spotlight,
    Duo,
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

// true когда лента под override и в нём включена анимация (long-running).
// Используется main.cpp чтобы выставить status.mode = LightAnimation/Idle.
bool animationsIsOverrideActive();

// ── Заморозка на время обновления прошивки ──────────────────────────

// Залить ленту цветом ОДИН раз и остановить движок: animationsLoop после
// этого не трогает светодиоды вовсе.
//
// Смысл не в картинке, а в тишине: FastLED.show() на 300 диодов занимает
// заметное время и делает это 30 раз в секунду, из-за чего MQTT обрабатывается
// урывками и загрузка прошивки растягивается втрое (замер на стенде: 90 с
// против 29 с). На время OTA лента должна просто гореть.
void animationsHoldStatic(const CRGB& color);

// Снять заморозку и вернуться к настройкам пользователя.
void animationsResume();

// ── Loop ────────────────────────────────────────────────────────────

// Один тик. Вызывать в каждой итерации loop(). Сам дросселирует частоту кадров.
// Если pulse активен — движок не пишет в leds[] и не вызывает FastLED.show().
void animationsLoop(uint32_t nowMs);
