# Radio firmware strategy and workload separation

**Status: design decision, not bench validation.** The MeshCrossway prototype uses a Waveshare ESP32-S3 touchscreen controller and two independent ESP32-S3/SX1262 XIAO ESP32-S3 + Wio-SX1262 kits. We favor upstream Meshtastic and MeshCore firmware and avoid maintaining forks unless a tested, essential capability cannot be exposed otherwise.

## Illustrated firmware boundaries

![Firmware responsibilities](images/architecture/firmware-responsibilities.svg)

The central host handles UI, storage and application policy; each radio keeps its native protocol stack.

## Ownership and responsibilities

| Component | Owns | Does not own |
|---|---|---|
| Waveshare ESP32-S3 | LVGL/touch, GNSS, navigation, POIs, tracks/GPX, microSD, location-sharing policy/scheduler, BLE companion bulk-transfer service (future), UI state, independent radio adapters | Full Meshtastic/MeshCore radio stacks |
| XIAO/Wio kit A | Meshtastic radio stack, SX1262, network identity, native routing, packet queues/status as supported | Map rendering, GPX, app-level tracking schedule |
| XIAO/Wio kit B | MeshCore Companion radio stack, SX1262, native routing, contact and packet handling as supported | Map rendering, GPX, app-level tracking schedule |

The radio devices are **not general-purpose coprocessors** while running standalone upstream protocol firmware. Giving them unrelated application workloads would require firmware changes and create maintainability risks. A failure in either adapter should not block the other adapter, GNSS, UI or stored history.

## Modification policy — least invasive first

1. **Level 1: upstream releases, configuration only.** Use documented serial/Companion APIs and device settings. Preferred for compatibility and maintainability.
2. **Level 2: custom upstream-source build, without protocol changes.** Adjust the target board, build flags, external UART pin binding, baud rate and enabled interfaces where stock images do not match the physical wiring. Track exact upstream commit, configuration and produced binary checksums. A custom build is **not automatically a fork**.
3. **Level 3: minimal source patch only for a demonstrated blocker.** If protocol APIs cannot implement a critical requirement, first consider a supported alternative or reduced feature. Propose upstream changes before maintaining a local patch. Keep any patch isolated, tested, documented, rebased and clearly license-compliant.

Do not assume that a firmware compiled for USB Companion also exposes the same API on hardware UART pins. Do not assume identical feature availability across board-specific upstream images or releases.

## Protocol integration

### Meshtastic radio

- Start with a compatible **official upstream firmware** build, configured for UART serial API with `PROTO` framing; verify board firmware target, pin routing, UART ownership, voltage levels, serial transport and reconnect behavior.
- Confirm whether *outgoing packets* submitted through the chosen host API can specify `hop_limit = 0` individually while leaving normal user messaging unchanged. Observe actual RF behavior rather than treating an API setting as proof.
- Do not modify the node's global role, default routing or network behavior just to enable Direct Tracking without understanding the effect on other traffic.

### MeshCore radio

- Start with **upstream Companion** firmware. The selected ESP32-S3/SX1262 board may require a **custom upstream-source build** to bind Companion to external hardware UART. Confirm the actual target, `SERIAL_RX`/`SERIAL_TX` build options and electrical wiring on the delivered board.
- Confirm an API-supported **true zero-hop** send path for the proposed Direct Tracking payload and verify no stored route, repeater or fallback flooding is used. Direct addressing alone does **not** imply zero-hop.
- Do not assume MeshCore group/channel transmission can be made zero-hop while preserving normal group semantics; validate or explicitly mark group Direct Only as unsupported.

For both stacks, preserve each network's native identity, encryption and security model. The controller unifies the *interface* but does not automatically bridge the radio networks.

## Main controller task scheduling

FreeRTOS service boundaries (exact core affinity assigned after profiling):

- High-responsiveness LVGL/touch task; never block it on UART, microSD, GNSS or Bluetooth.
- Two independent asynchronous UART adapters with bounded input/output queues, health status and reconnection handling.
- GNSS service with fix freshness, uncertainty and timestamp validation.
- Navigation/POI/GPX local storage with bounded writes and recovery.
- Location scheduler enforcing per-radio airtime budget, minimum gaps, movement filtering, permissions and session expiration.
- Optional future BLE companion for large transfers; it must not starve UI or messages.

The S3's external PSRAM improves available working space but does not eliminate internal RAM/heap, DMA, flash/PSRAM bus or display bandwidth limits. Prefer partial LVGL render buffers, streaming GPX and bounded map-tile caches. Profile under full load before freezing CPU affinity and memory allocation.

## Tests and decision gates

| Gate | Pass condition |
|---|---|
| Base firmware | Exact board-target and official release/source documented; boot and firmware identification verified |
| UART adapters | Stable messaging over two independent UARTs; disconnect/reconnect and restart recovery demonstrated |
| Zero-hop MT | Capture packets and demonstrate no repeater forwarding for tracking with `hop_limit = 0`, with regular messages unaffected |
| Zero-hop MC | Capture packets and show no repeater/path reuse/flooding for tracking; group behavior explicitly validated or disabled |
| Congestion | Compare channel airtime, missed messages and latency with tracking OFF/ON across multiple nodes; location budget suppresses overload |
| Location privacy | User consent, one-person/group audience, auto-stop, manual stop and stale GNSS handling verified |
| Controller capacity | Touch responsiveness, internal heap/PSRAM margin, UI framerate, microSD, UART and BLE coexistence under load |
| Upgrade path | Document pin/flag/build manifest; confirm new upstream releases without losing expected functionality |

No feature should be advertised as implemented or field-safe until hardware tests confirm it. Do not use the prototype as the sole emergency communications device.

## Future candidate: Reticulum

Reticulum is a possible later extension, described in the separate [Reticulum concepts and integration study](reticulum-future-integration.md). The initial design continues to use the dedicated MT and MC radios defined above.

An ordinary RNode supplies a radio interface; a selected host or embedded implementation must also run the Reticulum network and messaging layers. A later integration therefore needs its own API, resource, recovery and interoperability validation. The study starts with an external experiment and does not select a third radio, change the controller workload or commit a release milestone.

Related: [navigation and sharing](navigation-and-sharing.md), [bench wiring](hardware-wiring.md), [UI simulator](https://renbrant.github.io/MeshCrossway/).

## Detailed integration contract and source-review findings

See [Radio communication architecture and API compatibility](radio-communication-architecture.md). In particular:

- Meshtastic's serial `PROTO` mode transports length-framed Protobuf `ToRadio`/`FromRadio` objects. Its physical UART is not a text terminal. Explicitly configure and verify serial baud at both ends; do not assume 115200 is the upstream default.
- MeshCore Companion uses its separate binary command/event protocol. The Companion source has an optional ESP32 hardware-UART transport selected by `SERIAL_RX` and `SERIAL_TX`, making a **build-only customization** a plausible first step; the exact release/target and pin conflicts must be checked before claiming success.
- The UI must use per-radio capability discovery and must not assume Meshtastic and MeshCore share identity, group, path, acknowledgement or security semantics.
- Radio APIs, telemetry, queued messages and incoming notifications must be serviced asynchronously with separate bounded queues, reconnect and version handling. Reports must differentiate queued, accepted, transmitted and remotely acknowledged where supported.
- The central controller can schedule only application-originated transmissions reliably. Autonomous announcements, ACKs and retransmissions by both radios make RF coexistence and simultaneous transmissions an independent bench-validation concern.
- Strict zero-hop tracking and truly equivalent group semantics are **not demonstrated**. Do not ship a direct-only guarantee until over-the-air tests prove it; a firmware patch is a last resort after a reproducible API limitation.

## XIAO migration override (2026-10-26)

The V1 uses two **Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kits**, not Aideepen/Heltec. External XIAO UART candidate: D7/GPIO44 RX, D6/GPIO43 TX; configure firmware explicitly and keep binary transport free from debug output. Wio SPI is internal to the kit. Meshtastic has a matching Seeed image, MeshCore lists Xiao S3 WIO but external Companion UART remains **unverified**. MeshCore initial programming may require a **merged image** for persistent settings. Do not treat the old radio GPIO4/5 external UART or Heltec target as valid. [Full hardware guide](hardware-wiring.md).
