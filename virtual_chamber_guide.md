# Virtual Chamber (Klipper + WebSocket + iHeater-link)

## Что это

Способ получить из G-code значение температуры камеры (`M141 / M191`)  2
без обязательного наличия heater_generic chamber.

Работает в двух режимах:
- без сенсора — только target
- с сенсором — target + реальная температура

---

## Что получится

M141 S50 → target = 50 → iHeater-link включает  
M141 S0  → target = 0  → iHeater-link выключает  

---

# 1. Настройка Klipper

Добавить:

[include virtual_chamber.cfg]

---

## virtual_chamber.cfg
```
[gcode_macro VIRTUAL_CHAMBER]
variable_target: 0
variable_temperature: -1
variable_has_sensor: 0
gcode:

[gcode_macro M141]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}

[gcode_macro M191]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}

[gcode_macro CLEAR_VIRTUAL_CHAMBER]
gcode:
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE=0
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=temperature VALUE=-1
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=has_sensor VALUE=0
```
---

# 2. Опционально: термистор камеры

Найди в printer.cfg строку:
```
[temperature_sensor XXX] или [heater_generic XXX]
```
И подставь сюда:
```
printer["XXX"].temperature
```
---
```
#[delayed_gcode UPDATE_VIRTUAL_CHAMBER_TEMP]
#initial_duration: 1.0
#gcode:
#  {% set t = printer["heater_generic chamber"].temperature|float %}
#  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=temperature VALUE={t}
#  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=has_sensor VALUE=1
#  UPDATE_DELAYED_GCODE ID=UPDATE_VIRTUAL_CHAMBER_TEMP DURATION=2.0
```
---

# 3. Проверка
```
M141 S50
```
```
M141 S0
```

---

# 4. WebSocket
```
{
  "jsonrpc": "2.0",
  "method": "printer.objects.subscribe",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target","temperature","has_sensor"]
    }
  },
  "id": 1
}
```
---

# 5. Логика iHeater-link
```
target > 0 → ON  
target == 0 → OFF
```

---

# 6. Настройки слайсера

## Откуда берутся M141 / M191

Слайсеры не знают о `VIRTUAL_CHAMBER` — они выводят стандартные G-code команды для температуры камеры:

- **M141 S{T}** — установить температуру камеры (не ждёт прогрева)
- **M191 S{T}** — установить температуру камеры и ждать прогрева

Поскольку в Klipper определены макросы `M141` и `M191`, они перехватывают эти команды и записывают значение в `VIRTUAL_CHAMBER`.

## OrcaSlicer / BambuStudio

Температура камеры задаётся в настройках филамента:

1. **Filament Settings → Temperatures → Chamber temperature**
2. Установите нужную температуру для каждого типа филамента, например:
   - ABS / ASA: 40–50 °C
   - PLA: 0 (камера не нужна)

Слайсер автоматически добавит `M141 S{T}` в начало G-code при старте печати.

Чтобы убедиться — откройте предпросмотр G-code, в начале файла должна быть строка:

```
M141 S45
```

## PrusaSlicer / SuperSlicer

Температура камеры задаётся в **Filament Settings → Temperatures → Chamber**.

Если поле отсутствует — добавьте команду вручную в **Printer Settings → Custom G-code → Start G-code**:

```
M141 S45 ; chamber temperature for this filament
```

## Start G-code (универсально)

Если слайсер не поддерживает chamber temperature нативно, добавьте в Start G-code:

```
M141 S[chamber_temperature]   ; OrcaSlicer / BambuStudio
M141 S{chamber_temperature}   ; PrusaSlicer / SuperSlicer
```

Подставьте переменную слайсера или конкретное значение.

## End G-code — обязательно

По окончании печати нагрев **не выключится автоматически** — target сбрасывается только явной командой.

Добавьте в **End G-code** слайсера:

```
CLEAR_VIRTUAL_CHAMBER   ; сброс target → iHeater-link выключит нагрев
```

Или эквивалентно:

```
M141 S0
```