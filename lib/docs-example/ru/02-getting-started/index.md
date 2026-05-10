# Быстрый старт

Пять документов. Первый — выбор сценария (что именно вы делаете). Три квикстарта по сценариям. Пятый — как привязать устройство к аккаунту (в т.ч. для разработки без flasher-portal).

| Документ | Для кого |
|----------|----------|
| [01-choose-your-path.md](01-choose-your-path.md) | Выбор траектории из трёх |
| [02-quickstart-controller.md](02-quickstart-controller.md) | Свой контроллер (MCU): ручная сборка UART-кадра, CRC, hex-дампы |
| [03-quickstart-bridge.md](03-quickstart-bridge.md) | Свой сетевой модуль (LINK) на ESP32 + PlatformIO |
| [04-quickstart-standalone.md](04-quickstart-standalone.md) | Один ESP32 за всё: цикл до Online, публикация телеметрии |
| [05-dev-claim.md](05-dev-claim.md) | Три способа привязки устройства: экран / flasher-portal / Serial Monitor (для dev-теста) |

## Рекомендуемый порядок

1. Прочтите [01-choose-your-path.md](01-choose-your-path.md) — поймёте свой сценарий за 2 минуты.
2. Читайте тот квикстарт, который под ваш сценарий — 02 / 03 / 04.
3. Когда прошивка собирается и выходит на WiFi — [05-dev-claim.md](05-dev-claim.md): как привязать устройство к аккаунту.

## Что дальше

После квикстарта — углубление:

- [../03-uart/](../03-uart/index.md) — весь UART-протокол подробно.
- [../04-mqtt/](../04-mqtt/index.md) — MQTT-контракт.
- [../05-cloud/](../05-cloud/index.md) — HTTP API портала.
- [../06-flows/](../06-flows/index.md) — сквозные сценарии.
- [../07-features/](../07-features/index.md) — опциональные фичи (RFID, WebSocket, LINK-интеграции).
