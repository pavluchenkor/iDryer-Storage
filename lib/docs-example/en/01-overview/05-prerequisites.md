# What to Know Before Reading: Mini Tutorial

This page is a quick warm-up on C/C++ and embedded concepts that are used later in the documentation without explanation. If you have already used everything on the list confidently, you can skip it. If even one heading makes you think "not sure", read it.

!!! note "Who needs this"
    Before [../03-uart/](../03-uart/) (the binary protocol). All concepts below appear there at once, and without the basics it is easy to get lost.

---

## 1. Fixed-size types

In ordinary Arduino code, you write `int x;`. On ESP32, `int` is 32 bits (4 bytes); on a classic Arduino Uno, it is 16 bits (2 bytes). For a **binary protocol, that is a disaster**: the field size must be identical on both sides.

That is why the code uses explicit types from `<stdint.h>` everywhere:

| Type | Size | Range |
|-----|--------|----------|
| `uint8_t` | 1 byte | 0 ... 255 |
| `int8_t` | 1 byte | -128 ... 127 |
| `uint16_t` | 2 bytes | 0 ... 65 535 |
| `int16_t` | 2 bytes | -32 768 ... 32 767 |
| `uint32_t` | 4 bytes | 0 ... 4 294 967 295 |
| `int32_t` | 4 bytes | -2 147 483 648 ... 2 147 483 647 |

The rule is simple: **use only these types in UART frames**, never `int` / `short` / `long` without `_t`. The size must be unambiguous.

---

## 2. Bitwise operations `<<`, `>>`, `&`, `|`

Four operators on one number. Not scary, but easy to mix up:

### Shifts

`x << n` - shift **left** by `n` bits. In practice, this is multiplication by `2^n`.
`x >> n` - shift **right**. Division by `2^n`.

```cpp
uint8_t a = 0b00000001;   // = 1
uint8_t b = a << 3;       // 0b00001000 = 8
uint8_t c = b >> 2;       // 0b00000010 = 2
```

### Masks - `&` (AND) and `|` (OR)

`a & b` - a bit is 1 only if **both** bits are 1.
`a | b` - a bit is 1 if **at least one** bit is 1.

Examples from the real library:

```cpp
// Pack firmware version 1.2.3 into one 32-bit number:
uint32_t firmware = (1u << 16) | (2u << 8) | 3u;
// Result (HEX): 0x00010203

// Extract MINOR from that number:
uint8_t minor = (firmware >> 8) & 0xFF;
//   >> 8   - moved the minor version into the low byte
//   & 0xFF - trimmed the rest (mask "keep low byte only")
```

### Checking an individual bit

```cpp
uint16_t capabilities = 0x001B;   // 0b00000000_00011011
bool hasHeater = (capabilities & (1 << 0)) != 0;  // check bit 0
bool hasFan    = (capabilities & (1 << 1)) != 0;  // check bit 1
```

`(1 << 0)` = 0b00000001, `(1 << 1)` = 0b00000010, and so on - this is a "single-bit mask".

---

## 3. Little-endian (byte order)

A multi-byte number in memory can be stored in two ways:

- **Big-endian**: most significant byte first (the way we write numbers, left to right).
- **Little-endian**: **least significant** byte first.

ESP32 and most modern MCUs are **little-endian**. The entire iDryer UART protocol is little-endian.

### In plain terms

The number `uint16_t x = 0x1234` in memory:

```
big-endian (not our case):    0x12  0x34
little-endian (our case):     0x34  0x12
                              ^^^^
                              low byte
```

The number `uint32_t x = 0x12345678`:

```
little-endian:   0x78  0x56  0x34  0x12
                  LSB                MSB
```

### Practice

You send `uint16_t temperatureC10 = 553` (55.3 C x 10):

- `553` in hex = `0x0229`.
- The wire carries `0x29`, then `0x02`.

To reconstruct it from received bytes:

```cpp
uint16_t t = buf[0] | (buf[1] << 8);   // 0x29 | (0x02 << 8) = 0x0229 = 553
```

Or use `memcpy`, because ESP32 itself is little-endian:

```cpp
uint16_t t;
memcpy(&t, buf, sizeof(t));   // copy 2 bytes, get the correct value
```

### Why numbers are x 10

When you see `temperatureC10`, `humidityPct10`, or `weightGramsC10` in the protocol, that is **fixed-point**:

- `float` takes 4 bytes and can be inconsistent across platforms.
- Instead, store an integer multiplied by 10.
- `553` instead of `55.3`, sent as `int16` - 2 bytes, deterministic.
- On receive: `float temp = raw / 10.0f;`.

---

## 4. `#pragma pack(1)` - packed structs

By default, the compiler **inserts extra bytes** between struct fields so values land on "correct" addresses (alignment). That speeds up access, but **breaks** a binary protocol - the struct becomes larger than the sum of its fields, and the size differs across platforms.

Example:

```cpp
struct Wrong {
    uint8_t  a;        // 1 byte
    uint32_t b;        // 4 bytes
};
// sizeof(Wrong) == 8 (three padding bytes between a and b!)
```

To **disable** padding:

```cpp
#pragma pack(push, 1)
struct Right {
    uint8_t  a;        // 1 byte
    uint32_t b;        // 4 bytes
};
#pragma pack(pop)
// sizeof(Right) == 5 (exactly the sum of the fields)
```

All payload structs in `idryer-protocol` are wrapped in `#pragma pack(1)`, which is why `sizeof(HelloPayload) == 86`, not 88 or 92. Without this, a device on one platform and a device on another would **interpret frame length differently**.

---

## 5. `enum class`

An ordinary C-style enum is just named numbers:

```cpp
enum MessageKind { Hello = 1, Telemetry = 10 };
int x = Hello;   // implicitly converts to int - dangerous
```

`enum class` (C++11+) is a **typed** enum:

```cpp
enum class MessageKind : uint8_t { Hello = 0x01, Telemetry = 0x10 };

MessageKind k = MessageKind::Hello;           // access through ::
// int x = k;                                  // COMPILATION ERROR!
uint8_t raw = static_cast<uint8_t>(k);         // explicit cast required
```

Benefits:

- Type safety: different enums do not mix.
- Explicit base type (`: uint8_t` means exactly 1 byte, which matters for the protocol).
- Access through `::` makes code easier to read.

In the library code, `enum class` with `: uint8_t` (or `: uint16_t`) is used **everywhere**. That is not an accident - it is part of the binary contract.

---

## 6. C strings and zero padding

In C/C++, a string is a `char` array that ends with a `\0` byte (zero).

```cpp
char s[8] = "Hello";   // 5 characters + '\0' at position 5 + 2 more bytes at positions 6,7
// In memory: 'H' 'e' 'l' 'l' 'o' '\0' '\0' '\0'
```

In the iDryer protocol, string fields have a **fixed size** - 8 / 17 / 32 / 100 bytes. The contract is:

- The actual string plus `\0`, and the rest are zeros up to the end of the field.
- On receive: read until the first `\0`.

This matters when a field carries, for example, a 5-character name in a 32-byte slot - the other 27 bytes are **not garbage**, but exactly zeros.

---

## 7. `const T&` references

In the library code, you will see callbacks like this:

```cpp
void onHello(const HelloPayload& p, const FrameHeader& h) { ... }
```

`const HelloPayload& p` is a **reference to const**. Two things:

- **Reference** (`&`) - not a copy. The function gets the same object as the caller. It is cheap (86 bytes are not copied).
- **`const`** - you cannot change it through this reference, only read it.

In your function, work with `p` like a normal object: `p.firmwareVersion`, `p.unitsCount`. No `p->` - it is a reference, not a pointer.

---

## 8. Callbacks - why pass a function into a function

In Arduino, you are used to this: "call `Serial.println` and it prints." That is a **direct** call.

In an event-driven library (and `idryer-protocol` is one): "register a function -> I will call it when the event happens." That is a **callback**.

```cpp
// 1. Define your own function with the right signature:
void myOnTelemetry(const TelemetryPayload& p, const FrameHeader& h) {
    Serial.printf("T=%.1f\n", p.units[0].temperatureC10 / 10.0f);
}

// 2. Register it in the library:
uartBridge.setTelemetryHandler(myOnTelemetry);

// 3. Done. Now every time a Telemetry frame arrives,
//    the library will call your function by itself. You do not call myOnTelemetry explicitly.
```

You can also pass a lambda (a C++11 anonymous function with captured context):

```cpp
uartBridge.setTelemetryHandler([this](const TelemetryPayload& p, const FrameHeader& h) {
    this->handleIt(p);   // you can use 'this' from the enclosing class
});
```

Typically, all `setXxxHandler(...)` methods accept `std::function<void(...)>` - a generic "something callable with this signature" type. Your main job is to match the **signature** (the parameter list).

---

## 9. Namespaces

In the library, types live in namespaces:

- `DryerUart::` - UART types (`HelloPayload`, `FrameHeader`, `MessageKind`).
- `idryer::` - cloud types (`MqttClient`).
- `idryer::cloud::` - cloud containers (`CommandHandler`, `LinkIntegrationsManager`).
- `idryer::hal::` - HAL wrappers (`ArduinoSerial`).

Two ways to refer to them:

### Full path (verbose, but explicit)

```cpp
DryerUart::HelloPayload req{};
req.role = DryerUart::Role::Rp2040Controller;
```

### Using `using namespace`

```cpp
using namespace DryerUart;   // usually at the top of a .cpp file

HelloPayload req{};          // now without DryerUart::
req.role = Role::Rp2040Controller;
```

This does **not "copy" code**; it simply makes the compiler start looking for names in that namespace.

!!! warning "Not in .h files"
    `using namespace` in header files is bad practice (it pollutes the namespace for everyone who includes the header). In `.cpp` files, it is safe and convenient.

---

## 10. HAL, header-only, and other common terms

Several single-word terms show up often:

- **HAL** (Hardware Abstraction Layer) - a wrapper over platform-specific APIs. In this library, `ArduinoSerial` is the HAL implementation of the `ISerial` interface for Arduino-core. The point is that the library core does not know about "Arduino" or "pure ESP-IDF" - it works through an interface.

- **header-only** - a library/module made only of `.h` files, with no separate `.cpp` file to compile. Include it, and that is all - no linking step is needed. The UART structures in `uart_protocol.h` work this way.

- **NVS** (Non-Volatile Storage) - non-volatile ESP32 memory. An EEPROM equivalent. Used through `Preferences.h`. It stores `deviceToken`, integration settings, and anything that must survive a reboot.

- **TLS** (Transport Layer Security) - encryption over TCP. In code, this appears as `WiFiClientSecure`. To connect to the correct server, you need to provide the root CA certificate (`setCACert(ROOT_CA)`) so the server can be verified. `setInsecure()` disables verification (only for LAN devices such as Bambu devices with a self-signed certificate).

- **JWT** (JSON Web Token) - a signed token for HTTP authorization. The device does **not** need it (it has its own `deviceToken`), but the user app uses JWT to call `POST /devices/claim`.

- **mDNS** - "multicast DNS", automatic discovery of devices on the local network by names such as `myhost.local`. Used to find Home Assistant (`homeassistant.local`) and Klipper.

- **WebSocket** - a bidirectional persistent channel over TCP (RFC 6455). Unlike HTTP, it is not request/response, but an open pipe. Used in Moonraker to subscribe to Klipper events.

- **JSON-RPC** - a format for remote calls via JSON messages: the client sends `{method, params, id}`, and the server replies with `{result or error, id}`. Over WebSocket, that is exactly the Moonraker API.

---

## What next

If any section above still does not click, **do not move on yet** - look up examples for that exact topic. You will meet it 10 more times later.

If everything is clear, the next step is:

- [../02-getting-started/01-choose-your-path.md](../02-getting-started/01-choose-your-path.md) - choose your path.
- [../03-uart/](../03-uart/) - the UART protocol as a whole. All concepts above are used there at once.

← [Section 01-overview contents](../README.md)
