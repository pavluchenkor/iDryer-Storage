# Cloud / HTTP API портала

Три документа: обзор процесса привязки устройства (claim), точный HTTP-контракт REST API портала, и интерфейс `ICommandSink` как точка расширения для применения команд (особенно для устройств без UART).

| Документ | Содержание |
|----------|------------|
| [01-claiming-overview.md](01-claiming-overview.md) | Что такое claim, зачем PIN, состояния Link в БД, особые случаи (recovery, истекший PIN) |
| [02-http-api.md](02-http-api.md) | `POST /devices/provision`, `/register`, `/claim`, `GET /devices/check-claim/:token`; с `curl`-примерами |
| [03-command-sink.md](03-command-sink.md) | Интерфейс `ICommandSink` для устройств без UART (`UartCommandSink` vs кастомная реализация) |

## Навигация

- Хотите понять **зачем** claim и кто в нём участвует → [01-claiming-overview.md](01-claiming-overview.md).
- Реализуете HTTP-клиент в своей прошивке → [02-http-api.md](02-http-api.md).
- Делаете standalone-устройство без UART → [03-command-sink.md](03-command-sink.md).

## Связанное

- [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md) — тот же claim, но с UART-кадрами (MCU + LINK).
- [../02-getting-started/05-dev-claim.md](../02-getting-started/05-dev-claim.md) — три практических способа провести claim (экран / flasher-portal / Serial).
