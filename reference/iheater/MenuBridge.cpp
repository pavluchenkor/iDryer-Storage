#if defined(ESP32) || defined(ESP_PLATFORM)

#include "iheater/MenuBridge.h"

#include <string.h>

#include <hal/hal_types.h>
#include <mqtt/mqtt_client.h>

// Сгенерированные артефакты меню (v3_nvs).
#include <menu_bindings.h>
#include <menu_cache.h>
#include <menu_commands.h>
#include <menu_meta.h>
#include <menu_nvs_io.h>
#include <menu_state.h>

namespace iheaterlink {

namespace {

/// Три toggle в меню ПОДКЛЮЧЕНИЯ, из которых одновременно может быть активен
/// только один. Порядок важен только для лога.
constexpr const char *kConnectionToggles[] = {"bambu_en", "moon_en", "ha_en"};
constexpr size_t kConnectionTogglesCount =
    sizeof(kConnectionToggles) / sizeof(kConnectionToggles[0]);

/// Выключить все toggle ПОДКЛЮЧЕНИЙ кроме `keepBind`.
/// Возвращает количество отключённых.
int disableOtherConnections(const char *keepBind) {
  int off = 0;
  for (size_t i = 0; i < kConnectionTogglesCount; i++) {
    const char *b = kConnectionToggles[i];
    if (keepBind && strcmp(b, keepBind) == 0)
      continue;
    bool cur = false;
    if (menu_read_by_bind(b, &cur) && cur) {
      menu_apply_by_bind(b, 0.0f);
      off++;
      HAL_LOG_INFO("MENU", "exclusivity: %s → OFF", b);
    }
  }
  return off;
}

/// Если после загрузки NVS оказалось включено больше одного — оставить первый,
/// остальные сбросить. Invariant: максимум один активный.
void normalizeExclusivity() {
  const char *firstOn = nullptr;
  for (size_t i = 0; i < kConnectionTogglesCount; i++) {
    bool cur = false;
    if (menu_read_by_bind(kConnectionToggles[i], &cur) && cur) {
      if (!firstOn)
        firstOn = kConnectionToggles[i];
      else {
        menu_apply_by_bind(kConnectionToggles[i], 0.0f);
        HAL_LOG_WARN("MENU", "normalize: duplicate active toggle %s → OFF",
                     kConnectionToggles[i]);
      }
    }
  }
}

} // namespace

void MenuBridge::begin() {
  if (nvsReady_)
    return;

  // 1. Открыть NVS namespace (записывается один раз, держим открытым).
  if (!menu_nvs_begin()) {
    HAL_LOG_WARN("MENU",
                 "NVS begin failed — работаем только с дефолтами в RAM");
  }

  // 2. Выставить дефолты из YAML (перекроет предыдущее состояние MenuState).
  menu.initDefaults();

  // 3. Убедиться, что в NVS лежат служебные __magic / __version. Без них
  //    loadFromNVS() делает ранний return и теряет точечные apply'и (они
  //    пишут только нужный ключ, без magic). Если магик уже есть —
  //    ee_store_field ничего не перезапишет.
  {
    uint32_t magic = 0, ver = 0;
    ee_read(NVS_KEY_MAGIC, magic);
    ee_read(NVS_KEY_VERSION, ver);
    if (magic != NVS_MENU_MAGIC || ver != (uint32_t)NVS_MENU_VERSION) {
      ee_write(NVS_KEY_MAGIC, (uint32_t)NVS_MENU_MAGIC);
      ee_write(NVS_KEY_VERSION, (uint32_t)NVS_MENU_VERSION);
      HAL_LOG_INFO("MENU", "NVS header initialized (magic=0x%08X ver=%u)",
                   (unsigned)NVS_MENU_MAGIC, (unsigned)NVS_MENU_VERSION);
    }
  }

  // 4. Подтянуть сохранённые значения (magic/version проверяются внутри).
  menu.loadFromNVS();

  // 5. Нормализовать эксклюзивность ПОДКЛЮЧЕНИЙ (если в NVS два+ активных).
  normalizeExclusivity();

  // 6. Синхронизировать MenuState → g_menu_cache (для menu_buildFullJson).
  syncStateToCache();

  nvsReady_ = true;

  // 7. Сразу эмитим текущий active чтобы LinkIntegrationsManager применил
  //    выбранный источник (важно если в NVS уже было включено, скажем,
  //    moon_en).
  lastActive_ = ActiveConnection::None; // заставим emit-if-changed сработать
  emitActiveIfChanged();

  HAL_LOG_INFO("MENU", "Initialized (NVS namespace=%s, %u bindings)",
               NVS_MENU_NAMESPACE, (unsigned)g_bindings_count);
}

ActiveConnection MenuBridge::currentActive() const {
  bool en = false;
  if (menu_read_by_bind("bambu_en", &en) && en)
    return ActiveConnection::Bambu;
  if (menu_read_by_bind("moon_en", &en) && en)
    return ActiveConnection::Moonraker;
  if (menu_read_by_bind("ha_en", &en) && en)
    return ActiveConnection::Ha;
  return ActiveConnection::None;
}

void MenuBridge::emitActiveIfChanged() {
  ActiveConnection cur = currentActive();
  if (cur == lastActive_)
    return;
  lastActive_ = cur;
  if (activeCb_)
    activeCb_(cur);
}

void MenuBridge::syncStateToCache() {
  for (uint16_t i = 0; i < g_bindings_count; i++) {
    const MenuBinding &b = g_bindings[i];

    // Для global scope берём скаляр, для per_controller — поэлементно.
    if (b.scope == SCOPE_GLOBAL) {
      float v = 0.0f;
      switch (b.vtype) {
      case VT_F32:
        v = *(const float *)b.ptr;
        break;
      case VT_U16:
        v = (float)*(const uint16_t *)b.ptr;
        break;
      case VT_U8:
        v = (float)*(const uint8_t *)b.ptr;
        break;
      case VT_I32:
        v = (float)*(const int32_t *)b.ptr;
        break;
      case VT_BOOL:
        v = (*(const bool *)b.ptr) ? 1.0f : 0.0f;
        break;
      case VT_U32:
        v = (float)*(const uint32_t *)b.ptr;
        break;
      }
      g_menu_cache.setFloat(b.id, v, 0);
    } else {
      // per_controller: копируем NUM_UNITS значений.
      for (uint8_t u = 0; u < MENU_MAX_UNITS; u++) {
        float v = 0.0f;
        switch (b.vtype) {
        case VT_F32:
          v = ((const float *)b.ptr)[u];
          break;
        case VT_U16:
          v = (float)((const uint16_t *)b.ptr)[u];
          break;
        case VT_U8:
          v = (float)((const uint8_t *)b.ptr)[u];
          break;
        case VT_I32:
          v = (float)((const int32_t *)b.ptr)[u];
          break;
        case VT_BOOL:
          v = ((const bool *)b.ptr)[u] ? 1.0f : 0.0f;
          break;
        case VT_U32:
          v = (float)((const uint32_t *)b.ptr)[u];
          break;
        }
        g_menu_cache.setFloat(b.id, v, u);
      }
    }
  }
}

bool MenuBridge::publishFullConfig() {
  if (!mqtt_)
    return false;
  if (!nvsReady_)
    begin();

  // menu_buildFullJson собирает {v, menu:[...]} из g_menu_meta + g_menu_cache.
  static char buf[MENU_FULL_JSON_BUF_SIZE];
  size_t len = menu_buildFullJson(buf, sizeof(buf));
  if (len == 0) {
    HAL_LOG_ERROR("MENU", "menu_buildFullJson failed");
    return false;
  }

  uint16_t sent = mqtt_->publishConfigRaw(buf, len);
  HAL_LOG_INFO("MENU", "Published config: %u bytes, %u MQTT messages",
               (unsigned)len, (unsigned)sent);
  return sent > 0;
}

bool MenuBridge::applySetCommand(JsonObjectConst data) {
  if (!nvsReady_)
    begin();

  // Два способа адресации: по id (как у iDryer) или по bind (удобнее для
  // отладки).
  const MenuBinding *b = nullptr;

  if (data["bind"].is<const char *>()) {
    b = menu_find_bind(data["bind"].as<const char *>());
  } else if (data["id"].is<int>()) {
    int id = data["id"].as<int>();
    for (uint16_t i = 0; i < g_bindings_count; i++) {
      if (g_bindings[i].id == (uint16_t)id) {
        b = &g_bindings[i];
        break;
      }
    }
  }

  if (!b) {
    HAL_LOG_WARN("MENU", "set: не нашёл биндинг (ожидаем id или bind)");
    return false;
  }

  // Значение берём как float — menu_apply_by_bind сам приведёт к нужному vtype.
  float val = 0.0f;
  if (data["val"].is<float>())
    val = data["val"].as<float>();
  else if (data["val"].is<bool>())
    val = data["val"].as<bool>() ? 1.0f : 0.0f;
  else if (data["val"].is<int>())
    val = (float)data["val"].as<int>();
  else {
    HAL_LOG_WARN("MENU", "set: val не число/bool");
    return false;
  }

  // menu_apply_by_bind: store_value(MenuState) + NVS persist + on_change hook.
  bool ok = menu_apply_by_bind(b->bind, val);
  if (!ok) {
    HAL_LOG_WARN("MENU", "apply failed: bind=%s", b->bind);
    return false;
  }

  // Эксклюзивность ПОДКЛЮЧЕНИЙ: если включили bambu_en/moon_en/ha_en —
  // остальные два выключаем.
  if (val > 0.0f) {
    for (size_t i = 0; i < kConnectionTogglesCount; i++) {
      if (strcmp(b->bind, kConnectionToggles[i]) == 0) {
        disableOtherConnections(b->bind);
        break;
      }
    }
  }

  // Обновляем cache и рассылаем обновлённый config.
  syncStateToCache();

  // После эксклюзивности сообщаем наверх о новом активном ПОДКЛЮЧЕНИИ.
  emitActiveIfChanged();

  publishFullConfig();

  HAL_LOG_INFO("MENU", "set: %s = %.3f (id=%u)", b->bind, (double)val,
               (unsigned)b->id);
  return true;
}

} // namespace iheaterlink

#endif // ESP32
