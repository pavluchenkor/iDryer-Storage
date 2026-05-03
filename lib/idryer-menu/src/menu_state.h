// Auto-generated. Do not edit.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "menu_ids.h"

#ifndef NUM_UNITS
#define NUM_UNITS 1
#endif

class MenuState {
public:
  uint16_t led_count = (uint16_t)120;
  uint16_t psu_ma = (uint16_t)5000;
  uint8_t brightness = (uint8_t)200;
  bool chip_ws2812b = true;
  bool chip_ws2811 = false;
  bool chip_ws2813 = false;
  bool chip_ws2815 = false;
  bool chip_sk6812 = false;
  bool order_rgb = false;
  bool order_rbg = false;
  bool order_grb = true;
  bool order_gbr = false;
  bool order_brg = false;
  bool order_bgr = false;
  bool idle_enabled = false;
  bool anim_solid = false;
  bool anim_breathe = false;
  bool anim_wave = true;
  bool anim_rainbow = false;
  bool idle_red = false;
  bool idle_orange = false;
  bool idle_yellow = false;
  bool idle_green = false;
  bool idle_cyan = false;
  bool idle_blue = false;
  bool idle_magenta = false;
  bool idle_white = true;
  bool pulse_red = false;
  bool pulse_orange = false;
  bool pulse_yellow = false;
  bool pulse_green = false;
  bool pulse_cyan = false;
  bool pulse_blue = false;
  bool pulse_magenta = false;
  bool pulse_white = true;
  uint16_t pulse_dur_sec = (uint16_t)30;
  uint8_t units_count = (uint8_t)1;
  uint8_t language = (uint8_t)1;

  void initDefaults();   // выставить дефолты из YAML
  void loadFromNVS();    // подхватить значения из NVS
  void saveToNVS();      // записать все поля в NVS (создать namespace)
};

extern MenuState menu;