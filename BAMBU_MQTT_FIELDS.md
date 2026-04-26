# Bambu Lab LAN MQTT — iHeater Reader-режим

Справочный документ по подключению к локальному MQTT-брокеру Bambu Lab,
форматам сообщений и логике автонагрева в iHeater-link.

Первоисточники:

- **OpenBambuAPI** — реверс-инженерная документация протокола (сообщество):
  [`github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)
- **ha-bambulab models.py** — реальный парсинг Python в интеграции Home Assistant:
  [`github.com/greghesp/ha-bambulab/.../pybambu/models.py`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py)
- **ha-bambulab const.py** — константы протокола:
  [`github.com/greghesp/ha-bambulab/.../pybambu/const.py`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/const.py)

---

## 1. Подключение

> Источник: [`OpenBambuAPI mqtt.md — Local MQTT server`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)  
> В iHeater-link: `bambu_client.cpp:142`, `bambu_client.cpp:207–212`

| Параметр  | Значение                                       |
|-----------|------------------------------------------------|
| Хост      | IP-адрес принтера                              |
| Порт      | `8883`                                         |
| TLS       | да, сертификат самоподписанный (`setInsecure`) |
| Username  | `bblp`                                         |
| Password  | LAN Access Code (8 цифр)                       |
| Client ID | произвольный, уникальный                       |

---

## 2. Топики

> Источник: [`OpenBambuAPI mqtt.md — Topics`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)  
> В iHeater-link: `bambu_client.cpp:130–131`

| Топик                      | Направление      | Назначение   |
|---------------------------|------------------|--------------|
| `device/{SERIAL}/report`  | принтер → клиент | статус, ответы |
| `device/{SERIAL}/request` | клиент → принтер | команды      |

`{SERIAL}` — серийный номер принтера (например, `01P00A123456789`).

---

## 3. Сценарий: подключение

После установки MQTT-сессии клиент подписывается на `device/{SERIAL}/report`
и запрашивает полный снимок состояния.

P1-серия в штатном режиме присылает только изменившиеся поля. Без `pushall`
первый принятый `push_status` будет неполным.

**Отправить в `device/{SERIAL}/request`:**

> Источник: [`OpenBambuAPI mqtt.md — pushing.pushall`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md) — реальный формат запроса  
> В iHeater-link: `bambu_client.cpp:321–331` (`requestPushAll()`)

```json
{
  "pushing": {
    "sequence_id": "1",
    "command": "pushall",
    "version": 1,
    "push_target": 1
  }
}
```

> Предупреждение (источник: сообщество, [`OpenBambuAPI mqtt.md`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md),
> официального подтверждения от производителя нет): на P1P не отправлять
> чаще одного раза в 5 минут — по наблюдениям вызывает лаги.

---

## 4. Сценарий: принтер стоит (IDLE)

Принтер периодически публикует `push_status` в `device/{SERIAL}/report`.

**Входящее сообщение (упрощено из реального примера):**

> Источник: [`OpenBambuAPI mqtt.md — push_status report example`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)
> (реальный пример из документации, состояние `IDLE`, поля сокращены для читаемости)

```json
{
  "print": {
    "command": "push_status",
    "gcode_state": "IDLE",
    "nozzle_temper": 25.0,
    "nozzle_target_temper": 0.0,
    "bed_temper": 25.0,
    "bed_target_temper": 0.0,
    "chamber_temper": 24.0,
    "chamber_target": 0.0,
    "ams": {
      "tray_now": "255",
      "ams": [
        {
          "id": "0",
          "tray": [
            { "id": "0" },
            { "id": "1", "tray_type": "ABS", "tray_info_idx": "GFA00" },
            { "id": "2", "tray_type": "PLA", "tray_info_idx": "GFA05" },
            { "id": "3" }
          ]
        }
      ]
    }
  }
}
```

**Ключевые поля:**

| Поле | Значение | Смысл | Источник |
|---|---|---|---|
| `gcode_state` | `"IDLE"` | печать не идёт | [`OpenBambuAPI`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md), [`const.py#L172`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/const.py#L172) |
| `ams.tray_now` | `"255"` | нет активного трея | [`OpenBambuAPI mqtt.md#L251`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md) |

**Реакция iHeater:** `gcode_state != RUNNING/PREPARE` → нагрев выключен.

---

## 5. Сценарий: принтер печатает (RUNNING)

> **Внимание:** реального примера `push_status` в состоянии `RUNNING` в
> OpenBambuAPI нет. Формат сообщения ниже составлен на основе описания полей
> из [`OpenBambuAPI mqtt.md`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)
> и кода [`ha-bambulab models.py`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py).

**Входящее сообщение:**

```json
{
  "print": {
    "command": "push_status",
    "gcode_state": "RUNNING",
    "mc_percent": 42,
    "mc_remaining_time": 87,
    "nozzle_temper": 240.0,
    "nozzle_target_temper": 240.0,
    "bed_temper": 90.0,
    "bed_target_temper": 90.0,
    "chamber_temper": 35.0,
    "chamber_target": 0.0,
    "ams": {
      "tray_now": "1",
      "ams": [
        {
          "id": "0",
          "tray": [
            { "id": "0" },
            { "id": "1", "tray_type": "ABS", "tray_info_idx": "GFA00" },
            { "id": "2", "tray_type": "PLA", "tray_info_idx": "GFA05" },
            { "id": "3" }
          ]
        }
      ]
    }
  }
}
```

**Разбор `tray_now = "1"`:**

> Источник формулы: [`OpenBambuAPI mqtt.md#L251`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md),
> [`ha-bambulab models.py#L2787`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py#L2787)

```
ams_index  = 1 >> 2 = 0   →  ams[0]
tray_index = 1 & 0x3 = 1  →  tray[1]
tray_type  = "ABS"
```

**Ключевые поля:**

| Поле | Значение | Смысл | Источник |
|---|---|---|---|
| `gcode_state` | `"RUNNING"` | идёт печать | [`OpenBambuAPI`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md), [`const.py#L172`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/const.py#L172) |
| `ams.tray_now` | `"1"` | активен AMS 0, трей 1 | [`OpenBambuAPI mqtt.md#L251`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md), [`models.py#L2770`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py#L2770) |
| `ams[0].tray[1].tray_type` | `"ABS"` | тип активного филамента | [`OpenBambuAPI mqtt.md#L192`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md), [`models.py#L2944`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py#L2944) |
| `chamber_target` | `0.0` | X1C без камеры или P1S | [`OpenBambuAPI`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md) |

**Реакция iHeater:**
```
gcode_state == RUNNING → разрешаем нагрев
chamber_target == 0    → нет данных от принтера
tray_type == "ABS"     → materialTempFromMenu("ABS") → menu.mat_abs → target
```

---

## 6. Сценарий: X1C с датчиком камеры (RUNNING)

> **Внимание:** формат составлен на основе описания полей `chamber_temper` /
> `chamber_target` из [`OpenBambuAPI mqtt.md`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md).
> Реального примера с ненулевым `chamber_target` в документации нет.

На X1C принтер сам задаёт целевую температуру камеры:

```json
{
  "print": {
    "gcode_state": "RUNNING",
    "chamber_temper": 38.0,
    "chamber_target": 45.0,
    "ams": {
      "tray_now": "1"
    }
  }
}
```

**Реакция iHeater:** `chamber_target = 45.0 > 0` → target берётся от принтера
напрямую, `tray_type` не используется.

---

## 7. Сценарий: печать завершена (FINISH)

> **Внимание:** формат составлен на основе описания `gcode_state` из
> [`OpenBambuAPI mqtt.md`](https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md)
> и [`ha-bambulab const.py#L172`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/const.py#L172).

```json
{
  "print": {
    "gcode_state": "FINISH",
    "ams": {
      "tray_now": "255"
    }
  }
}
```

**Реакция iHeater:** `gcode_state == FINISH` → нагрев выключается.

---

## 8. Цепочка приоритетов (планируемая логика)

```
Получен push_status
        │
        ▼
bambu_en == false?  ──yes──▶  OFF
        │ no
        ▼
gcode_state == RUNNING или PREPARE?  ──no──▶  OFF
        │ yes
        ▼
chamber_target > 0?  ──yes──▶  target = chamber_target  (X1C, от принтера)
        │ no
        ▼
tray_now → tray_type != ""?  ──yes──▶  target = materialTempFromMenu(tray_type)
        │ no
        ▼
       OFF
```

---

## 9. Что сейчас реализовано и что нет

### Реализовано

| Компонент | Файл | Статус |
|---|---|---|
| Подключение LAN MQTT | `bambu_client.cpp:140–212` | готово |
| Подписка на `report` | `bambu_client.cpp:220–227` | готово |
| `pushall` при старте | `bambu_client.cpp:321` | готово |
| Парсинг `gcode_state` | `bambu_client.cpp:372` | готово |
| Парсинг `chamber_target` | `bambu_client.cpp:426` | готово |
| Парсинг `tray_type` | `bambu_client.cpp:442` | частично — берёт первый непустой трей, `tray_now` игнорируется |
| Нагрев по `chamber_target` | `HeaterDevice.cpp:402` | готово |
| Нагрев по `tray_type` → menu | `HeaterDevice.cpp:408` | готово, но работает на неверном трее |

### Не реализовано

**1. Корректный выбор активного трея через `tray_now`**

Файл: `lib/idryer-protocol/src/cloud/bambu_client.cpp:431–454`

Текущее поведение: берётся первый непустой трей из `ams[0]`.

Требуемое поведение — **полная формула** из реального кода
([`models.py#L2775–2788`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py#L2775)):

```
tray_now = int(print.ams.tray_now)

255  → нет активного трея  (ams_index=255, tray_index=255)

254  → внешняя катушка     (ams_index=255, tray_index=0)
         → tray_type из print.ams.vt_tray.tray_type

>= 80 → AMS HT (индексы 0x80–0x87 = 128–135)
         → ams_index = tray_now  (сам индекс AMS HT)
           tray_index = 0
           tray_type из ams[ams_index].tray[0].tray_type

else → стандартный AMS / AMS Lite
         → ams_index  = tray_now >> 2
           tray_index = tray_now & 0x3
           tray_type из ams[ams_index].tray[tray_index].tray_type
```

> **Внимание:** документ BAMBU_MQTT_FIELDS.md изначально содержал неполную
> формулу (только случаи 255/254/else) без ветки AMS HT (`>= 80`) и с
> неверным описанием 254 (на самом деле 254 → `ams_index=255`, т.е. внешняя
> катушка через `vt_tray`, а не AMS с id=254).
> Источник-подтверждение: [`models.py#L2775–2788`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/models.py#L2775),
> проверено напрямую по GitHub 2026-04-26.

**2. Проверка `gcode_state` в логике нагрева**

Файл: `src/iheater/HeaterDevice.cpp:396`

Текущее поведение: нагрев включается при наличии любого `tray_type` в AMS,
независимо от состояния принтера.

Требуемое поведение: нагрев разрешён только при `gcode_state == "RUNNING"`
или `"PREPARE"`. Значения приходят в JSON **заглавными буквами** (`"RUNNING"`,
`"PREPARE"`, `"IDLE"`, `"FINISH"` и т. д.); const.py хранит их в нижнем
регистре после преобразования, но сравнение в C++ должно быть с uppercase.

> Источник списка состояний: [`const.py GCODE_STATE_OPTIONS`](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/const.py)
> (failed / finish / idle / init / offline / pause / prepare / running / slicing / unknown),
> проверено напрямую по GitHub 2026-04-26.

---

## 10. Конфигурация от портала

> В iHeater-link: `link_integrations_manager.cpp:590–608`

Портал отправляет команду через `idryer/<SERIAL>/commands/link_integration`:

```json
{
  "type": "bambu",
  "enabled": true,
  "ip": "192.168.1.100",
  "serial": "01P00A123456789",
  "lanAccessCode": "12345678"
}
```

Поля `defaultAmsId`, `defaultTrayId`, `autoApplyOnTagDetect` относятся к
Writer-режиму (iDryer) и для iHeater не используются.
