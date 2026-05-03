# iDryer Contracts

Единый источник правды для всех коммуникационных каналов платформы iDryer:
MQTT (portal/HA/Bambu), UART (RP2040↔ESP), WebSocket (Moonraker, local-WS),
HTTP REST (portal claim flow), HA discovery, WiFi provisioning.

## Структура

```
mqtt_contract.yaml          ← source of truth (yaml)
mqtt_contract.schema.json   ← meta-schema (JSON Schema, валидирует yaml)

validate_contract.py        ← валидация yaml + cross-refs + sizeof расчёт
gen_uart_protocol_h.py      ← yaml → C++ UART header
gen_mqtt_topics_h.py        ← yaml → C++ MQTT topics header
gen_ts_types.py             ← yaml → TypeScript types

regen.sh                    ← единая точка: validate + регенерация всего
pre_commit.sh               ← git hook: вызывает regen.sh + sync-check
HOOKS.md                    ← как установить hook

_generated/                 ← выходы генераторов (DO NOT EDIT)
  uart_protocol.h           ← C++ structs / enums / kind ids
  mqtt_topics.h             ← C++ topic constants / QoS / retained
  mqtt-api.types.ts         ← TS types для портала
```

## Pipeline

```bash
./regen.sh
```

Внутри: `validate_contract.py` → 3 генератора подряд. Список генераторов
держится в массиве `GENERATORS` в `regen.sh` — добавлять новый туда же.

`pre_commit.sh` зовёт тот же `regen.sh` и проверяет что `_generated/*`
не разъехались с репо — см. `HOOKS.md`.

## Правило изменения

Любая правка коммуникационного канала обновляет **в одном changeset**:

1. `mqtt_contract.yaml`
2. регенерация `_generated/*`
3. код firmware / портала

Порядок строгий: **сначала контракт, потом код**. Pre-commit hook не даст
закоммитить yaml без актуальных `_generated/*`.

## Что покрыто в yaml

| Канал | Секция |
|---|---|
| MQTT (portal) | `messages`, `mqtt_only`, `legacy_command_topics` |
| UART (RP2040↔ESP) | `messages.bindings.uart`, `uart_only`, `uart_kind_ranges` |
| HA integration runtime | `ha_integration_topics` |
| HA Discovery | `ha_discovery_topics` |
| Bambu LAN MQTT | `bambu_lan_mqtt` |
| Moonraker WebSocket | `moonraker_websocket` |
| Local WebSocket server | `local_websocket` |
| Cloud HTTP API (claim) | `cloud_http_api` |
| WiFi provisioning | `wifi_provisioning` |

## Конвенции

- `rules.timestamp_convention` — поле `timestamp` (ISO 8601 UTC) в обе стороны.
- `rules.mqtt_session`, `rules.publish_qos`, `rules.uart_*` — общие транспортные правила.
- Расхождения и не-implemented entries фиксируются в `known_mismatches` с
  `decision_required` / `decision_made` / `decision_date`.

## Источники реальности (на момент составления)

- `idryer-core` (новая SDK): `lib/idryer-core/src/`
- `idryer-protocol` (legacy): `idryer-link/lib/idryer-protocol/src/`
- portal backend (NestJS): `iDryerPortal/backend/src/`
