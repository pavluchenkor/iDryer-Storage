# Changelog

Notable changes to the `iDryer-Storage` firmware — the filament storage cabinet. Russian version: [CHANGELOG.ru.md](CHANGELOG.ru.md).

## Types of changes

- **Added** — for new features.
- **Changed** — for changes in existing functionality.
- **Deprecated** — for soon-to-be removed features.
- **Removed** — for now removed features.
- **Fixed** — for any bug fixes.
- **Security** — in case of vulnerabilities.

## [3.0.0] — 2026-09-23

### Added

- **Firmware updates over the air.** The cabinet updates itself from the portal.
- **Portal binding without a PIN.** The device receives its key on connection and is addressed in the cloud by its own identifier.
- **Unbinding from the portal.** A portal command wipes the key and the device returns to the pairing state.
- **Local network control.** The app talks to the cabinet directly on the same network, without the cloud.
- **Card actions in the portal and in the app.** The cabinet declares what it can do, and the interface is drawn from that declaration: light on with a chosen effect and colour, light off.
- **Home Assistant from the card manifest.** The cabinet appears in Home Assistant with its readings and controls; entities are built from the same declaration.
- **LED strip animations.** Ten effects, selectable on the device and from the card: solid, breathe, wave, rainbow, twinkle, colour cycle, swell, ripple, spotlight and two tones.
- **Flash layout without a filesystem.** Each firmware partition is 1984 KB instead of 1280 KB: the firmware does not use a filesystem.

### Changed

- The firmware moved to the shared `idryer-core` library: portal link, binding and protocol are the same across the ecosystem.
- **Only Home Assistant is compiled in.** The cabinet does not talk to printers, so Bambu Lab and Moonraker are left out of the image — 36 KB smaller.
- **The device declares its integrations.** The portal and the app draw only what the cabinet actually has.
- Telemetry and status periods come from the contract.

### Removed

- Five noise-based animations — aurora, candle, ocean, lava and forest. They cost 3.7 KB of flash, which the cabinet needed elsewhere; the code is kept, commented out, next to the remaining effects.

## [2.x]

A series of trial builds. The direction was closed, the work carried over into 3.0.0.
