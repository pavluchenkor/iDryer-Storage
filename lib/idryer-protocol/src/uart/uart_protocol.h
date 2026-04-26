#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * UART протокол между RP2040 (основной контроллер) и ESP32 (сетевой мост LINK).
 * Физика: 115200 бод, 8N1, без аппаратного контроля потока. Стороны держат
 * отдельную очередь команд и ожидают подтверждение с тем же номером
 * последовательности.
 *
 * Кадр:
 * byte 0  : стартовый байт 0xAA
 * byte 1  : версия протокола (PROTOCOL_VERSION)
 * byte 2  : флаги (бит0 — требуется ACK, бит1 — это ACK, бит2 — ошибка, бит3 — фрагмент, бит4 — последний фрагмент)
 * byte 3  : тип сообщения (MessageKind)
 * byte 4  : номер последовательности (0..255, инкремент при каждом кадре)
 * byte 5  : длина полезной нагрузки (0..MAX_PAYLOAD_SIZE)
 * payload : данные
 * CRC16   : младший, затем старший байты (polynom 0x1021, init 0xFFFF)
 */
namespace DryerUart
{

  constexpr uint8_t SOF = 0xAA;
  constexpr uint8_t PROTOCOL_VERSION = 1;

  constexpr uint8_t FLAG_ACK_REQUIRED = 0x01;
  constexpr uint8_t FLAG_IS_ACK = 0x02;
  constexpr uint8_t FLAG_ERROR = 0x04;
  constexpr uint8_t FLAG_FRAGMENTED = 0x08;    // Fragment (not last)
  constexpr uint8_t FLAG_LAST_FRAGMENT = 0x10; // Last fragment

  constexpr uint8_t MAX_PAYLOAD_SIZE = 200;
  constexpr uint8_t MAX_RETRIES = 3;
  constexpr uint16_t HEARTBEAT_INTERVAL_MS = 5000;
  constexpr uint16_t TELEMETRY_ACTIVE_INTERVAL_MS = 1000;
  constexpr uint16_t TELEMETRY_IDLE_INTERVAL_MS = 15000;
  constexpr uint16_t COMMAND_REPLY_TIMEOUT_MS = 700;
  constexpr uint32_t LINK_LOSS_TIMEOUT_MS = 20000;

  // Hello Request retry (LINK → MCU)
  constexpr uint16_t HELLO_REQUEST_INTERVAL_MS = 5000; // Интервал между попытками
  constexpr uint8_t HELLO_REQUEST_MAX_ATTEMPTS = 12;   // Максимум попыток (60 сек)

  enum class Role : uint8_t
  {
    Rp2040Controller = 0x01,
    EspBridge = 0x02,
    HelloRequest = 0xFF, // Триггер от LINK: "MCU, пришли свой Hello"
  };

  // Тип продукта. Передаётся MCU в HelloPayload.deviceType, LINK пробрасывает в MQTT info.
  // Unknown=0 означает legacy-прошивку (поле не заполнено) — портал трактует как Dryer.
  // Количество камер сушилки вычисляется порталом из HelloPayload.unitsCount и не входит в этот enum.
  enum class DeviceType : uint8_t
  {
    Unknown    = 0x00, // Legacy / не заполнено → портал: fallback = Dryer
    Dryer      = 0x01, // iDryer (любое число камер)
    Heater     = 0x02, // iHeater
    Telemetry  = 0x03, // Модуль телеметрии
    Link       = 0x04, // Универсальный LINK (standalone, без MCU-компаньона)
    IHeaterLink = 0x05, // iHeater Link: ESP32-мост к контроллеру iHeater (STM32)
    // 0x06..0xFE — резерв для будущих продуктов
    // 0xFF       — резерв
  };

  enum class MessageKind : uint8_t
  {
    Hello = 0x01,        // RP2040 -> ESP, несёт Role, версию прошивки
    HelloAck = 0x02,     // ESP -> RP2040, подтверждение и сетевой статус
    Telemetry = 0x10,    // RP2040 -> ESP, пакет TelemetryPayload
    TelemetryAck = 0x11, // ESP -> RP2040, подтверждение доставки
    Weights = 0x12,      // RP2040 -> ESP, вес филамента (до 4 датчиков)
    Status = 0x13,       // RP2040 -> ESP, режим работы юнитов (IDLE/DRYING/PROFILE)
    Rfid = 0x14,         // RP2040 -> ESP, RFID события (tag_detected/tag_removed)

    // Коды 0x15-0x19 зарезервированы (ранее использовались для старого claiming протокола)

    // RFID Data Protocol
    RfidReadData = 0x1A,  // RP2040 -> ESP, данные прочитанные с метки (888 байт, фрагментированные)
    RfidWriteData = 0x1B, // ESP -> RP2040, данные для записи на метку (888 байт, фрагментированные)

    Command = 0x20,      // ESP -> RP2040: CommandPayload (13 B) или ProfilePayload (64 B)
    CommandAck = 0x21,   // RP2040 -> ESP, подтверждение / отказ
    ConfigPush = 0x30,   // Обе стороны, JSON конфиг меню (полный/delta, фрагментированный)
    ConfigAck = 0x31,    // Обе стороны, подтверждение ConfigPush
    Heartbeat = 0x40,    // обе стороны, несёт uptime и уровни RSSI/питания
    Error = 0x50,        // несёт ErrorPayload
    Log = 0x60,          // произвольные диагностические сообщения
    ClaimStart = 0x70,   // RP2040 -> ESP, запрос начала claiming (пустой payload)
    ClaimStatus = 0x71,  // ESP -> RP2040, статус claiming (PIN, waiting)
    ClaimComplete = 0x72, // ESP -> RP2040, claiming завершен (deviceId)

    // WebSocket Local Access Protocol (0x73-0x76)
    // @see docs/09-features/02-ws-local-access.md
    WsEnable = 0x73,        // RP2040 -> ESP, включить/выключить WS сервер
    WsStatus = 0x74,        // ESP -> RP2040, статус WS (для отображения на экране)
    WsResetClients = 0x75,  // RP2040 -> ESP, сбросить все привязанные клиенты
    WsStatusRequest = 0x76  // RP2040 -> ESP, запросить текущий статус WS
  };

  enum class DryerState : uint8_t
  {
    Idle = 0,
    Preheat = 1,
    Drying = 2,
    Cooling = 3,
    Fault = 4,
    Service = 5,
  };

  enum class DryerMode : uint8_t
  {
    Idle = 0,    // Устройство бездействует
    Drying = 1,  // Простая сушка (температура + время)
    Storage = 2, // Хранение (температура, без времени)
    Profile = 3, // Профильный режим (многоэтапный)
    Fault = 4,   // Ошибка
  };

  enum class StagePhase : uint8_t
  {
    Ramp = 0, // Фаза разгона (нагрев до целевой температуры)
    Hold = 1, // Фаза удержания (поддержание целевой температуры)
  };

  enum class CommandCode : uint8_t
  {
    // MQTT команды (Backend → ESP → RP2040):
    Start = 0x01,     // Запуск режима (DRYING/STORAGE/PROFILE)
    Stop = 0x02,      // Остановка юнита
    Find = 0x03,      // Поиск устройства (мигание LED/экрана)
    GetConfig = 0x05, // Запрос JSON конфига меню
    SetConfig = 0x06, // Применить настройки из JSON
    ReadRfid = 0x07,  // Прочитать OpenPrintTag с RFID метки
    WriteRfid = 0x08, // Записать OpenPrintTag на RFID метку

    // Служебные команды (только UART):
    ResetFault = 0x10,  // Сброс ошибки
    WifiStatus = 0x11,  // RP2040 запрашивает IP адрес для отображения на экране
    ClearErrors = 0x12, // ESP32 → RP2040: пользователь подтвердил ошибки (сброс EEPROM лога)
  };

  enum class ErrorCode : uint8_t
  {
    None = 0x00,
    CrcMismatch = 0x01,
    UnknownMessage = 0x02,
    InvalidPayload = 0x03,
    Busy = 0x04,
    Timeout = 0x05,
    SequenceMismatch = 0x06,
  };

  enum class ClaimingStatus : uint8_t
  {
    Idle = 0x00,         // Claiming не активен
    Provisioning = 0x01, // ESP32 выполняет POST /provision
    WaitingClaim = 0x02, // PIN получен, ожидание пользователя
    Claimed = 0x03,      // Устройство успешно привязано
    Error = 0x04,        // Ошибка (нет WiFi, Backend недоступен)
  };

#pragma pack(push, 1)

  struct FrameHeader
  {
    uint8_t sof;
    uint8_t version;
    uint8_t flags;
    MessageKind kind;
    uint8_t sequence;
    uint8_t payloadLength;
  };

  struct Frame
  {
    FrameHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
  };

  // Hardware capabilities битовые флаги
  // Определяют какое оборудование установлено на юните
  namespace UnitCapabilities
  {
    constexpr uint16_t HEATER = (1 << 0);             // Есть нагреватель
    constexpr uint16_t FAN = (1 << 1);                // Есть вентилятор
    constexpr uint16_t SERVO = (1 << 2);              // Есть сервопривод заслонки
    constexpr uint16_t RH_AIR_SENSOR = (1 << 3);      // Есть датчик влажности воздуха
    constexpr uint16_t TEMP_AIR_SENSOR = (1 << 4);    // Есть датчик температуры воздуха
    constexpr uint16_t TEMP_HEATER_SENSOR = (1 << 5); // Есть датчик температуры нагревателя
    // Биты 6-15 зарезервированы для будущих флагов
  }

  // Конфигурация одного юнита (камеры)
  // Определяет какие датчики весов и RFID ридеры принадлежат данному юниту,
  // а также какое оборудование (актуаторы и сенсоры) установлено
  struct UnitConfig
  {
    uint8_t unitId;        // ID юнита (0-3), соответствует U1=0, U2=1, U3=2, U4=3
    uint8_t _pad1;         // Выравнивание для capabilities (uint16_t должен быть на четном адресе)
    uint16_t capabilities; // Битовые флаги hardware возможностей (см. UnitCapabilities)
    uint8_t scales[4];     // Индексы датчиков весов (0-3), неиспользуемые = 0xFF
    uint8_t rfid[4];       // Индексы RFID ридеров (0-3), неиспользуемые = 0xFF
  } __attribute__((packed));

  struct HelloPayload
  {
    Role role;                // UART: идентификация стороны (MCU/ESP32)
    uint8_t deviceType;       // MQTT info: тип продукта (DeviceType). 0 = legacy (портал: fallback = Dryer)
    uint8_t _pad1[2];         // Выравнивание для firmwareVersion
    uint32_t firmwareVersion; // MQTT info: "1.2.3" (MAJOR<<16 | MINOR<<8 | PATCH)
    uint32_t workTimeCounter; // MQTT info: счётчик наработки (секунды)
    char hardwareVersion[8];  // MQTT info: версия аппаратной платы ("v1.0")
    uint8_t unitsCount;       // MQTT info: количество камер (0-4)
    UnitConfig units[4];      // MQTT info: конфигурация каждого юнита (scales/rfid привязка)
    char mcuSerial[17];       // MQTT info: уникальный серийный номер MCU (flash ID hex + '\0')
  } __attribute__((packed));

  // Compile-time size checks
  static_assert(sizeof(UnitConfig) == 12, "UnitConfig must be 12 bytes");
  static_assert(sizeof(HelloPayload) == 86, "HelloPayload must be 86 bytes");

  struct HelloAckPayload
  {
    uint32_t ipAddress; // IP адрес (network byte order), 0 = нет подключения
    char ssid[33];      // название WiFi сети (null-terminated), "" если нет подключения
  };

  struct TelemetryEntry
  {
    uint8_t unitId;         // 0-3 (U1=0, U2=1, U3=2, U4=3)
    int16_t temperatureC10; // температура *10 (°C)
    uint16_t humidityPct10; // влажность *10 % (452 → 45.2%)
    uint8_t heaterPowerPct; // мощность нагревателя % (0-100)
    uint8_t fanOn;          // статус вентилятора (0/1 → false/true в MQTT)
  };

  struct TelemetryPayload
  {
    uint8_t count;           // Количество юнитов (1-4)
    TelemetryEntry units[4]; // Массив данных телеметрии
  };

  struct CommandPayload
  {
    CommandCode command;
    uint8_t targetState; // используется для StartDry/PushConfig
    uint8_t unitId;      // ID юнита (0-3) или 0xFF для всех юнитов
    uint8_t reserved[2]; // выравнивание
    uint32_t arg0;
    uint32_t arg1;
  };

  struct ConfigPayload
  {
    int16_t targetTemperatureC10;
    uint16_t targetHumidityPct;
    uint16_t durationMinutes;
    uint16_t fanDutyPct;
  };

  // Состояние облачного подключения (LINK → MCU через Heartbeat)
  // Дублирует cloud::CloudState для использования на стороне MCU без зависимости от cloud модуля.
  // MCU отслеживает переходы между состояниями для LED индикации.
  enum class LinkCloudState : uint8_t
  {
    Idle = 0,           // Начальное состояние (до begin())
    WifiConnecting = 1, // Подключение к WiFi
    Provisioning = 2,   // Получение токена (POST /provision)
    Registering = 3,    // Получение PIN (POST /register)
    AwaitingClaim = 4,  // Ожидание привязки (polling /check-claim)
    Ready = 5,          // Готов к MQTT (WiFi + токен + deviceId)
    MqttConnecting = 6, // Подключение к MQTT брокеру
    Online = 7,         // MQTT подключён, полностью онлайн
  };

  struct HeartbeatPayload
  {
    uint32_t uptimeSeconds;
    int16_t wifiRssiDbm;       // ESP указывает RSSI, RP2040 передаёт температуру MCU
    uint16_t errorsSinceBoot;
    uint8_t cloudState;        // LinkCloudState: текущее состояние облачного подключения (ESP→RP2040)
  };

  struct AckPayload
  {
    uint8_t ackSequence;
    ErrorCode status;
  };

  struct ErrorPayload
  {
    ErrorCode code;
    uint8_t lastSequence;
    uint16_t detail; // например, ожидаемая длина, фактическая длина и т. д.
  };

  struct LogPayload
  {
    char severity[10]; // "critical", "error", "warning", "info"
    char source[20];   // "THERMISTOR", "HEATER", "SHT", "SERVO", etc.
    char event[32];    // "SENSOR_SHORT", "OVER_MAX", "NO_RESPONSE", etc.
    char message[100]; // "Thermistor short circuit", "Value over maximum", etc.
    uint8_t unitId;    // 0-3 (controller ID)
    uint8_t _pad;      // выравнивание
  };

  // Device Claiming Protocol
  // @see docs/10-flows/01-basic-flows.md (Claiming Flow)

  struct ClaimStatusPayload
  {
    ClaimingStatus status;     // Статус claiming процесса
    char pin[9];               // PIN код (8 цифр + \0), пусто если не waiting
    uint32_t expiresAt;        // Unix timestamp (секунды), когда истекает PIN
    uint32_t remainingSeconds; // Оставшееся время до истечения PIN
  };

  struct ClaimCompletePayload
  {
    uint8_t success;   // 1 = success, 0 = timeout/error
    char deviceId[37]; // UUID устройства (только для отображения на экране RP2040)
  };

  // WebSocket Local Access Protocol (0x73-0x76)
  // @see docs/09-features/02-ws-local-access.md

  enum class WsState : uint8_t
  {
    Disabled = 0,  // WS сервер выключен
    Listening = 1, // WS сервер запущен, ждёт подключения
    Connected = 2, // Клиент подключён
  };

  struct WsEnablePayload
  {
    uint8_t enable;      // 1 = включить, 0 = выключить
    uint8_t reserved;    // Выравнивание
    uint16_t pin;        // PIN 0-9999 (4 цифры, генерируется на MCU)
  };

  struct WsStatusPayload
  {
    WsState state;       // Текущее состояние WS сервера
    uint16_t pin;        // PIN 0-9999 (4 цифры)
    uint8_t pairedCount; // Кол-во привязанных клиентов (0-5)
    uint8_t maxClients;  // Максимум привязанных клиентов (5)
    uint8_t reserved;    // Выравнивание
  };

  // Weights Protocol (0x12); MQTT JSON: docs/03-mqtt/01-mqtt.md

  struct WeightEntry
  {
    uint8_t sensorId;        // 0-3 (W1=0, W2=1, W3=2, W4=3)
    uint8_t unitId;          // 0-3 (U1=0, U2=1, U3=2, U4=3)
    uint16_t weightGramsC10; // 0-5000
  };

  struct WeightsPayload
  {
    uint8_t count;          // Количество датчиков (1-4)
    WeightEntry weights[4]; // Массив данных весов
  };

  // Status Protocol (0x13); MQTT JSON: docs/03-mqtt/01-mqtt.md

  struct StatusEntry
  {
    uint8_t unitId;                 // 0-3 (U1=0, U2=1, U3=2, U4=3)
    DryerMode mode;                 // IDLE/DRYING/STORAGE/PROFILE/FAULT
    uint32_t sessionNum;            // Порядковый номер сессии (автоинкремент 1++), 0 для IDLE/FAULT
    int16_t targetTempC10;          // Целевая температура ×10 (не передается для IDLE/FAULT)
    uint16_t targetHumidityPct;     // Целевая влажность % (0 = не используется)
    uint16_t durationMinutes;       // Длительность (0 = бесконечно/STORAGE)
    uint32_t elapsedSeconds;        // MQTT: totalElapsed - секунд прошло с начала работы
    uint32_t stageElapsedSeconds;   // MQTT: stageElapsed - секунд на текущем этапе (PROFILE)
    uint32_t stageRemainingSeconds; // MQTT: stageRemaining - секунд до конца этапа (PROFILE)
    uint32_t totalRemainingSeconds; // MQTT: totalRemaining - секунд до конца программы
    uint8_t currentStage;           // Номер текущего этапа (для PROFILE, 0-based)
    uint8_t totalStages;            // Общее количество этапов (для PROFILE)
    StagePhase stagePhase;          // Фаза текущего этапа: RAMP или HOLD (для PROFILE)
    uint8_t _pad;                   // Выравнивание
  };

  struct StatusPayload
  {
    uint8_t count;        // Количество юнитов (1-4)
    StatusEntry units[4]; // Массив статусов юнитов
    uint32_t uptime;      // Uptime устройства в секундах (общий для всех юнитов)
  };

  // RFID Protocol (0x14); MQTT JSON: docs/03-mqtt/01-mqtt.md

  enum class RfidEvent : uint8_t
  {
    TagDetected = 1,      // Метка обнаружена
    TagRemoved = 2,       // Метка удалена
    ReaderUnavailable = 3, // Ридер занят или метки нет (ответ на ReadRfid/WriteRfid)
  };

  struct RfidPayload
  {
    RfidEvent event;  // TagDetected / TagRemoved
    uint8_t readerId; // 0-3 (R1=0, R2=1, R3=2, R4=3)
    char tag[32];     // HEX ID метки (пусто для TagRemoved)
    uint8_t unitId;   // 0-3 (U1=0, U2=1, U3=2, U4=3)
    uint8_t _pad[2];  // Выравнивание
  };

  // RFID Data Protocol (0x1A, 0x1B); команды MQTT: docs/03-mqtt/01-mqtt.md; фичи: docs/09-features/

  struct RfidDataPayload
  {
    uint8_t readerId;      // 0-3 (R1=0, R2=1, R3=2, R4=3)
    uint8_t unitId;        // 0-3 (U1=0, U2=1, U3=2, U4=3)
    char tag[32];          // HEX ID метки (для валидации)
    uint8_t fragment[163]; // Фрагмент данных (888 байт / 163 = 6 фрагментов)
    uint8_t _pad[2];       // Выравнивание
  };
  // Размер: 1 + 1 + 32 + 163 + 2 = 199 байт (влезает в MAX_PAYLOAD_SIZE=200)
  // Используется для:
  //   - RfidReadData (0x1A): RP2040 → ESP32 (ответ на команду ReadRfid)
  //   - RfidWriteData (0x1B): ESP32 → RP2040 (данные для записи на метку)

  // Profile Drying Protocol (Command 0x20, payload ProfilePayload 64 B)
  // @see docs/10-flows/07-profile-mode.md, docs/02-uart/02-binary-format.md

  struct ProfileStage
  {
    uint16_t temp; // Температура ×10 (60°C → 600)
    uint16_t ramp; // Время разгона до целевой температуры (секунды)
    uint16_t hold; // Время удержания целевой температуры (секунды)
  };
  // Размер: 2 + 2 + 2 = 6 байт

  struct ProfilePayload
  {
    uint8_t unitId;          // 0-3 (U1=0, U2=1, U3=2, U4=3)
    uint8_t totalStages;     // Количество этапов (1-10)
    uint8_t startStage;      // Индекс этапа для старта (0-based), по умолчанию 0
    uint8_t _pad;            // Выравнивание
    ProfileStage stages[10]; // Массив этапов (фиксированный размер для упрощения)
  };
  // Размер: 1 + 1 + 1 + 1 + (6×10) = 64 байта (влезает в MAX_PAYLOAD_SIZE=200)
  // Кадр MessageKind::Command (0x20), payload целиком = эта структура (см. uart_bridge)

  // =========================================================================
  // Remote Config Protocol (0x30 ConfigPush)
  // @see docs/10-flows/01-basic-flows.md (Remote Config), docs/02-uart/01-uart.md
  // =========================================================================

  // Размер данных в одном чанке: MAX_PAYLOAD_SIZE - sizeof(ConfigChunkHeader)
  constexpr uint8_t CONFIG_CHUNK_HEADER_SIZE = 6;
  constexpr uint8_t CONFIG_CHUNK_DATA_SIZE = MAX_PAYLOAD_SIZE - CONFIG_CHUNK_HEADER_SIZE; // 194 байта

  // Заголовок фрагмента конфигурации (6 байт)
  struct ConfigChunkHeader
  {
    uint16_t transferId; // ID передачи (не смешиваем разные конфиги)
    uint16_t totalSize;  // Полный размер JSON (только в первом фрагменте, chunkIndex==0)
    uint16_t chunkIndex; // Индекс фрагмента: 0, 1, 2, ... N-1
  };

  // Payload для ConfigPush (0x30)
  // Для маленького JSON (без фрагментации): просто JSON данные
  // Для большого JSON (с фрагментацией): ConfigChunkHeader + данные
  struct ConfigChunkPayload
  {
    ConfigChunkHeader header;
    uint8_t data[CONFIG_CHUNK_DATA_SIZE]; // JSON данные (до 194 байт)
  };
  // Размер: 6 + 194 = 200 байт (MAX_PAYLOAD_SIZE)

  // Типы конфига (для определения топика MQTT)
  enum class ConfigType : uint8_t
  {
    Full = 0,  // Полный конфиг -> MQTT topic: config
    Delta = 1, // Delta-обновление -> MQTT topic: config/delta
  };

#pragma pack(pop)

  static_assert(sizeof(FrameHeader) == 6, "Frame header must remain packed");
  static_assert(sizeof(AckPayload) <= MAX_PAYLOAD_SIZE,
                "Ack payload must fit in frame");
  static_assert(sizeof(HelloPayload) <= MAX_PAYLOAD_SIZE,
                "Hello payload must fit in frame");
  static_assert(sizeof(HelloAckPayload) <= MAX_PAYLOAD_SIZE,
                "HelloAck payload must fit in frame");
  static_assert(sizeof(TelemetryPayload) <= MAX_PAYLOAD_SIZE,
                "Telemetry payload must fit in frame");
  static_assert(sizeof(CommandPayload) <= MAX_PAYLOAD_SIZE,
                "Command payload must fit in frame");
  static_assert(sizeof(ConfigPayload) <= MAX_PAYLOAD_SIZE,
                "Config payload must fit in frame");
  static_assert(sizeof(HeartbeatPayload) <= MAX_PAYLOAD_SIZE,
                "Heartbeat payload must fit in frame");
  static_assert(sizeof(ErrorPayload) <= MAX_PAYLOAD_SIZE,
                "Error payload must fit in frame");
  static_assert(sizeof(LogPayload) <= MAX_PAYLOAD_SIZE,
                "Log payload must fit in frame");
  static_assert(sizeof(ClaimStatusPayload) <= MAX_PAYLOAD_SIZE,
                "ClaimStatus payload must fit in frame");
  static_assert(sizeof(ClaimCompletePayload) <= MAX_PAYLOAD_SIZE,
                "ClaimComplete payload must fit in frame");
  static_assert(sizeof(WeightsPayload) <= MAX_PAYLOAD_SIZE,
                "Weights payload must fit in frame");
  static_assert(sizeof(StatusPayload) <= MAX_PAYLOAD_SIZE,
                "Status payload must fit in frame");
  static_assert(sizeof(RfidPayload) <= MAX_PAYLOAD_SIZE,
                "Rfid payload must fit in frame");
  static_assert(sizeof(RfidDataPayload) <= MAX_PAYLOAD_SIZE,
                "RfidData payload must fit in frame");
  static_assert(sizeof(ProfilePayload) <= MAX_PAYLOAD_SIZE,
                "Profile payload must fit in frame");
  static_assert(sizeof(ConfigChunkPayload) <= MAX_PAYLOAD_SIZE,
                "ConfigChunk payload must fit in frame");
  static_assert(sizeof(ConfigChunkHeader) == CONFIG_CHUNK_HEADER_SIZE,
                "ConfigChunkHeader must be exactly 6 bytes");
  static_assert(sizeof(WsEnablePayload) <= MAX_PAYLOAD_SIZE,
                "WsEnable payload must fit in frame");
  static_assert(sizeof(WsStatusPayload) <= MAX_PAYLOAD_SIZE,
                "WsStatus payload must fit in frame");
  inline bool requiresAck(uint8_t flags)
  {
    return (flags & FLAG_ACK_REQUIRED) != 0;
  }

  inline uint8_t makeAckFlags()
  {
    return FLAG_IS_ACK;
  }

  /**
   * Расчёт CRC16-CCITT (0x1021). Реализация добавляется при интеграции;
   * интерфейс вынесен для унификации RP2040 и ESP.
   */
  uint16_t calculateCrc(const uint8_t *data, size_t length);

} // namespace DryerUart
