// Auto-generated. Do not edit.
#include <string.h>
#include <stdio.h>
#include "menu_state.h"
#include "menu_types.h"
#include "menu_nvs.h"
#include "menu_nvs_io.h"

MenuState menu;

void MenuState::initDefaults(){
  this->bambu_en = false;
  this->moon_en = false;
  this->ha_en = false;
  this->mat_pla = 45.0f;
  this->mat_petg = 50.0f;
  this->mat_abs = 60.0f;
  this->mat_asa = 65.0f;
  this->mat_pc = 70.0f;
  this->mat_pa = 70.0f;
  this->units_count = (uint8_t)1;
  this->language = (uint8_t)1;
}

void MenuState::loadFromNVS(){
  uint32_t magic = 0, ver = 0;
  ee_read(NVS_KEY_MAGIC, magic);
  ee_read(NVS_KEY_VERSION, ver);
  if (magic != NVS_MENU_MAGIC || ver != (uint32_t)NVS_MENU_VERSION) return;
  char key[16];
  (void)key;
  ee_read("bambu_en", this->bambu_en);
  ee_read("moon_en", this->moon_en);
  ee_read("ha_en", this->ha_en);
  ee_read("mat_pla", this->mat_pla);
  ee_read("mat_petg", this->mat_petg);
  ee_read("mat_abs", this->mat_abs);
  ee_read("mat_asa", this->mat_asa);
  ee_read("mat_pc", this->mat_pc);
  ee_read("mat_pa", this->mat_pa);
  ee_read("units_count", this->units_count);
  ee_read("language", this->language);
}

void MenuState::saveToNVS(){
  ee_write(NVS_KEY_MAGIC,   (uint32_t)NVS_MENU_MAGIC);
  ee_write(NVS_KEY_VERSION, (uint32_t)NVS_MENU_VERSION);
  char key[16];
  (void)key;
  ee_store_field("bambu_en", this->bambu_en);
  ee_store_field("moon_en", this->moon_en);
  ee_store_field("ha_en", this->ha_en);
  ee_store_field("mat_pla", this->mat_pla);
  ee_store_field("mat_petg", this->mat_petg);
  ee_store_field("mat_abs", this->mat_abs);
  ee_store_field("mat_asa", this->mat_asa);
  ee_store_field("mat_pc", this->mat_pc);
  ee_store_field("mat_pa", this->mat_pa);
  ee_store_field("units_count", this->units_count);
  ee_store_field("language", this->language);
}