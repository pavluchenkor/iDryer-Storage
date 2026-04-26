// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu metadata only (no pointers to data or callbacks).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MENU_META_COUNT 16
#define MENU_LANG_COUNT 2

typedef enum {
    META_SUBMENU = 0,
    META_ACTION = 1,
    META_VALUE = 2,
    META_TOGGLE = 3
} MenuMetaType;

typedef enum {
    META_VT_F32 = 0,
    META_VT_U16 = 1,
    META_VT_U8 = 2,
    META_VT_I32 = 3,
    META_VT_BOOL = 4,
    META_VT_U32 = 5
} MenuMetaValueType;

typedef enum {
    META_SCOPE_GLOBAL = 0,
    META_SCOPE_PER_UNIT = 1
} MenuMetaScope;

typedef struct {
    uint16_t id;
    const char* title[MENU_LANG_COUNT];
    const char* unit[MENU_LANG_COUNT];
    MenuMetaType type;
    int16_t parent;
    int16_t first_child;
    uint16_t child_count;
    MenuMetaValueType vtype;
    float min_val;
    float max_val;
    float step;
    MenuMetaScope scope;
} MenuMeta;

static const MenuMeta g_menu_meta[MENU_META_COUNT] = {
    // [0] root
    { 0, { "IHEATER LINK", "IHEATER LINK" }, { nullptr, nullptr },
      META_SUBMENU, -1, 1, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [1] portal
    { 1, { "ПОРТАЛ", "PORTAL" }, { nullptr, nullptr },
      META_SUBMENU, 0, 2, 1,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [2] start_claim
    { 2, { "СВЯЗАТЬ", "CLAIM" }, { nullptr, nullptr },
      META_ACTION, 1, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [3] connections
    { 3, { "ПОДКЛЮЧЕНИЯ", "CONNECTIONS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 4, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [4] bambu_enabled
    { 4, { "BAMBU", "BAMBU" }, { nullptr, nullptr },
      META_TOGGLE, 3, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL },
    // [5] moon_enabled
    { 5, { "MOONRAKER", "MOONRAKER" }, { nullptr, nullptr },
      META_TOGGLE, 3, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL },
    // [6] ha_enabled
    { 6, { "HOME ASSISTANT", "HOME ASSISTANT" }, { nullptr, nullptr },
      META_TOGGLE, 3, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL },
    // [7] materials
    { 7, { "МАТЕРИАЛЫ", "MATERIALS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 8, 6,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [8] iheater_link_pla_temp
    { 8, { "PLA", "PLA" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [9] iheater_link_petg_temp
    { 9, { "PETG", "PETG" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [10] iheater_link_abs_temp
    { 10, { "ABS", "ABS" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [11] iheater_link_asa_temp
    { 11, { "ASA", "ASA" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [12] iheater_link_pc_temp
    { 12, { "PC", "PC" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [13] iheater_link_pa_temp
    { 13, { "PA / NYLON", "PA / NYLON" }, { "°C", "°C" },
      META_VALUE, 7, -1, 0,
      META_VT_F32, 30.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [14] units_count
    { 14, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 1.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL },
    // [15] language
    { 15, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 0.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL },
};

static inline const MenuMeta* menu_meta_get(uint16_t id) {
    if (id < MENU_META_COUNT) return &g_menu_meta[id];
    return nullptr;
}
