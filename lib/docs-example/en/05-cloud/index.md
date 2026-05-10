# Cloud / Portal HTTP API

Three documents: an overview of the device claiming process, the exact REST API contract for the portal HTTP endpoints, and the `ICommandSink` interface as an extension point for command application, especially for devices without UART.

| Document | Content |
|----------|---------|
| [01-claiming-overview.md](01-claiming-overview.md) | What claiming is, why the PIN exists, Link states in the database, and special cases (recovery, expired PIN) |
| [02-http-api.md](02-http-api.md) | `POST /devices/provision`, `/register`, `/claim`, `GET /devices/check-claim/:token`; with `curl` examples |
| [03-command-sink.md](03-command-sink.md) | The `ICommandSink` interface for devices without UART (`UartCommandSink` vs a custom implementation) |

## Navigation

- Want to understand **why** claiming exists and who participates in it → [01-claiming-overview.md](01-claiming-overview.md).
- Implementing an HTTP client in your firmware → [02-http-api.md](02-http-api.md).
- Building a standalone device without UART → [03-command-sink.md](03-command-sink.md).

## Related

- [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md) — the same claim flow, but with UART frames (MCU + LINK).
- [../02-getting-started/05-dev-claim.md](../02-getting-started/05-dev-claim.md) — three practical ways to perform claiming (screen / flasher portal / Serial).
