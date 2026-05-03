# Как добавить actuator

**Actuator** (исполнитель) — продуктовый компонент, управляющий железом: LED-лента, нагреватель, реле, мотор, сервопривод. Actuator принимает команды и меняет физическое состояние устройства.

В `idryer-core` actuator подключается через `ActionDispatcher` (для `commands/invoke`) и `IProfile::applyConfig` (для `commands/set`). Эталон — `LedStripExecutor` в Storage Link (`src/storage/led_strip/`).

## Шаблон actuator-класса

```cpp
// src/myproduct/actuators/MyActuator.h
#pragma once
#include <ArduinoJson.h>

class MyActuator {
public:
    MyActuator(int pin) : pin_(pin) {}

    void begin();
    bool execute(const char* action, JsonObjectConst args);   // вызывается из onInvoke
    void loop();                                              // таймеры, off-by-timer и т.п.

    void setIntensity(uint8_t value);
    bool isActive() const { return active_; }

private:
    int     pin_;
    bool    active_ = false;
    uint32_t offAt_ = 0;
};
```

`execute` принимает имя action и аргументы из payload `commands/invoke`. Возвращает `true` если action распознан и применён, `false` — если action для другого actuator.

```cpp
bool MyActuator::execute(const char* action, JsonObjectConst args) {
    if (strcmp(action, "myactuator.on") == 0) {
        uint32_t durationSec = args["duration"] | 0;
        analogWrite(pin_, args["intensity"] | 255);
        active_ = true;
        offAt_  = (durationSec > 0) ? millis() + durationSec * 1000 : 0;
        return true;
    }
    if (strcmp(action, "myactuator.off") == 0) {
        analogWrite(pin_, 0);
        active_ = false;
        return true;
    }
    return false;
}
```

`loop()` обрабатывает таймеры (например, "включи на 30 секунд"):

```cpp
void MyActuator::loop() {
    if (active_ && offAt_ != 0 && millis() >= offAt_) {
        analogWrite(pin_, 0);
        active_ = false;
    }
}
```

Принципы:

- **Не пишите `delay()` внутри `execute`**. `execute` вызывается синхронно из MQTT-callback — блокировка ломает MQTT-сессию.
- **Off-by-timer делайте в `loop()`**, не в задаче FreeRTOS. Простое сравнение `millis() >= offAt_` достаточно.
- **`execute` идемпотентен**. Повторный вызов с теми же аргументами должен давать тот же результат.

## Подключение actuator к `commands/invoke`

`ActionDispatcher` маршрутизирует `commands/invoke` через зарегистрированный `InvokeHandler`. В composition root:

```cpp
// в main.cpp

static MyActuator s_actuator(MY_PIN);

static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    return s_actuator.execute(action, args);
}

void setup() {
    s_actuator.begin();
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    // ...
}

void loop() {
    s_runtime.loop();
    s_actuator.loop();
}
```

`handleCommand` (продуктовый) делегирует invoke в dispatcher:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... продуктовые команды ...
}
```

`s_dispatcher.handleInvoke(data)` извлекает `data["action"]` и `data["args"]` и вызывает `onInvoke`.

## Несколько actuator'ов

`InvokeHandler` один на устройство. Если actuator'ов несколько, в `onInvoke` диспатчите по префиксу:

```cpp
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    if (strncmp(action, "led.", 4) == 0)    return s_ledExecutor.execute(action, args);
    if (strncmp(action, "heater.", 7) == 0) return s_heater.execute(action, args);
    if (strncmp(action, "fan.", 4) == 0)    return s_fan.execute(action, args);
    return false;
}
```

Префикс — продуктовое соглашение (`led.pulse`, `heater.set_target` и т.п.).

## Конфигурация actuator через `commands/set`

`commands/set` приходит как `{"id": N, "val": V}`. Эта команда меняет конфиг, не запускает действие. Маршрутизируется через `IProfile::applyConfig`.

В профиле (`MyProfile : public IProfile`):

```cpp
bool MyProfile::applyConfig(int id, int val) override {
    switch (id) {
        case CFG_MAX_INTENSITY:
            menu.max_intensity = val;
            menu.saveToNVS();
            actuator_->setMaxIntensity(val);
            return true;
        case CFG_PIN:
            // pin требует перезагрузки — сохраняем, применим при следующем boot
            menu.pin = val;
            menu.saveToNVS();
            return true;
    }
    return false;   // неизвестный id — изменения игнорируются
}
```

`id` приходит из бэкенда и должен совпадать с ID, который продукт ожидает (обычно из сгенерированного `menu_ids.h`).

## Отчёт о состоянии actuator

Если бэкенду нужно знать состояние actuator (включён/выключен, оставшееся время) — публикуйте это:

- Периодически — через продуктовый publisher в `telemetry`.
- По изменению — через `MqttClient::publishStatus` или `MqttClient::publishEvent`.
- По запросу — через action `myactuator.state`, который публикует ответ.

## Чего не делаем

- Не вызываем `s_dispatcher.handleInvoke` сами из actuator. Actuator — низкоуровневый слой, он принимает команды, а не диспатчит.
- Не делаем actuator `IProfile`. `IProfile` — это контракт config/info/lifecycle. Actuator — отдельный класс, на который профиль может ссылаться (как `LedStripProfile` ссылается на `LedStripExecutor`).
- Не публикуем напрямую из `execute`. Если ответ о выполнении нужен — публикуйте отдельно, после возврата управления в `loop()`.

## Связанные документы

- [04-runtime/01-idryer-runtime.md](../04-runtime/01-idryer-runtime.md) — как `IdryerRuntime` маршрутизирует команды.
- [08-profiles-and-products/01-profiles-model.md](../08-profiles-and-products/01-profiles-model.md) — `ActionDispatcher` и `IProfile`.
- [03-mqtt/02-topics-and-messages.md](../03-mqtt/02-topics-and-messages.md) — формат `commands/invoke` и `commands/set`.
