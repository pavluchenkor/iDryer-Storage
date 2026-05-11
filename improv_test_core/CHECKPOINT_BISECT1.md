# Checkpoint BISECT 1 — РАБОТАЕТ

Дата: 2026-05-11

## Что подтверждено
- `improv_test/` (чистый Improv, без idryer-core) — **РАБОТАЕТ**
- `improv_test_core/` (Improv + глобальный `iDryer::Link s_link(CFG)` БЕЗ
  вызова `s_link.begin()` и `s_link.loop()`) — **РАБОТАЕТ**
- `improv_test_core/` с вызовом `s_link.begin()` + `s_link.loop()` — **TIMEOUT**
- Storage main.cpp с idryer-core — **TIMEOUT**
- iHeater Link main.cpp с idryer-core — **TIMEOUT**

## Вывод
- Конструкторы `Impl` (создание `MqttClient`, `CloudStateMachine`, `LocalAccess`,
  `IntegrationsManager`, `ImprovWiFi(&Serial)` и т.д.) **НЕ ломают** Improv.
- Ломает что-то в `Link::begin()` или `Link::loop()` (даже с DIAG-патчем
  «pre-WL_CONNECTED только Improv»).

## Что в idryer-core сейчас (рабочее состояние)
- `Link::loop()` — DIAG-патч: pre-WL_CONNECTED выполняется только
  `improv.handleSerial()`, остальное паузится. Цена: cloud SM не работает
  pre-WL_CONNECTED → нет auto-reconnect к сохранённому WiFi после reset.
- `Link::begin()` — оригинал.
- `ArduinoWifiManager` — оригинал (с блокирующим scanNetworks).
- `Improv-callback` — оригинал (без Serial.printf).

## Файлы snapshot (для отката)
- `src/main.cpp.bisect1-WORKS` — рабочий main.cpp
- `idryer-core_iDryer.cpp.bisect1-WORKS` — копия рабочего iDryer.cpp с DIAG

## Как воспроизвести
```bash
cd /Users/ruslanpavlucenko/Projects/iDryerProject/docs/iDryer-Storage/improv_test_core
pio run -t clean
python3 -m esptool --chip esp32c3 --port /dev/cu.usbmodem11401 erase_flash
pio run -t upload
# Затем install.idryer.org → Improv → подключается
```

## Как откатиться к этому состоянию (если что-то сломал в bisect 2+)
```bash
# Восстановить main.cpp:
cp src/main.cpp.bisect1-WORKS src/main.cpp

# Восстановить iDryer.cpp:
cp idryer-core_iDryer.cpp.bisect1-WORKS \
   /Users/ruslanpavlucenko/Projects/iDryerProject/docs/idryer-core/src/iDryer.cpp
```

## Следующий шаг (BISECT 2)
В `improv_test_core/main.cpp` добавить вызов `s_link.begin()` в setup()
(но БЕЗ `s_link.loop()` в loop). Если **ломается** — виновен `Link::begin()`,
ищем какая именно строка в нём (поэтапное отключение). Если **работает** —
виновен `Link::loop()` даже с DIAG-патчем (что странно — нужно искать).
