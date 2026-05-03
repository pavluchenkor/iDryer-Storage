#pragma once

// Menu glue for the LED strip.
//
// Свободные функции, без класса. Связывают сгенерированное из menu_v2.yaml
// меню (lib/idryer-menu) с конкретным железом — LedStripExecutor + FastLED.

#include <stdint.h>
#include <FastLED.h>

class LedStripExecutor;

// ── Chipset / color order / animation indexes ───────────────────────
// Возвращаются на main.cpp::initLedStrip и в анимационный движок.
//
// Chipset:     0=WS2812B 1=WS2811 2=WS2813 3=WS2815 4=SK6812
// Color order: 0=RGB     1=RBG    2=GRB    3=GBR    4=BRG    5=BGR
// Animation:   0=Solid   1=Breathe 2=Wave  3=Rainbow

uint8_t selectedChipset();
uint8_t selectedColorOrder();
uint8_t selectedAnimation();

// CRGB цвет, выбранный в idle / pulse секции меню.
// Возвращает чистый цвет (без brightness scale — FastLED.setBrightness применяется глобально).
CRGB selectedIdleColor();
CRGB selectedPulseColor();

// Текущая яркость 0..255 (потолок намерения; ток БП FastLED режет дополнительно).
uint8_t selectedBrightness();

// idle включён в меню?
bool isIdleEnabled();

// Default duration для led.pulse если args.durationSec не пришёл.
uint16_t pulseDefaultDurationSec();

// ── Bootstrap ───────────────────────────────────────────────────────

// Починка эксклюзивных toggle-групп после menu.loadFromNVS():
//   chipset (5), color_order (6), animation (4), idle_color (8), pulse_color (8).
void normalizeMenuGroups();

// Применить led_count, psu_ma и brightness к executor + FastLED globals.
void applyMenuToExecutor(LedStripExecutor& exec);

// ── Set-handler ─────────────────────────────────────────────────────

// Применить одно значение из commands/set.
//   id — из menu_ids.h.
// Возвращает true если параметр распознан и применён.
//
// onChanged (опционально) — вызывается после успешного apply, чтобы main
// мог обновить состояние анимаций / executor defaults без передёргивания
// всего стека.
using OnMenuChanged = void (*)();
bool applyConfigChange(int id, int val, LedStripExecutor& exec, OnMenuChanged onChanged = nullptr);
