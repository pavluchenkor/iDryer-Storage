# Dev Notes: Feature Flags — отключение HA для iHeater

## Задача

Скрыть пункт меню Home Assistant от пользователя в прошивке iHeater Link.
HA-функциональность остаётся в библиотеке `idryer-protocol` как есть — библиотека универсальная.
Управление — через флаги при инициализации, не через `#ifdef`.

---

## Архитектура (текущее состояние)

```
main.cpp
  └─ HeaterDevice(wifi, http, store, apiBaseUrl)   ← нет feature flags
       ├─ MenuBridge(mqtt)                          ← нет feature flags
       └─ LinkIntegrationsManager(mqtt, store)      ← нет feature flags
```

`MenuBridge` принимает только `MqttClient*`.
`LinkIntegrationsManager` принимает только `MqttClient*` и `LinkIntegrationsStore*`.

---

## Рекомендуемое решение

### Шаг 1 — Добавить структуру `LinkFeatures` в `idryer-protocol`

**Файл:** `lib/idryer-protocol/src/cloud/link_features.h` (новый файл)

```cpp
struct LinkFeatures {
    bool homeAssistant = true;
    bool bambu         = true;
    bool moonraker     = true;
};
```

Значения по умолчанию `true` — поведение библиотеки не меняется для проектов, которые не передают флаги.

---

### Шаг 2 — Принять `LinkFeatures` в `LinkIntegrationsManager`

**Файл:** `lib/idryer-protocol/src/cloud/link_integrations_manager.h / .cpp`

Добавить параметр в конструктор:
```cpp
LinkIntegrationsManager(MqttClient* mqtt, LinkIntegrationsStore* store,
                        const LinkFeatures& features = {});
```

В `begin()` и `applyActiveIntegration()` добавить проверку:
```cpp
if (!features_.homeAssistant) {
    haClient_.shutdown();  // не инициализировать, не подключать
}
```

Если `homeAssistant = false` — `HaIntegrationAdapter` никогда не переходит из состояния `Disabled`.

---

### Шаг 3 — Скрыть пункт меню в `MenuBridge`

**Файл:** `src/iheater/MenuBridge.h / .cpp`

Добавить параметр в конструктор:
```cpp
explicit MenuBridge(MqttClient* mqtt, const LinkFeatures& features = {});
```

В `MenuBridge::begin()`, после `menu.loadFromNVS()`, добавить:
```cpp
if (!features_.homeAssistant) {
    menu.ha_en = false;
    // Скрыть пункт из навигации: уменьшить child_count родительского пункта CONNECTIONS
    // CONNECTIONS = g_menu[MENU_CONNECTIONS], у него child_count = 3 (BAMBU, MOON, HA)
    // Убрать HA из отображения: child_count = 2
    // Примечание: HA — последний дочерний элемент (индекс 3 в CONNECTIONS)
    g_menu[MENU_CONNECTIONS].child_count = 2;
}
```

> **Важно:** `g_menu` — статический массив из автогенерата. Изменение `child_count` в runtime
> не ломает массив — лишь ограничивает диапазон итерации в UI. Это корректный способ скрыть пункт.

---

### Шаг 4 — Пробросить `LinkFeatures` через `HeaterDevice`

**Файл:** `src/iheater/HeaterDevice.h / .cpp`

Добавить параметр в конструктор:
```cpp
HeaterDevice(IWifiManager* wifi, IHttpClient* http, ICredentialStore* store,
             const char* apiBaseUrl = nullptr,
             const LinkFeatures& features = {});
```

Передать `features` в `MenuBridge` и `LinkIntegrationsManager` при инициализации.

---

### Шаг 5 — Задать флаги в `main.cpp` (проект iHeater)

**Файл:** `src/main.cpp`

```cpp
#include "cloud/link_features.h"

static const idryer::LinkFeatures kFeatures = {
    .homeAssistant = false,
    .bambu         = true,
    .moonraker     = true,
};

iheaterlink::HeaterDevice device(&wifiManager, &httpClient, &credStore,
                                 IDRYER_API_BASE, kFeatures);
```

Константа в `main.cpp` — проектный выбор, не параметр сборки. Прошивка iDryer Dryer передаёт `{}` (все `true` по умолчанию).

---

## Итоговый эффект

| Проект        | `homeAssistant` | Меню HA | HA MQTT-клиент |
|---------------|-----------------|---------|----------------|
| iHeater Link  | `false`         | скрыт   | не запускается |
| iDryer Dryer  | `true` (default)| виден   | работает       |

---

## Что НЕ нужно делать

- Не трогать автогенерат `menu_data.cpp`, `menu_meta.h`, `menu_bindings.cpp`.
- Не добавлять `#ifdef` в исходники библиотеки.
- Не добавлять `build_flags` в `platformio.ini` — это runtime-решение, не compile-time.
- Не удалять классы `HaMqttClient`, `HaPublisher`, `HaIntegrationAdapter` из библиотеки.
