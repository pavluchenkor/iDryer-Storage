# Сквозные сценарии

Четыре типовых сценария, прослеживающих путь данных через все три слоя (MCU ↔ UART ↔ LINK ↔ MQTT ↔ Cloud). Здесь не просто справочник — а **последовательность событий во времени**.

| Документ | Сценарий |
|----------|----------|
| [01-first-boot.md](01-first-boot.md) | Первый запуск: от питания до `Online`, с таймлайном в секундах |
| [02-claiming-flow.md](02-claiming-flow.md) | Полный цикл привязки: `ClaimStart` → HTTP → PIN → claim → `ClaimComplete` → MQTT |
| [03-remote-config.md](03-remote-config.md) | Конфиг меню: полный JSON, delta, `set`, `invoke` (UART ConfigPush + MQTT) |
| [04-profile-mode.md](04-profile-mode.md) | Профильная сушка: MQTT → ProfilePayload → PID-автомат RAMP/HOLD на MCU |

## Когда читать

- Первый раз изучаете как работает устройство целиком → [01-first-boot.md](01-first-boot.md).
- Разбираетесь почему устройство не появляется в приложении → [02-claiming-flow.md](02-claiming-flow.md).
- Меняете настройки через меню и хотите понять поток → [03-remote-config.md](03-remote-config.md).
- Реализуете многоэтапную сушку на MCU → [04-profile-mode.md](04-profile-mode.md).

## Связанное

- [../03-uart/](../03-uart/index.md) — справочник UART-кадров, используемых в этих сценариях.
- [../04-mqtt/](../04-mqtt/index.md) — JSON-форматы, используемые в этих сценариях.
- [../05-cloud/](../05-cloud/index.md) — HTTP-часть.
