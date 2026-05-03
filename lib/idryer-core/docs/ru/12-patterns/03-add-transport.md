# Как добавить transport

**Transport** — канал обмена с внешним миром. В `idryer-core` уже встроены два transport'а:

- **MQTT** (`MqttClient`) — облачное подключение через `mqtt.idryer.org`.
- **LocalAccess** (опционально, `local_access/local_access.h`) — LAN WebSocket-сервер на порту 81 + mDNS-обнаружение.

Этот раздел описывает: как один и тот же продуктовый обработчик команд используется обоими transport'ами, и как добавить третий transport, не ломая архитектуру.

## Принцип "один CommandHandler — несколько transport'ов"

И MQTT, и LocalAccess принимают входящие команды и в итоге зовут одну и ту же функцию продукта:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    // ... продуктовый роутинг ...
}
```

Подключение в `setup()`:

```cpp
s_runtime.setCommandHandler(handleCommand);   // MQTT путь
s_local.setCommandSink(handleCommand);        // LocalAccess путь
```

Каждый transport отвечает за **разворачивание envelope**:

- MQTT: топик `idryer/{serial}/commands/<cmd>` → `(cmd, payloadJson)`.
- LocalAccess: `{"type":"command","command":"<cmd>","data":{...}}` → `(cmd, data)`.

После разворачивания обе ветки попадают в одну функцию. Это и есть *single command extension path*.

## Принцип "один publisher — несколько transport'ов"

Для исходящих данных используется тот же подход. `DevicePublisher` — тонкий dual-publish helper, который вызывает `MqttClient` и `LocalAccess` одной строкой:

```cpp
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);

s_pub.publishTelemetry(doc);   // → MQTT + WS, если клиент подключён
s_pub.publishConfig(doc);      // → MQTT + WS
s_pub.publishStatus(doc);      // → MQTT + WS
```

Если LocalAccess не используется — публикуйте напрямую через `s_mqtt.publishTelemetry(doc)`.

## Когда нужен новый transport

Случаи, когда стандартных MQTT + LAN WS недостаточно:

- BLE (например, для конфигурации с приложения телефона без WiFi).
- Локальный HTTP-сервер для legacy-клиентов.
- Serial JSON-протокол для отладки или интеграции с другим устройством.
- Прокси через UART (двухпроцессорные устройства уже используют `UartBridge`, но для Link → Cloud это не "transport", это внутренняя шина).

## Шаблон нового transport

Минимальный transport состоит из двух частей:

1. **Прёмная сторона** — разворачивает envelope и зовёт `handleCommand`.
2. **Передающая сторона** — принимает `JsonDocument` и кодирует в свой протокол.

```cpp
// src/myproduct/transports/MyTransport.h
#include <functional>
#include <ArduinoJson.h>

class MyTransport {
public:
    using CommandSink = std::function<void(const char* command, JsonObjectConst data)>;

    void begin();
    void loop();

    void setCommandSink(CommandSink cb) { sink_ = cb; }

    void publish(const char* type, JsonDocument& doc);
    bool isClientConnected() const { return clientConnected_; }

private:
    CommandSink sink_;
    bool clientConnected_ = false;

    void onIncoming(const char* rawMessage);
};
```

Внутри `onIncoming` парсите сообщение, извлекайте команду и зовите `sink_`:

```cpp
void MyTransport::onIncoming(const char* rawMessage) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, rawMessage)) return;
    const char* cmd = doc["command"];
    if (!cmd) return;
    if (sink_) sink_(cmd, doc["data"].as<JsonObjectConst>());
}
```

## Подключение в composition root

```cpp
// в main.cpp

static MyTransport s_myTransport;

void setup() {
    // ... обычная инициализация ...
    s_myTransport.begin();
    s_myTransport.setCommandSink(handleCommand);   // тот же handler

    s_runtime.setCommandHandler(handleCommand);
    s_local.setCommandSink(handleCommand);
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();
    s_local.loop();
    s_myTransport.loop();
}
```

## Расширение `DevicePublisher`

`DevicePublisher` встроенный — он знает только про `MqttClient` и `LocalAccess`. Если вам нужно публиковать в третий transport, есть два пути:

### Вариант A — продуктовый publisher

Создайте свой класс, который повторяет интерфейс `DevicePublisher`, но добавляет ваш transport:

```cpp
class MyProductPublisher {
public:
    MyProductPublisher(idryer::MqttClient* mqtt, idryer::LocalAccess* local, MyTransport* my)
        : mqtt_(mqtt), local_(local), my_(my) {}

    bool publishTelemetry(JsonDocument& doc) {
        bool ok = mqtt_->publishTelemetry(doc);
        if (local_->isClientConnected()) local_->publish("telemetry", doc);
        if (my_->isClientConnected())    my_->publish("telemetry", doc);
        return ok;
    }
    // ... остальные publish-методы ...
private:
    idryer::MqttClient*  mqtt_;
    idryer::LocalAccess* local_;
    MyTransport*         my_;
};
```

Этот класс заменяет `DevicePublisher` для продукта, но не меняет `idryer-core`.

### Вариант B — рядом с `DevicePublisher`

Используйте `DevicePublisher` для MQTT + LAN WS, а свой transport публикуйте отдельно:

```cpp
s_pub.publishTelemetry(doc);            // MQTT + LAN WS
if (s_myTransport.isClientConnected()) {
    s_myTransport.publish("telemetry", doc);
}
```

## Что не делается в transport

- Transport не парсит и не интерпретирует команду — он только разворачивает envelope.
- Transport не вызывает `IProfile`, `ActionDispatcher` или `MqttClient`. Эту работу делает продуктовый `handleCommand`.
- Transport не хранит state устройства — он только канал.
- Transport не публикует автоматически. Публикует продукт, передавая `JsonDocument`.

## Чего не делаем

- Не пытаемся встроить новый transport в `idryer-core`. Если transport нужен только одному продукту — он живёт в product code.
- Не дублируем `handleCommand`. Один обработчик, несколько transport'ов.
- Не делаем shared state между transport'ами через глобальные переменные. Все связи — через явные указатели в composition root.

## Связанные документы

- [02-architecture/03-data-flow.md](../02-architecture/03-data-flow.md) — общая схема потоков.
- [04-runtime/01-idryer-runtime.md](../04-runtime/01-idryer-runtime.md) — почему `setCommandHandler` — единственная точка расширения.
- LocalAccess: `local_access/local_access.h` — рабочий пример transport'а в библиотеке.
