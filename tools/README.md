# Инструменты разработчика iDryer Link

## tools/mock_portal.py

Мок backend-портала для тестирования cloud-флоу без реального сервера.

**Запуск:**
```bash
pip install flask python-socketio eventlet
python3 tools/mock_portal.py
```

Сервер слушает `http://0.0.0.0:5050`.

**Настройка прошивки:**
```ini
build_flags =
  -DIDRYER_API_BASE="http://<host>:5050/api"
  -DMQTT_USE_TLS=0
```

**API:**
- `POST /api/devices/provision` — токен `mock-token-{serial}`
- `POST /api/devices/register` — PIN `12345678`
- `GET  /api/devices/check-claim/{token}` — автоматически подтверждает при первом вызове

---

## extra_scripts/copy_firmware.py

Post-build скрипт. Копирует артефакты сборки:

- `firmware.bin`, `bootloader.bin`, `partitions.bin`, `boot_app0.bin`
- локально: `firmware/<board>/`
- в flasher-portal: `/Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link/<slot>/<board>/`

Slot (`prod` / `stage`) определяется по имени окружения.

---

## extra_scripts/stage_auto_claim.py

Post-upload скрипт для staging. После прошивки читает Serial, ждёт `CLAIM_PIN:<pin>:<expires>` и автоматически заявляет устройство через staging API.

Подробнее: [STAGING.md](../STAGING.md).
