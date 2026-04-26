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
  bool bambu_en = false;
  bool moon_en = false;
  bool ha_en = false;
  float mat_pla = 45.0f;
  float mat_petg = 50.0f;
  float mat_abs = 60.0f;
  float mat_asa = 65.0f;
  float mat_pc = 70.0f;
  float mat_pa = 70.0f;
  uint8_t units_count = (uint8_t)1;
  uint8_t language = (uint8_t)1;

  void initDefaults();   // выставить дефолты из YAML
  void loadFromNVS();    // подхватить значения из NVS
  void saveToNVS();      // записать все поля в NVS (создать namespace)
};

extern MenuState menu;