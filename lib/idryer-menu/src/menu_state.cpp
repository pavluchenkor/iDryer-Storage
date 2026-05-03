// Auto-generated. Do not edit.
#include <string.h>
#include <stdio.h>
#include "menu_state.h"
#include "menu_types.h"
#include "menu_nvs.h"
#include "menu_nvs_io.h"

MenuState menu;

void MenuState::initDefaults(){
  this->led_count = (uint16_t)120;
  this->psu_ma = (uint16_t)5000;
  this->brightness = (uint8_t)200;
  this->chip_ws2812b = true;
  this->chip_ws2811 = false;
  this->chip_ws2813 = false;
  this->chip_ws2815 = false;
  this->chip_sk6812 = false;
  this->order_rgb = false;
  this->order_rbg = false;
  this->order_grb = true;
  this->order_gbr = false;
  this->order_brg = false;
  this->order_bgr = false;
  this->idle_enabled = false;
  this->anim_solid = false;
  this->anim_breathe = false;
  this->anim_wave = true;
  this->anim_rainbow = false;
  this->idle_red = false;
  this->idle_orange = false;
  this->idle_yellow = false;
  this->idle_green = false;
  this->idle_cyan = false;
  this->idle_blue = false;
  this->idle_magenta = false;
  this->idle_white = true;
  this->pulse_red = false;
  this->pulse_orange = false;
  this->pulse_yellow = false;
  this->pulse_green = false;
  this->pulse_cyan = false;
  this->pulse_blue = false;
  this->pulse_magenta = false;
  this->pulse_white = true;
  this->pulse_dur_sec = (uint16_t)30;
  this->units_count = (uint8_t)1;
  this->language = (uint8_t)1;
}

void MenuState::loadFromNVS(){
  menu_nvs_begin();
  uint32_t magic = 0, ver = 0;
  ee_read(NVS_KEY_MAGIC, magic);
  ee_read(NVS_KEY_VERSION, ver);
  if (magic != NVS_MENU_MAGIC || ver != (uint32_t)NVS_MENU_VERSION) {
    menu_nvs_end();
    saveToNVS();  // first boot: persist defaults + magic
    return;
  }
  char key[16];
  (void)key;
  ee_read("led_count", this->led_count);
  ee_read("psu_ma", this->psu_ma);
  ee_read("brightness", this->brightness);
  ee_read("chip_ws2812b", this->chip_ws2812b);
  ee_read("chip_ws2811", this->chip_ws2811);
  ee_read("chip_ws2813", this->chip_ws2813);
  ee_read("chip_ws2815", this->chip_ws2815);
  ee_read("chip_sk6812", this->chip_sk6812);
  ee_read("order_rgb", this->order_rgb);
  ee_read("order_rbg", this->order_rbg);
  ee_read("order_grb", this->order_grb);
  ee_read("order_gbr", this->order_gbr);
  ee_read("order_brg", this->order_brg);
  ee_read("order_bgr", this->order_bgr);
  ee_read("idle_enabled", this->idle_enabled);
  ee_read("anim_solid", this->anim_solid);
  ee_read("anim_breathe", this->anim_breathe);
  ee_read("anim_wave", this->anim_wave);
  ee_read("anim_rainbow", this->anim_rainbow);
  ee_read("idle_red", this->idle_red);
  ee_read("idle_orange", this->idle_orange);
  ee_read("idle_yellow", this->idle_yellow);
  ee_read("idle_green", this->idle_green);
  ee_read("idle_cyan", this->idle_cyan);
  ee_read("idle_blue", this->idle_blue);
  ee_read("idle_magenta", this->idle_magenta);
  ee_read("idle_white", this->idle_white);
  ee_read("pulse_red", this->pulse_red);
  ee_read("pulse_orange", this->pulse_orange);
  ee_read("pulse_yellow", this->pulse_yellow);
  ee_read("pulse_green", this->pulse_green);
  ee_read("pulse_cyan", this->pulse_cyan);
  ee_read("pulse_blue", this->pulse_blue);
  ee_read("pulse_magenta", this->pulse_magenta);
  ee_read("pulse_white", this->pulse_white);
  ee_read("pulse_dur_sec", this->pulse_dur_sec);
  ee_read("units_count", this->units_count);
  ee_read("language", this->language);
  menu_nvs_end();
}

void MenuState::saveToNVS(){
  menu_nvs_begin();
  ee_write(NVS_KEY_MAGIC,   (uint32_t)NVS_MENU_MAGIC);
  ee_write(NVS_KEY_VERSION, (uint32_t)NVS_MENU_VERSION);
  char key[16];
  (void)key;
  ee_store_field("led_count", this->led_count);
  ee_store_field("psu_ma", this->psu_ma);
  ee_store_field("brightness", this->brightness);
  ee_store_field("chip_ws2812b", this->chip_ws2812b);
  ee_store_field("chip_ws2811", this->chip_ws2811);
  ee_store_field("chip_ws2813", this->chip_ws2813);
  ee_store_field("chip_ws2815", this->chip_ws2815);
  ee_store_field("chip_sk6812", this->chip_sk6812);
  ee_store_field("order_rgb", this->order_rgb);
  ee_store_field("order_rbg", this->order_rbg);
  ee_store_field("order_grb", this->order_grb);
  ee_store_field("order_gbr", this->order_gbr);
  ee_store_field("order_brg", this->order_brg);
  ee_store_field("order_bgr", this->order_bgr);
  ee_store_field("idle_enabled", this->idle_enabled);
  ee_store_field("anim_solid", this->anim_solid);
  ee_store_field("anim_breathe", this->anim_breathe);
  ee_store_field("anim_wave", this->anim_wave);
  ee_store_field("anim_rainbow", this->anim_rainbow);
  ee_store_field("idle_red", this->idle_red);
  ee_store_field("idle_orange", this->idle_orange);
  ee_store_field("idle_yellow", this->idle_yellow);
  ee_store_field("idle_green", this->idle_green);
  ee_store_field("idle_cyan", this->idle_cyan);
  ee_store_field("idle_blue", this->idle_blue);
  ee_store_field("idle_magenta", this->idle_magenta);
  ee_store_field("idle_white", this->idle_white);
  ee_store_field("pulse_red", this->pulse_red);
  ee_store_field("pulse_orange", this->pulse_orange);
  ee_store_field("pulse_yellow", this->pulse_yellow);
  ee_store_field("pulse_green", this->pulse_green);
  ee_store_field("pulse_cyan", this->pulse_cyan);
  ee_store_field("pulse_blue", this->pulse_blue);
  ee_store_field("pulse_magenta", this->pulse_magenta);
  ee_store_field("pulse_white", this->pulse_white);
  ee_store_field("pulse_dur_sec", this->pulse_dur_sec);
  ee_store_field("units_count", this->units_count);
  ee_store_field("language", this->language);
  menu_nvs_end();
}