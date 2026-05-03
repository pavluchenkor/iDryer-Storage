// AUTO-GENERATED. DO NOT EDIT.
#include <string.h>
#include <stdio.h>
#include "menu_bindings.h"
#include "menu_nvs_io.h"
#include "menu_cache.h"   // g_menu_cache — sync inside menu_apply_by_bind

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

extern MenuState menu;
const MenuBinding g_bindings[] = {
  {MENU_LED_COUNT, "led_count", VT_U16, (void*)&menu.led_count, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PSU_MA, "psu_ma", VT_U16, (void*)&menu.psu_ma, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_BRIGHTNESS, "brightness", VT_U8, (void*)&menu.brightness, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_CHIP_WS2812B, "chip_ws2812b", VT_BOOL, (void*)&menu.chip_ws2812b, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_CHIP_WS2811, "chip_ws2811", VT_BOOL, (void*)&menu.chip_ws2811, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_CHIP_WS2813, "chip_ws2813", VT_BOOL, (void*)&menu.chip_ws2813, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_CHIP_WS2815, "chip_ws2815", VT_BOOL, (void*)&menu.chip_ws2815, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_CHIP_SK6812, "chip_sk6812", VT_BOOL, (void*)&menu.chip_sk6812, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_RGB, "order_rgb", VT_BOOL, (void*)&menu.order_rgb, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_RBG, "order_rbg", VT_BOOL, (void*)&menu.order_rbg, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_GRB, "order_grb", VT_BOOL, (void*)&menu.order_grb, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_GBR, "order_gbr", VT_BOOL, (void*)&menu.order_gbr, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_BRG, "order_brg", VT_BOOL, (void*)&menu.order_brg, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ORDER_BGR, "order_bgr", VT_BOOL, (void*)&menu.order_bgr, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_ENABLED, "idle_enabled", VT_BOOL, (void*)&menu.idle_enabled, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ANIM_SOLID, "anim_solid", VT_BOOL, (void*)&menu.anim_solid, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ANIM_BREATHE, "anim_breathe", VT_BOOL, (void*)&menu.anim_breathe, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ANIM_WAVE, "anim_wave", VT_BOOL, (void*)&menu.anim_wave, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_ANIM_RAINBOW, "anim_rainbow", VT_BOOL, (void*)&menu.anim_rainbow, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_RED, "idle_red", VT_BOOL, (void*)&menu.idle_red, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_ORANGE, "idle_orange", VT_BOOL, (void*)&menu.idle_orange, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_YELLOW, "idle_yellow", VT_BOOL, (void*)&menu.idle_yellow, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_GREEN, "idle_green", VT_BOOL, (void*)&menu.idle_green, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_CYAN, "idle_cyan", VT_BOOL, (void*)&menu.idle_cyan, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_BLUE, "idle_blue", VT_BOOL, (void*)&menu.idle_blue, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_MAGENTA, "idle_magenta", VT_BOOL, (void*)&menu.idle_magenta, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IDLE_WHITE, "idle_white", VT_BOOL, (void*)&menu.idle_white, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_RED, "pulse_red", VT_BOOL, (void*)&menu.pulse_red, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_ORANGE, "pulse_orange", VT_BOOL, (void*)&menu.pulse_orange, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_YELLOW, "pulse_yellow", VT_BOOL, (void*)&menu.pulse_yellow, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_GREEN, "pulse_green", VT_BOOL, (void*)&menu.pulse_green, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_CYAN, "pulse_cyan", VT_BOOL, (void*)&menu.pulse_cyan, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_BLUE, "pulse_blue", VT_BOOL, (void*)&menu.pulse_blue, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_MAGENTA, "pulse_magenta", VT_BOOL, (void*)&menu.pulse_magenta, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_WHITE, "pulse_white", VT_BOOL, (void*)&menu.pulse_white, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_PULSE_DURATION, "pulse_dur_sec", VT_U16, (void*)&menu.pulse_dur_sec, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_UNITS_COUNT, "units_count", VT_U8, (void*)&menu.units_count, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_LANGUAGE, "language", VT_U8, (void*)&menu.language, true, nullptr, false, SCOPE_GLOBAL},
};
const uint16_t g_bindings_count = sizeof(g_bindings)/sizeof(g_bindings[0]);

ConfigChangeHookFn g_config_change_hook = nullptr;

void menu_set_config_change_hook(ConfigChangeHookFn hook) {
  g_config_change_hook = hook;
}

const MenuBinding* menu_find_bind(const char* bind) {
  if (!bind) return nullptr;
  for (uint16_t i=0;i<g_bindings_count;i++)
    if (strcmp(g_bindings[i].bind, bind) == 0) return &g_bindings[i];
  return nullptr;
}

static inline void store_value(const MenuBinding& b, float v, uint8_t idx) {
  switch (b.vtype) {
    case VT_F32:  if (b.scope==SCOPE_GLOBAL) *(float*)b.ptr    = v; else ((float*)b.ptr)[idx]    = v; break;
    case VT_U16:  if (b.scope==SCOPE_GLOBAL) *(uint16_t*)b.ptr = (uint16_t)v; else ((uint16_t*)b.ptr)[idx] = (uint16_t)v; break;
    case VT_U8:   if (b.scope==SCOPE_GLOBAL) *(uint8_t*)b.ptr  = (uint8_t)v;  else ((uint8_t*)b.ptr)[idx]  = (uint8_t)v;  break;
    case VT_I32:  if (b.scope==SCOPE_GLOBAL) *(int32_t*)b.ptr  = (int32_t)v;  else ((int32_t*)b.ptr)[idx]  = (int32_t)v;  break;
    case VT_BOOL: if (b.scope==SCOPE_GLOBAL) *(bool*)b.ptr     = (bool)(v!=0.0f); else ((bool*)b.ptr)[idx]     = (bool)(v!=0.0f); break;
    case VT_U32:  if (b.scope==SCOPE_GLOBAL) *(uint32_t*)b.ptr = (uint32_t)v; else ((uint32_t*)b.ptr)[idx] = (uint32_t)v; break;
  }
}

static inline void read_value(const MenuBinding& b, void* out_value, uint8_t idx) {
  switch (b.vtype) {
    case VT_F32:  *(float*)out_value    = (b.scope==SCOPE_GLOBAL)? *(float*)b.ptr    : ((float*)b.ptr)[idx]; break;
    case VT_U16:  *(uint16_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint16_t*)b.ptr : ((uint16_t*)b.ptr)[idx]; break;
    case VT_U8:   *(uint8_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(uint8_t*)b.ptr  : ((uint8_t*)b.ptr)[idx]; break;
    case VT_I32:  *(int32_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(int32_t*)b.ptr  : ((int32_t*)b.ptr)[idx]; break;
    case VT_BOOL: *(bool*)out_value     = (b.scope==SCOPE_GLOBAL)? *(bool*)b.ptr     : ((bool*)b.ptr)[idx]; break;
    case VT_U32:  *(uint32_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint32_t*)b.ptr : ((uint32_t*)b.ptr)[idx]; break;
  }
}

static inline void build_nvs_key(const MenuBinding& b, uint8_t idx, char* out, size_t cap){
  if (b.scope == SCOPE_GLOBAL) {
    strncpy(out, b.bind, cap - 1);
    out[cap - 1] = '\0';
  } else {
    snprintf(out, cap, "%s_%u", b.bind, (unsigned)idx);
  }
}

bool menu_read_by_bind(const char* bind, void* out_value) {
  const MenuBinding* b = menu_find_bind(bind);
  if (!b || !out_value) return false;
  uint8_t idx = 0;
  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();
  read_value(*b, out_value, idx);
  return true;
}

bool menu_apply_by_bind(const char* bind, float v) {
  const MenuBinding* b = menu_find_bind(bind);
  if (!b) return false;
  uint8_t idx = 0;
  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();
  store_value(*b, v, idx);

  // Sync into g_menu_cache so menu_buildFullJson() returns the fresh value.
  // Cache lives separately from MenuState — without this sync the
  // re-published config after commands/set would show stale data.
  {
    float cached = 0.0f;
    switch (b->vtype) {
      case VT_F32:  cached = (b->scope==SCOPE_GLOBAL) ? *(const float*)b->ptr    : ((const float*)b->ptr)[idx]; break;
      case VT_U16:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint16_t*)b->ptr : ((const uint16_t*)b->ptr)[idx]); break;
      case VT_U8:   cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint8_t*)b->ptr  : ((const uint8_t*)b->ptr)[idx]); break;
      case VT_I32:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const int32_t*)b->ptr  : ((const int32_t*)b->ptr)[idx]); break;
      case VT_BOOL: cached = ((b->scope==SCOPE_GLOBAL) ? *(const bool*)b->ptr    : ((const bool*)b->ptr)[idx]) ? 1.0f : 0.0f; break;
      case VT_U32:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint32_t*)b->ptr : ((const uint32_t*)b->ptr)[idx]); break;
    }
    g_menu_cache.setFloat(b->id, cached, (b->scope==SCOPE_GLOBAL) ? 0 : idx);
  }

  if (b->persist) {
    char key[16];
    build_nvs_key(*b, idx, key, sizeof(key));
    menu_nvs_begin();
    switch (b->vtype) {
      case VT_F32:  ee_store_field<float>(   key, (b->scope==SCOPE_GLOBAL)? *(float*)b->ptr    : ((float*)b->ptr)[idx]); break;
      case VT_U16:  ee_store_field<uint16_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint16_t*)b->ptr : ((uint16_t*)b->ptr)[idx]); break;
      case VT_U8:   ee_store_field<uint8_t>( key, (b->scope==SCOPE_GLOBAL)? *(uint8_t*)b->ptr  : ((uint8_t*)b->ptr)[idx]); break;
      case VT_I32:  ee_store_field<int32_t>( key, (b->scope==SCOPE_GLOBAL)? *(int32_t*)b->ptr  : ((int32_t*)b->ptr)[idx]); break;
      case VT_BOOL: ee_store_field<bool>(    key, (b->scope==SCOPE_GLOBAL)? *(bool*)b->ptr     : ((bool*)b->ptr)[idx]); break;
      case VT_U32:  ee_store_field<uint32_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint32_t*)b->ptr : ((uint32_t*)b->ptr)[idx]); break;
    }
    menu_nvs_end();
  }
  if (b->on_change) b->on_change((void*)b->ptr);
  if (g_config_change_hook) g_config_change_hook(b->id, idx, b->bind);
  return true;
}
