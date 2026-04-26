# UART-протокол

Бинарный протокол обмена между MCU (контроллером) и LINK (сетевым модулем). Этот раздел — **нормативный справочник**: структура кадра, CRC16, типы сообщений, байт-в-байт офсеты каждого payload, ACK/retry/фрагментация, живые hex-дампы.

| Документ | Содержание |
|----------|------------|
| [01-physical-layer.md](01-physical-layer.md) | Физика: 115200 8N1, пины, level-shifter для 5 В |
| [02-frame-and-crc.md](02-frame-and-crc.md) | Структура кадра, CRC-16/CCITT-FALSE, с рабочим кодом на C |
| [03-message-types.md](03-message-types.md) | Таблица всех MessageKind со статусами реализации |
| [04-binary-structures.md](04-binary-structures.md) | Офсеты, размеры и типы полей всех payload-ов |
| [05-ack-retry.md](05-ack-retry.md) | ACK, retry с backoff, фрагментация ConfigPush и RFID |
| [06-examples.md](06-examples.md) | **14 живых hex-дампов** с проверенными CRC |

## Если вы впервые здесь

Самая тяжёлая часть для новичка. Если в concept-ах типа «little-endian», «`#pragma pack`», «битовые операции» плаваете — обязательно сначала [../01-overview/05-prerequisites.md](../01-overview/05-prerequisites.md). Без этого раздел читается как шифр.

## Порядок чтения

- Пишете свой парсер / собиратель с нуля → читайте по порядку 01 → 06.
- Уже есть готовый кадр на руках и нужно понять что внутри → сразу в [06-examples.md](06-examples.md), оттуда по ссылкам на конкретные структуры.
- Работаете с `UartBridge` из библиотеки → достаточно [03-message-types.md](03-message-types.md) (список типов) и [04-binary-structures.md](04-binary-structures.md) (формы данных), реализацию не трогаете.

## Что дальше

- [../04-mqtt/](../04-mqtt/index.md) — как UART-данные превращаются в MQTT-сообщения.
- [../06-flows/](../06-flows/index.md) — сквозные сценарии, где UART-кадры идут в контексте.
