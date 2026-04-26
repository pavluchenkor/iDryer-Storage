// Auto-generated. Do not edit.
#include <stddef.h>
#include "menu_ids.h"
#include "menu_types.h"
#include "menu_state.h"

#ifdef __cplusplus
extern "C" {
#endif
void start_claim(void);
#ifdef __cplusplus
}
#endif

extern MenuState menu;

const MenuItem g_menu[MENU__COUNT] = {
  [0] = {
    MENU_ROOT, { "IHEATER LINK", "IHEATER LINK" }, { nullptr, nullptr },
    MN_SUBMENU, -1, 1, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [1] = {
    MENU_PORTAL, { "ПОРТАЛ", "PORTAL" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 2, 1,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [2] = {
    MENU_START_CLAIM, { "СВЯЗАТЬ", "CLAIM" }, { nullptr, nullptr },
    MN_ACTION, 1, -1, 0,
    { { start_claim }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [3] = {
    MENU_CONNECTIONS, { "ПОДКЛЮЧЕНИЯ", "CONNECTIONS" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 4, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [4] = {
    MENU_BAMBU_ENABLED, { "BAMBU", "BAMBU" }, { nullptr, nullptr },
    MN_TOGGLE, 3, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.bambu_en, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [5] = {
    MENU_MOON_ENABLED, { "MOONRAKER", "MOONRAKER" }, { nullptr, nullptr },
    MN_TOGGLE, 3, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.moon_en, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [6] = {
    MENU_HA_ENABLED, { "HOME ASSISTANT", "HOME ASSISTANT" }, { nullptr, nullptr },
    MN_TOGGLE, 3, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.ha_en, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [7] = {
    MENU_MATERIALS, { "МАТЕРИАЛЫ", "MATERIALS" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 8, 6,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [8] = {
    MENU_IHEATER_LINK_PLA_TEMP, { "PLA", "PLA" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_pla, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [9] = {
    MENU_IHEATER_LINK_PETG_TEMP, { "PETG", "PETG" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_petg, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [10] = {
    MENU_IHEATER_LINK_ABS_TEMP, { "ABS", "ABS" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_abs, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [11] = {
    MENU_IHEATER_LINK_ASA_TEMP, { "ASA", "ASA" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_asa, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [12] = {
    MENU_IHEATER_LINK_PC_TEMP, { "PC", "PC" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_pc, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [13] = {
    MENU_IHEATER_LINK_PA_TEMP, { "PA / NYLON", "PA / NYLON" }, { "°C", "°C" },
    MN_VALUE, 7, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.mat_pa, 30, 90, 1, nullptr, false } },
    -1, 0
  },
  [14] = {
    MENU_UNITS_COUNT, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
    MN_VALUE, 0, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.units_count, 1, 1, 1, nullptr, false } },
    -1, 0
  },
  [15] = {
    MENU_LANGUAGE, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
    MN_VALUE, 0, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.language, 0, 1, 1, nullptr, false } },
    -1, 0
  },
};
