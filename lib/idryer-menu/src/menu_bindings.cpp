// AUTO-GENERATED. DO NOT EDIT.
#include <string.h>
#include <stdio.h>
#include "menu_bindings.h"
#include "menu_nvs_io.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

extern MenuState menu;
const MenuBinding g_bindings[] = {
  {MENU_BAMBU_ENABLED, "bambu_en", VT_BOOL, (void*)&menu.bambu_en, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_MOON_ENABLED, "moon_en", VT_BOOL, (void*)&menu.moon_en, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_HA_ENABLED, "ha_en", VT_BOOL, (void*)&menu.ha_en, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_PLA_TEMP, "mat_pla", VT_F32, (void*)&menu.mat_pla, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_PETG_TEMP, "mat_petg", VT_F32, (void*)&menu.mat_petg, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_ABS_TEMP, "mat_abs", VT_F32, (void*)&menu.mat_abs, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_ASA_TEMP, "mat_asa", VT_F32, (void*)&menu.mat_asa, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_PC_TEMP, "mat_pc", VT_F32, (void*)&menu.mat_pc, true, nullptr, false, SCOPE_GLOBAL},
  {MENU_IHEATER_LINK_PA_TEMP, "mat_pa", VT_F32, (void*)&menu.mat_pa, true, nullptr, false, SCOPE_GLOBAL},
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
  if (b->persist) {
    char key[16];
    build_nvs_key(*b, idx, key, sizeof(key));
    switch (b->vtype) {
      case VT_F32:  ee_store_field<float>(   key, (b->scope==SCOPE_GLOBAL)? *(float*)b->ptr    : ((float*)b->ptr)[idx]); break;
      case VT_U16:  ee_store_field<uint16_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint16_t*)b->ptr : ((uint16_t*)b->ptr)[idx]); break;
      case VT_U8:   ee_store_field<uint8_t>( key, (b->scope==SCOPE_GLOBAL)? *(uint8_t*)b->ptr  : ((uint8_t*)b->ptr)[idx]); break;
      case VT_I32:  ee_store_field<int32_t>( key, (b->scope==SCOPE_GLOBAL)? *(int32_t*)b->ptr  : ((int32_t*)b->ptr)[idx]); break;
      case VT_BOOL: ee_store_field<bool>(    key, (b->scope==SCOPE_GLOBAL)? *(bool*)b->ptr     : ((bool*)b->ptr)[idx]); break;
      case VT_U32:  ee_store_field<uint32_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint32_t*)b->ptr : ((uint32_t*)b->ptr)[idx]); break;
    }
  }
  if (b->on_change) b->on_change((void*)b->ptr);
  if (g_config_change_hook) g_config_change_hook(b->id, idx, b->bind);
  return true;
}
