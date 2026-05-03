#pragma once

// Фоновые анимации Storage Link.
//
// Работают только когда:
//   • в меню idle_enabled = true;
//   • нет активного led.pulse от портала (executor.isPulseActive() == false).
//
// При смене любой настройки в меню (animation type / color / brightness)
// движок плавно (~500 мс) переходит в новое состояние через nblend.
//
// Виды анимаций (выбираются эксклюзивным toggle в меню):
//   Solid    — вся лента в выбранном цвете.
//   Breathe  — синусоидное дыхание яркости 20%↔100%, период 4 сек.
//   Wave     — бегущая «комета» выбранного цвета, ~30 LED/сек, fade-tail.
//   Rainbow  — HSV-проход всего hue, период 10 сек. Цвет из меню игнорируется.

#include <stdint.h>

class LedStripExecutor;

// Привязать движок к executor (берёт у него leds[], ledsCount, isPulseActive).
// Один раз в setup().
void animationsWire(LedStripExecutor* exec);

// Перечитать текущие настройки из меню. Вызывать в setup() ПОСЛЕ menu load,
// и из menu OnMenuChanged callback при любом изменении.
void animationsApply();

// Один тик. Вызывать в каждой итерации loop(). Сам дросселирует частоту кадров.
// Если pulse активен — движок не пишет в leds[] и не вызывает FastLED.show().
void animationsLoop(uint32_t nowMs);
