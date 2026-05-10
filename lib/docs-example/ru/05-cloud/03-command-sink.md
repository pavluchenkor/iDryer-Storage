# ICommandSink — точка расширения для применения команд

`CommandHandler` разбирает входящие MQTT-команды и превращает их в **UART-payload структуры** (`CommandPayload`, `ProfilePayload`, `ConfigChunkPayload`). Но куда эти структуры потом деваются — определяет реализация интерфейса **`ICommandSink`**.

Это позволяет одной и той же логике парсинга команд работать в двух сценариях:

- **UART-мост (iDryer Link):** sink отправляет payload по UART на MCU.
- **Standalone (iHeater Link II):** sink применяет команду локально, управляет нагревателем/датчиками прямо на ESP32.

!!! note
    Интерфейс: `src/cloud/command_sink.h`. Реализация для UART — `src/cloud/uart_command_sink.h`. Потребитель — `src/cloud/command_handler.cpp`.

---

## Интерфейс

```cpp
namespace idryer::cloud {

class ICommandSink {
public:
    virtual ~ICommandSink() = default;

    virtual void sendCommand(const DryerUart::CommandPayload& payload,
                             bool ackRequired) = 0;

    virtual void sendProfileCommand(const DryerUart::ProfilePayload& payload,
                                    bool ackRequired) = 0;

    virtual void sendConfigPushChunk(const DryerUart::ConfigChunkPayload& payload,
                                     uint8_t dataLen, uint8_t flags) = 0;
};

}  // namespace idryer::cloud
```

Сигнатуры 1-в-1 совпадают с соответствующими методами `DryerUart::UartBridge`. Миграция существующих потребителей — чисто механическая.

---

## Реализация для устройств с UART: `UartCommandSink`

Идёт в коробке с библиотекой. Используется в конфигурациях с отдельным MCU.

Файл: `src/cloud/uart_command_sink.h`.

```cpp
#include <idryer_protocol.h>

DryerUart::UartBridge           uart(...);
idryer::cloud::UartCommandSink  sink(&uart);
idryer::cloud::CommandHandler   cmdHandler(&sink);

// При входящей MQTT-команде:
cmdHandler.handleMqttCommand("drying", jsonData);
// → sink.sendCommand(...) → uart.sendCommand(...) → MCU
```

Null-guard: если `uart == nullptr`, вызовы `sendCommand`/`sendProfileCommand`/`sendConfigPushChunk` молча игнорируются. Это упрощает тестирование и dev-конфигурации.

---

## Реализация для standalone: свой sink

Прикладной код пишет свой sink, который применяет команды **локально**, не уходя в UART. Пример для iHeater Link II (ESP32 за всё):

```cpp
#include <idryer_protocol.h>

class LocalHeaterSink : public idryer::cloud::ICommandSink {
public:
    LocalHeaterSink(HeaterController& heater) : heater_(heater) {}

    void sendCommand(const DryerUart::CommandPayload& p, bool /*ack*/) override {
        using DryerUart::CommandCode;

        switch (p.command) {
        case CommandCode::Start:
            // p.arg0 = target temperature × 10
            heater_.setTargetTemperature(p.arg0 / 10.0f);
            heater_.start(p.arg1 /* duration minutes */);
            break;

        case CommandCode::Stop:
            heater_.stop();
            break;

        case CommandCode::ClearErrors:
            heater_.clearFaultLog();
            break;

        default:
            // Другие команды в этом продукте не нужны
            break;
        }
    }

    void sendProfileCommand(const DryerUart::ProfilePayload& p, bool) override {
        heater_.applyProfile(p);   // своя реализация профиля
    }

    void sendConfigPushChunk(const DryerUart::ConfigChunkPayload& p,
                             uint8_t dataLen, uint8_t flags) override {
        // Если пользователь меняет настройку из меню — применить к локальному конфигу
        config_.applyChunk(p, dataLen, flags);
    }

private:
    HeaterController& heater_;
    LocalConfig       config_;
};

// Использование:
LocalHeaterSink               sink(heater);
idryer::cloud::CommandHandler cmdHandler(&sink);

mqtt.setCommandCallback([&](const char* cmd, JsonObjectConst data) {
    cmdHandler.handleMqttCommand(cmd, data);
});
```

Библиотека про `LocalHeaterSink` ничего не знает — это приложение.

---

## Что `ICommandSink` **не делает**

- **Не заменяет UART-протокол.** Он транслирует те же payload-структуры, которые `UartBridge` пишет в провод. В standalone-конфигурации эти структуры остаются «форматом команды», даже если не уходят по UART — это позволяет переиспользовать `CommandHandler` без доработок.
- **Не добавляет высокоуровневый API.** `ICommandSink::sendCommand(CommandPayload, ack)` — это всё ещё низкий уровень. Если нужен API вроде `startDrying(unitId, tempC, durationMin)` — пишите его поверх sink или поверх прикладного кода, это отдельная задача.
- **Не скрывает `UartBridge` от приложения.** На устройствах с MCU приложение по-прежнему держит `UartBridge` для приёма телеметрии; `UartCommandSink` лишь оборачивает **исходящий** канал команд.

---

## Жизненный цикл

Потребитель (приложение) создаёт sink и передаёт указатель в конструктор `CommandHandler`. **Sink должен жить не короче, чем `CommandHandler`.**

В референсной прошивке iDryer Link оба — члены класса `IdryerDevice`:

```cpp
class IdryerDevice {
    DryerUart::UartBridge           uart_;
    idryer::cloud::UartCommandSink  sink_;       // объявлен до cmdHandler_
    idryer::cloud::CommandHandler   cmdHandler_; // sink_ переданы в конструктор
    // ...
};
```

Sink объявлен **до** `cmdHandler_` в теле класса, поэтому инициализируется первым и уничтожается последним. Правило соблюдайте.

---

## Расширения

Интерфейс минимален и расширяется при необходимости. Если вам нужна отдельная ветка «при приходе команды сделать X + отправить на MCU», лучший путь:

1. Оставить `UartCommandSink` как «транспорт».
2. Подписать свой слой перед ним — например, логгировать все команды в NVS, считать метрики, анализировать параметры.

Пример «декоратор»:

```cpp
class LoggingSinkDecorator : public idryer::cloud::ICommandSink {
public:
    LoggingSinkDecorator(idryer::cloud::ICommandSink* inner) : inner_(inner) {}

    void sendCommand(const DryerUart::CommandPayload& p, bool ack) override {
        logCmd(p);
        inner_->sendCommand(p, ack);
    }
    // ...

private:
    idryer::cloud::ICommandSink* inner_;
};
```

---

## Что дальше

- [01-claiming-overview.md](01-claiming-overview.md) — процесс привязки.
- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) — какие JSON-команды превращаются в какие payload-структуры.
- [../06-flows/](../06-flows/) — полные сценарии с участием `CommandHandler` и sink.
