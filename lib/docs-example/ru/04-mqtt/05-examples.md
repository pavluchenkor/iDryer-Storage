# MQTT: примеры для отладки (mosquitto_sub/pub)

Готовые команды для терминала: смотреть, что шлёт ваше устройство, и отправлять команды вручную.

!!! note "Нужно"
    `mosquitto_sub` / `mosquitto_pub` — ставятся из пакета `mosquitto-clients`:
    ```bash
    # macOS:    brew install mosquitto
    # Debian:   sudo apt install mosquitto-clients
    ```

В примерах ниже подставьте:

- `BROKER_HOST` — адрес MQTT-брокера (зависит от окружения).
- `SERIAL` — ваш `serialNumber` (Client ID устройства).
- `TOKEN` — `deviceToken` устройства.

Для публичного продакшена на `portal.idryer.org` параметры запрашивайте у владельца инфраструктуры — эти данные не хранятся в репозитории библиотеки.

---

## Смотреть всё, что пишет устройство

```bash
mosquitto_sub \
  -h BROKER_HOST -p 8883 --cafile /path/to/isrg_root_x1.pem \
  -u SERIAL -P TOKEN \
  -t "idryer/SERIAL/#" -v
```

Ключи:

- `-v` — печатает топик перед payload-ом.
- `--cafile` — корневой сертификат (тот же Let's Encrypt, который зашит в `src/mqtt/root_ca.h`).
- Опускание `--cafile` и переход на `-p 1883` — для локальной разработки без TLS.

Ожидаемый вывод (при живом устройстве):

```
idryer/SERIAL/info {"hardwareVersion":"v1.0","firmwareVersion":"1.2.3",...}
idryer/SERIAL/status {"units":[{"unitId":"U1","mode":"IDLE"}],"uptime":32}
idryer/SERIAL/telemetry {"units":[{"unitId":"U1","temperature":25.3,"humidity":42.1,...}]}
...
```

---

## Смотреть только телеметрию

```bash
mosquitto_sub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -t "idryer/SERIAL/telemetry" -v
```

---

## Отправить команду: запустить сушку

```bash
mosquitto_pub \
  -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN \
  -q 1 \
  -t "idryer/SERIAL/commands/drying" \
  -m '{"unitId":"U1","params":{"temperature":55,"duration":240}}'
```

Ожидаемая реакция:

- В подписке `idryer/SERIAL/status` через ~1 секунду появится новое сообщение с `"mode":"DRYING"`.
- В подписке `idryer/SERIAL/telemetry` показания `heaterPower` начнут расти.

---

## Остановить сушку

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/stop" \
  -m '{"unitId":"U1"}'
```

Через секунду в `status` — `"mode":"IDLE"`.

---

## Запустить профиль

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/profile" \
  -m '{
    "unitId":"U1",
    "stages":[
      {"temp":60,"ramp":300,"hold":1800},
      {"temp":100,"ramp":600,"hold":6000},
      {"temp":70,"ramp":600,"hold":12000}
    ]
  }'
```

`status` перейдёт в `"mode":"PROFILE"`, появятся поля `currentStage`, `stagePhase`, `stageElapsed`, `stageRemaining`.

---

## Запросить полный конфиг меню

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/get_config" \
  -m '{}'
```

Через несколько секунд в `idryer/SERIAL/config` прилетит полный JSON меню.

---

## Поменять одно значение в меню

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/set" \
  -m '{"id":3,"unit":0,"val":55}'
```

Устройство применит и опубликует `config/delta` с подтверждением нового значения.

---

## Ping

Проверить, что устройство онлайн и отвечает на команды (без побочных эффектов):

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/ping" \
  -m '{}'
```

В логе устройства (Serial Monitor) появится запись, что команда принята. MQTT-ответа нет — ожидайте телеметрию или подписывайтесь на `idryer/SERIAL/events`.

---

## Мониторинг оффлайн

Чтобы заранее знать, когда устройство падает по LWT:

```bash
mosquitto_sub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -t "idryer/SERIAL/offline" -v
```

Сообщение `{}` в этом топике = аварийное отключение TCP. Штатное `disconnect()` не публикует LWT.

---

## Отладка TLS

Если клиент не подключается:

- Проверьте, что используете правильный CA. Он в репо — `src/mqtt/root_ca.h` (PEM-формат, удобно сохранить в файл).
- Системные даты клиента и устройства должны быть корректные — иначе TLS-сертификат может показаться «просроченным».
- На Linux: `openssl s_client -connect BROKER_HOST:8883 -CAfile ca.pem` — покажет цепочку сертификатов.

---

## Альтернатива: MQTT Explorer

Для визуальной отладки (видеть дерево топиков, retained-сообщения, фильтры) подходит [MQTT Explorer](https://mqtt-explorer.com/) — десктопный GUI-клиент. Настройте подключение:

- Host: `BROKER_HOST`
- Port: `8883` + TLS
- Username: `SERIAL`
- Password: `TOKEN`
- Custom CA: путь к Let's Encrypt root

Дерево `idryer/SERIAL/*` сразу покажет последние retained-сообщения (info, status, rfid).

---

## Что дальше

- [../05-cloud/](../05-cloud/) — HTTP-цикл провизии и claim (получение `SERIAL` и `TOKEN`).
- [../06-flows/](../06-flows/) — сквозные сценарии: первый запуск, claiming, remote config, profile.
