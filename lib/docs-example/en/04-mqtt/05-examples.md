# MQTT: Debugging examples (mosquitto_sub/pub)

Ready-to-use terminal commands: inspect what your device publishes and send commands manually.

!!! note "Needed"
    `mosquitto_sub` / `mosquitto_pub` are installed from the `mosquitto-clients` package:
    ```bash
    # macOS:    brew install mosquitto
    # Debian:   sudo apt install mosquitto-clients
    ```

In the examples below, substitute:

- `BROKER_HOST` - the MQTT broker address (depends on the environment).
- `SERIAL` - your `serialNumber` (device Client ID).
- `TOKEN` - the device `deviceToken`.

For the public production deployment on `portal.idryer.org`, request the values from the infrastructure owner - this data is not stored in the library repository.

---

## See everything the device publishes

```bash
mosquitto_sub \
  -h BROKER_HOST -p 8883 --cafile /path/to/isrg_root_x1.pem \
  -u SERIAL -P TOKEN \
  -t "idryer/SERIAL/#" -v
```

Flags:

- `-v` - prints the topic before the payload.
- `--cafile` - root certificate (the same Let's Encrypt root that is embedded in `src/mqtt/root_ca.h`).
- Omitting `--cafile` and switching to `-p 1883` is for local development without TLS.

Expected output (with a live device):

```
idryer/SERIAL/info {"hardwareVersion":"v1.0","firmwareVersion":"1.2.3",...}
idryer/SERIAL/status {"units":[{"unitId":"U1","mode":"IDLE"}],"uptime":32}
idryer/SERIAL/telemetry {"units":[{"unitId":"U1","temperature":25.3,"humidity":42.1,...}]}
...
```

---

## See telemetry only

```bash
mosquitto_sub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -t "idryer/SERIAL/telemetry" -v
```

---

## Send a command: start drying

```bash
mosquitto_pub \
  -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN \
  -q 1 \
  -t "idryer/SERIAL/commands/drying" \
  -m '{"unitId":"U1","params":{"temperature":55,"duration":240}}'
```

Expected reaction:

- In the `idryer/SERIAL/status` subscription, a new message with `"mode":"DRYING"` appears after about 1 second.
- In the `idryer/SERIAL/telemetry` subscription, `heaterPower` starts to rise.

---

## Stop drying

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/stop" \
  -m '{"unitId":"U1"}'
```

One second later, `status` shows `"mode":"IDLE"`.

---

## Start a profile

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

`status` switches to `"mode":"PROFILE"`, and the `currentStage`, `stagePhase`, `stageElapsed`, and `stageRemaining` fields appear.

---

## Request the full menu config

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/get_config" \
  -m '{}'
```

After a few seconds, the full menu JSON arrives in `idryer/SERIAL/config`.

---

## Change one value in the menu

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/set" \
  -m '{"id":3,"unit":0,"val":55}'
```

The device applies the change and publishes `config/delta` with the new value confirmation.

---

## Ping

Verify that the device is online and responding to commands, without side effects:

```bash
mosquitto_pub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -q 1 \
  -t "idryer/SERIAL/commands/ping" \
  -m '{}'
```

The device log (Serial Monitor) shows that the command was received. There is no MQTT reply - wait for telemetry or subscribe to `idryer/SERIAL/events`.

---

## Offline monitoring

To know in advance when the device drops off via LWT:

```bash
mosquitto_sub -h BROKER_HOST -p 8883 --cafile ca.pem \
  -u SERIAL -P TOKEN -t "idryer/SERIAL/offline" -v
```

A `{}` message in this topic means an unexpected TCP disconnect. A normal `disconnect()` does not publish the LWT.

---

## TLS debugging

If the client does not connect:

- Verify that you are using the correct CA. It is in the repo - `src/mqtt/root_ca.h` (PEM format, convenient to save to a file).
- The client and device clocks must be correct, otherwise the TLS certificate may appear "expired".
- On Linux: `openssl s_client -connect BROKER_HOST:8883 -CAfile ca.pem` - shows the certificate chain.

---

## Alternative: MQTT Explorer

For visual debugging (topic tree, retained messages, filters), [MQTT Explorer](https://mqtt-explorer.com/) works well - a desktop GUI client. Configure the connection:

- Host: `BROKER_HOST`
- Port: `8883` + TLS
- Username: `SERIAL`
- Password: `TOKEN`
- Custom CA: path to the Let's Encrypt root

The `idryer/SERIAL/*` tree immediately shows the latest retained messages (info, status, rfid).

---

## Next

- [../05-cloud/](../05-cloud/) - the provisioning and claim HTTP cycle (getting `SERIAL` and `TOKEN`).
- [../06-flows/](../06-flows/) - end-to-end scenarios: first boot, claiming, remote config, profile.
