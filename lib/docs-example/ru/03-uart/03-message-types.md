# Типы сообщений (MessageKind)

Исчерпывающий список всех типов UART-кадров с направлением, ожидаемой длиной payload и статусом в актуальном релизе библиотеки. Если работаете над приёмной стороной — это ваш чек-лист.

!!! note "Источник правды"
    `enum class MessageKind` в `src/uart/uart_protocol.h`. Любое расхождение с этой таблицей — баг документации.

---

## Условные обозначения

- **Направление** — первичное направление. Кадры ACK могут идти в обратную сторону — см. соответствующую колонку.
- **LEN** — ожидаемая длина payload. «Ф» — фрагментируется.
- **ACK** — требуется ли подтверждение. «Опц.» — отправитель может выбирать, `ackRequired` в API.
- **Статус** — зрелость в релизе. ✅ работает; ⚠️ частично; ❌ только контракт.

---

## Полная таблица

| Код | Имя | Направление | LEN | ACK | Статус | Назначение |
|-----|-----|-------------|-----|-----|--------|------------|
| `0x01` | `Hello` | MCU → LINK | 86 | Опц. | ✅ | Приветствие MCU, роль, версия, units, mcuSerial |
| `0x01` | `Hello` (HelloRequest) | LINK → MCU | 86 | — | ✅ | Триггер с `role=0xFF`, просит MCU прислать свой Hello |
| `0x02` | `HelloAck` | LINK → MCU | 37 | — | ✅ | Ответ: IP + SSID |
| `0x10` | `Telemetry` | MCU → LINK | 29 | Опц. | ✅ | Температура, влажность, нагреватель, вентилятор |
| `0x11` | `TelemetryAck` | LINK → MCU | 2 | — | ✅ | Подтверждение Telemetry |
| `0x12` | `Weights` | MCU → LINK | 17 | Опц. | ✅ | Веса филамента, до 4 датчиков |
| `0x13` | `Status` | MCU → LINK | 133 | Опц. | ✅ | Режимы, таймеры, сессии |
| `0x14` | `Rfid` | MCU → LINK | 37 | Опц. | ✅ | События tag_detected / tag_removed |
| `0x1A` | `RfidReadData` | MCU → LINK | Ф | Опц. | ✅ | Данные метки для чтения. Фрагментируется по 163 Б, сборка по `FLAG_LAST_FRAGMENT` |
| `0x1B` | `RfidWriteData` | LINK → MCU | Ф | Да | ✅ | Фрагмент данных для записи. Stop-and-wait ACK flow control |
| `0x20` | `Command` | LINK → MCU | 13 или 64 | Да | ✅ | Команда Start/Stop/… (13) **или** ProfilePayload (64) |
| `0x21` | `CommandAck` | MCU → LINK | 2 | — | ✅ | Подтверждение Command |
| `0x30` | `ConfigPush` | Обе стороны | ≤200, Ф | Да | ✅ | JSON-конфиг меню (полный или delta), фрагментируется |
| `0x31` | `ConfigAck` | Обе стороны | 2 | — | ✅ | Подтверждение ConfigPush |
| `0x40` | `Heartbeat` | Обе стороны | 9 | Нет | ✅ | Периодическое «жив», uptime, RSSI, cloudState |
| `0x50` | `Error` | Обе стороны | 4 | Нет | ✅ | Код ошибки, SEQ проблемного кадра, детали |
| `0x60` | `Log` | MCU → LINK | 164 | Нет | ✅ | Структурированное событие приложения |
| `0x70` | `ClaimStart` | MCU → LINK | 0 | Да | ✅ | Запрос на начало привязки |
| `0x71` | `ClaimStatus` | LINK → MCU | 18 | Нет | ✅ | PIN и остаток времени |
| `0x72` | `ClaimComplete` | LINK → MCU | 38 | Нет | ✅ | Сообщает успех/ошибку claim, deviceId |
| `0x73` | `WsEnable` | MCU → LINK | 4 | Нет | ✅ | Включить/выключить локальный WebSocket |
| `0x74` | `WsStatus` | LINK → MCU | 6 | Нет | ✅ | Статус WebSocket-сервера |
| `0x75` | `WsResetClients` | MCU → LINK | 0 | Нет | ✅ | Сбросить привязки WS-клиентов |
| `0x76` | `WsStatusRequest` | MCU → LINK | 0 | Нет | ✅ | Запрос текущего статуса WS |

Зарезервированные коды `0x03–0x0F`, `0x15–0x19`, `0x77–0xFF` — не использовать.

---

## Категории

### Handshake (0x01–0x02)

Инициализация связи. См. раздел «Кто говорит первым» в [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md).

### Телеметрия и статус (0x10–0x14, 0x1A, 0x1B)

Периодические данные от MCU. Ожидаемая частота:

- `Telemetry` — раз в секунду (активный режим) / раз в 15 секунд (idle). Константы `TELEMETRY_ACTIVE_INTERVAL_MS`, `TELEMETRY_IDLE_INTERVAL_MS`.
- `Status` — по событию (изменение режима), не по таймеру.
- `Weights` — по изменению веса больше порога либо периодически.
- `Rfid` — по событию (tag_detected / tag_removed), плюс кэш при подключении.
- `RfidReadData` — ответ на `commands/read_rfid`, переменное число фрагментов по размеру user-memory чипа.
- `RfidWriteData` — фрагменты для записи, отправляются после `Command WriteRfid` (0x08). Каждый фрагмент идёт с `FLAG_ACK_REQUIRED` — stop-and-wait flow control. См. [../07-features/01-rfid.md](../07-features/01-rfid.md).

### Команды (0x20–0x21)

Управление от облака. `Command` может нести:

- `CommandPayload` (13 байт) — простая команда (Start, Stop, Find, GetConfig, ReadRfid, ClearErrors и т.д.);
- `ProfilePayload` (64 байта) — запуск профиля сушки с этапами.

**Дискриминация формата:** по длине payload. Получатель должен смотреть на `payloadLength`, а не на `targetState` или иное поле.

### Remote config (0x30–0x31)

Передача JSON-конфига меню между MCU и LINK. Фрагментируется чанками по 194 байта. Подробно: [05-ack-retry.md](05-ack-retry.md) и [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

### Служебные (0x40–0x60)

- `Heartbeat` — каждые 5 секунд. В кадре LINK → MCU несёт `cloudState` (состояние подключения к облаку).
- `Error` — при ошибке парсера или валидации.
- `Log` — структурированные события приложения (критические ошибки железа, предупреждения). В референсной прошивке LINK публикуются в MQTT-топик `events` (если прикладной код подписан на `setLogHandler`).

### Claiming (0x70–0x72)

Привязка устройства к аккаунту пользователя через портал. Инициируется MCU по действию пользователя (кнопка меню). Подробно: [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).

### WebSocket local (0x73–0x76)

Локальный канал управления без облака. Подробно: [../07-features/02-websocket-local.md](../07-features/02-websocket-local.md).

---

## Статусы фич — детально

### ⚠️ Частичное

- **Фрагментация `ConfigPush` из MQTT для больших `set`/`invoke`** — код отправляет «fragmentation not implemented» при попытке передать JSON больше `CONFIG_CHUNK_DATA_SIZE`. Для конфигов с MCU фрагментация работает.

### ℹ️ Важные особенности RFID

- **`RfidWriteData` (0x1B)** использует **stop-and-wait ACK flow control**: каждый фрагмент идёт с `FLAG_ACK_REQUIRED`, LINK не шлёт следующий до получения `CommandAck`. ACK timeout 200 мс, до 3 ретраев, потом abort с логом в `events`. См. [../07-features/01-rfid.md](../07-features/01-rfid.md).
- **`RfidReadData` (0x1A)** — переменное число фрагментов, сборка по `FLAG_LAST_FRAGMENT`, не по счётчику.

---

## Что дальше

- [04-binary-structures.md](04-binary-structures.md) — детальные офсеты каждой структуры payload.
- [05-ack-retry.md](05-ack-retry.md) — механика ACK, retry и фрагментации.
- [06-examples.md](06-examples.md) — живые hex-дампы.
