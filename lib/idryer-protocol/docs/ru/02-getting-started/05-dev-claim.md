# Привязка устройства: штатные и dev-пути

Claim — процесс привязки устройства к аккаунту пользователя на портале iDryer. Результат: устройство получает `deviceToken` (MQTT-пароль), портал получает запись в БД с привязкой к пользователю, устройство видно в приложении.

Этот документ описывает **три способа** как провести claim, отсортированных от пользовательского UX к dev-тесту:

1. [Путь 1 — через экран устройства](#путь-1--через-экран-устройства) (штатный для iDryer).
2. [Путь 2 — через веб-инсталлер](#путь-2--через-веб-инсталлер-flasher-portal) (штатный для устройств без экрана).
3. [Путь 3 — через Serial Monitor](#путь-3--через-serial-monitor-dev-тест) (dev-тест, когда flasher-portal ещё не готов).

Все три варианта используют **один и тот же HTTP API** — [05-cloud/02-http-api.md](../05-cloud/02-http-api.md). Разница только в том, как пользователь получает PIN и как видит результат.

!!! note "Контекст"
    Общая архитектура claim (что такое PIN, зачем `deviceToken`, что делает backend) — [05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md). Сквозной сценарий с UART-кадрами — [06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).

---

## Путь 1 — через экран устройства

Штатный способ для устройств с локальным UI (iDryer).

### Что видит пользователь

1. Включает устройство, подключает к WiFi (через меню).
2. В меню выбирает **«Начать привязку»** (или аналог).
3. Устройство показывает **PIN (8 цифр)** на экране + обратный отсчёт 10 минут.
4. Пользователь открывает приложение iDryer / [portal.idryer.org](https://portal.idryer.org), «Добавить устройство» → вводит PIN.
5. Через несколько секунд экран устройства показывает «Успешно привязано» + имя, которое ввёл пользователь в приложении.

### Что делает прошивка

```cpp
// по событию «пользователь нажал 'начать привязку' в меню»
cloud.requestClaim();
```

Всё остальное делает `CloudStateMachine`: переходит через `Provisioning → Registering → AwaitingClaim → Ready → Online`. Прошивка подписана на колбэки и показывает PIN на экране:

```cpp
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresInSeconds, void*) {
    display.showPin(pin, expiresInSeconds);
}, nullptr);

cloud.setClaimCompleteCallback([](const char* deviceId, void*) {
    display.showMessage("Подключено!");
}, nullptr);
```

Этот способ используется в референсной прошивке iDryer Link — см. `src/IdryerDevice.cpp`, обработчики `onClaimPin` / `onClaimComplete`.

---

## Путь 2 — через веб-инсталлер (flasher-portal)

Штатный способ для устройств **без экрана** или когда пользователю удобнее прошить и привязать «в один клик» через браузер.

### Что видит пользователь

1. Открывает `portal.idryer.org` в браузере с поддержкой Web Serial (Chrome/Edge).
2. Подключает устройство через USB.
3. Кликает «Прошить» — flasher-portal через Web Serial заливает прошивку.
4. После перезагрузки устройство через Serial сообщает свой `mcuSerial`:
   ```
   RP2040_SERIAL:36B955AB4350FEDC
   ```
5. Портал ловит эту строку, вызывает `POST /devices/provision` + `/register`, получает PIN.
6. PIN показывается **прямо в браузере** — пользователь сразу видит его рядом с устройством и вводит в приложение (или портал делает это автоматически, если пользователь уже залогинен).

### Что делает прошивка

В прошивке: при первом запуске после заливки отправить `mcuSerial` в Serial — чтобы flasher мог его прочитать:

```cpp
// в IdryerDevice::handleRpHello или в setup()
Serial.printf("RP2040_SERIAL:%s\n", mcuSerial);
```

Сам claim-цикл — тот же `cloud.requestClaim()` (или автоматический старт при пустом NVS). PIN приходит в колбэк — прошивка может вывести его в тот же Serial для удобства инсталлера:

```cpp
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresIn, void*) {
    Serial.printf("CLAIM_PIN:%s\n", pin);
}, nullptr);
```

flasher-portal ловит строку `CLAIM_PIN:...` и красиво показывает её в UI браузера.

### Когда недоступен

Если flasher-portal **ещё не знает про твой тип устройства** (например, вы разрабатываете новый продукт — iHeater / LinkII / свой собственный), этот путь временно не работает. Используйте [Путь 3](#путь-3--через-serial-monitor-dev-тест) пока не подключите новый тип к flasher.

---

## Путь 3 — через Serial Monitor (dev-тест)

Нужен когда:

- Устройство в разработке, экрана пока нет либо UI привязки ещё не написан.
- flasher-portal не настроен под ваш тип устройства.
- Вы хотите быстро проверить MQTT-подключение без возни с UI.

### Идея

Любая прошивка на базе `idryer-protocol` может принимать команду `claim` **из Serial-порта** — это 5 строк кода. Ты в Serial Monitor набираешь слово `claim`, устройство запускает цикл, PIN появляется прямо в логе. Ты руками вводишь PIN в веб-приложение iDryer — готово.

### Минимальный код в прошивке

В `loop()`:

```cpp
void loop() {
    cloud.loop();
    // ... остальная работа ...

    // Dev-триггер claim через Serial. Снять в проде.
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "claim") {
            Serial.println("[USER] Starting claim...");
            cloud.requestClaim();
        }
    }
}
```

И колбэк PIN — чтобы видеть его в логе (если не уже есть):

```cpp
// в setup(), рядом с остальной настройкой cloud
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresIn, void*) {
    Serial.println();
    Serial.println("================================");
    Serial.printf("  PIN: %s\n", pin);
    Serial.printf("  Введите в приложении iDryer\n");
    Serial.printf("  Действителен: %u сек\n", expiresIn);
    Serial.println("================================");
    Serial.println();
}, nullptr);

cloud.setClaimCompleteCallback([](const char* deviceId, void*) {
    Serial.printf("[CLOUD] Claimed! deviceId=%s\n", deviceId);
}, nullptr);
```

Полный рабочий пример — `examples/esp32_standalone/esp32_standalone.ino` в корне репо библиотеки. Этот пример запускается как есть, показывает всю цепочку.

### Последовательность действий разработчика

1. **Залить прошивку** (PlatformIO `pio run -t upload`, или любой Flasher).
2. **Открыть Serial Monitor** на 115200 бод.
3. Дождаться подключения к WiFi:
   ```
   [CLOUD] State: WifiConnecting -> Ready
   ```
4. **Ввести в терминал `claim` + Enter**. Устройство отвечает:
   ```
   [USER] Starting claim...
   [CLOUD] State: Ready -> Provisioning
   [CLOUD] State: Provisioning -> Registering
   [CLOUD] State: Registering -> AwaitingClaim

   ================================
     PIN: 12345678
     Введите в приложении iDryer
     Действителен: 600 сек
   ================================
   ```
5. Открыть в браузере [portal.idryer.org](https://portal.idryer.org), залогиниться, «Добавить устройство», ввести PIN `12345678`, дать устройству имя.
6. В Serial появится подтверждение:
   ```
   [CLOUD] State: AwaitingClaim -> Ready
   [CLOUD] Claimed! deviceId=3fa85f64-...
   [CLOUD] State: Ready -> MqttConnecting
   [CLOUD] State: MqttConnecting -> Online
   ```

Устройство видно в приложении, телеметрия идёт, команды принимаются.

### Автоклэйм при первом запуске

Если не хочется каждый раз писать `claim` в терминал, можно автоматизировать — устройство само стартует цикл, если в NVS нет токена:

```cpp
void setup() {
    // ... обычная инициализация ...
    cloud.begin();

    // Первый запуск: NVS пуста → автоматически запускаем claim
    if (!cloud.getIdentity().hasToken()) {
        Serial.println("[DEV] No token in NVS — auto claim");
        cloud.requestClaim();
    }
}
```

После claim'а `deviceToken` сохраняется в NVS, и при следующем boot устройство **сразу** идёт в `WifiConnecting → Ready → MqttConnecting → Online` без нового PIN-а.

### Как сбросить claim для повторного теста

Если хочется протестировать flow заново — сбросить токен в NVS:

```cpp
// одноразово вставить в setup() перед cloud.begin()
store.clear();  // ArduinoCredentialStore::clear() — очистит всё

// или только серийник/токен через Preferences.h
// Preferences prefs;
// prefs.begin("idryer", false);
// prefs.clear();
// prefs.end();
```

Либо через меню устройства (если есть) — «Сбросить настройки».

**Альтернатива с портала:** пользователь удаляет устройство в приложении. После этого повторный `claim` на устройстве выдаст новый PIN → привяжется по новой.

---

## Что важно про ошибки

| Симптом | Причина | Решение |
|---|---|---|
| `State: AwaitingClaim` висит бесконечно | Пользователь не ввёл PIN в течение 10 минут | PIN истёк → устройство повторит `register` или ждёт команду. Перезапустить claim. |
| `provision returned deviceToken: null, isClaimed: true` | Устройство уже привязано на портале | Пользователь должен удалить устройство в приложении, потом claim повторно. |
| `State: Error` | Backend недоступен или WiFi умер во время запроса | Проверить интернет на устройстве, повторить `claim`. |
| PIN в Serial есть, но в приложении «неверный PIN» | Ошиблись цифрой / PIN истёк | Пересчитать, перезапустить claim если истёк. |

Полный справочник состояний `CloudState` и переходов — [05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md).

---

## Что дальше

- [Основные потоки → Claiming](../06-flows/02-claiming-flow.md) — сквозной сценарий с UART-кадрами (для тех, кто делает контроллер + LINK как две платы).
- [HTTP API портала](../05-cloud/02-http-api.md) — детали endpoint-ов `/provision`, `/register`, `/claim`, `/check-claim`.
- [Стандартные команды](../04-mqtt/04-backend-to-device.md) — что доступно после успешного claim.
