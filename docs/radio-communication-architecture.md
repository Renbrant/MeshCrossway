# Radio communication architecture, APIs and interoperability

> **SUPERSEDED (2026-10-28):** The UART-integrated architecture below is historical bench reference, not the approved product. Current design: [three independently powered BLE-connected detachable modules](modular-architecture.md).

**Status:** source/design review, 2026-10-26. No PCB, firmware build, UART link, RF capture or field test has yet validated the implementation. This file defines expected interfaces and explicit acceptance gates rather than proven product capabilities.

[Project overview](../README.md) · [Hardware & wiring](hardware-wiring.md) · [Radio firmware strategy](radio-firmware-strategy.md) · [Navigation & tracking](navigation-and-sharing.md)

## Illustrated communication layers

![Radio communication stack](images/architecture/communication-stack.svg)

The protocols use separate APIs even though both links are UART. The illustration is explanatory and does not assert native interoperability.

## 1. Chosen topology

- **Central:** Waveshare ESP32-S3-Touch-LCD-3.5 runs LVGL, application model, radio adapters, GNSS, storage and location/airtime policy.
- **Radio A:** Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit, **Meshtastic**, UART1 and binary Protobuf API.
- **Radio B:** second Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit, **MeshCore Companion**, UART2 and Companion binary API.
- **GNSS:** u-blox MAX-M10S module on host-side I2C. microSD resides on Waveshare.
- No transparent Meshtastic↔MeshCore RF bridge, shared contact identity, shared encryption system or interchangeable mesh routing is presumed.

Each radio operates and recovers independently; the UI remains operational if a radio is unavailable.

## 2. Pin-level UART contract (proposal, not approved for production)

| Host Waveshare | Direction | Radio | Radio pin | Protocol |
|---|---|---|---|---|
| GPIO39 / UART1 TX | host → A | Meshtastic | D7 / GPIO44 RX | PROTO |
| GPIO40 / UART1 RX | A → host | Meshtastic | D6 / GPIO43 TX | PROTO |
| GPIO17 / UART2 TX | host → B | MeshCore | D7 / GPIO44 RX | Companion |
| GPIO18 / UART2 RX | B → host | MeshCore | D6 / GPIO43 TX | Companion |
| GND | common reference | A and B | GND | logic ground |

Use 3.3 V **logic levels** on the UART; never connect TTL UART directly to RS-232 or 5 V serial. For initial protocol bring-up, power each XIAO independently over USB-C and share signal ground only. Rev A's normal carrier power input for each XIAO is **pin 14 / VBUS / 5V** through the selectable, Schottky-isolated power path described in [power tests](power-architecture.md). Do not combine USB-C and carrier VBUS until backfeed behavior is verified. Prevent unpowered-radio backfeeding through TX/RX by proven-safe sequencing or hardware isolation if measurements require it.

These host GPIOs share optional camera functions: disable the camera and confirm there are no board-revision or peripheral conflicts. Verify actual shipped Seeed XIAO ESP32-S3 and Wio-SX1262 B2B kit pins against manufacturer schematic and continuity, not seller imagery. Keep UART0 debug and native USB separate. Allow short traces, test pads, optional 22–100 Ω series-resistor footprints, individual reset controls and preferably independent switched power for recovery. Neither series resistors nor bulk capacitors replace correct voltage, regulator headroom or isolation.

**Serial baseline:** 115200 baud, 8N1, no hardware flow control, explicitly configured and read back on both sides. Do not assume 115200 is the default Meshtastic serial baud; check upstream defaults and build/release behavior.

## 3. Meshtastic serial integration

Use official Meshtastic firmware, **Serial module enabled and mode PROTO**, with module RXD=44 and TXD=43 (subject to target-board support). The controller implements the serial framing:

`0x94 0xC3 | length_hi | length_lo | serialized protobuf`

The binary application API transports **ToRadio** and **FromRadio** Protocol Buffer messages. Use the exact .proto definitions corresponding to the pinned firmware, generate C/C++ bindings (such as nanopb where suitable), and test parsing against known frames. Do not treat serial PROTO as plaintext CLI or depend on human-readable debug logs on this UART.

Supported *classes* of operations include send/receive messages, node information, channels, configuration, position, telemetry and administrative responses; each command and field must be verified on the selected version. Treat TX queue acceptance, radio transmission, protocol ACK and recipient reading as different states. Never report a completed remote delivery based only on local serial success.

**Tracking blocker to verify:** whether externally originated position packets can specify an *individual outgoing packet* hop limit of zero and whether the firmware sends the intended packet without rebroadcast; avoid modifying global radio behavior merely to implement one tracking mode. Packet capture and a third repeater node are required.

Sources: [Serial module configuration](https://meshtastic.org/docs/configuration/module/serial/), [official Protobuf definitions](https://github.com/meshtastic/protobufs), [firmware](https://github.com/meshtastic/firmware), [Meshtastic Arduino client](https://github.com/meshtastic/meshtastic-arduino).

## 4. MeshCore Companion integration

Run **MeshCore Companion**, not a repeater KISS/RS-232 transport. Companion exposes a separate command/response/event binary API with start/query, contacts, message synchronization, send private/channel text, advertising coordinates, statistics and trace/path diagnostics. Use the exact upstream Companion wire specification and its version/capability response, including framing, length encoding, sequence/timeout behavior, command/event codes, payload size and error codes. Do not use Meshtastic Protobuf on this link.

**UART availability:** current upstream MeshCore includes a dedicated `Xiao_S3_WIO_companion_radio_serial` build environment with `SERIAL_TX=D6` and `SERIAL_RX=D7`, which map to GPIO43/GPIO44 on XIAO ESP32-S3. This confirms the source-level serial target we need. The physical UART link, boot logging, framing under load and persistent configuration still require bench validation.

**Direct-only blocker to verify:** explicit zero-hop send without stored path reuse, relay forwarding, or flooding fallback. Contact-directed delivery by itself is not proof of zero-hop. Native group/channel semantics may involve flooding; direct-only groups should remain unsupported or experimental until proven by RF captures. MeshCore custom payloads may be unintelligible to generic upstream clients.

Sources: [MeshCore firmware](https://github.com/meshcore-dev/MeshCore), [Companion code](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/main.cpp), [Companion protocol wiki](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol), [meshcore_py](https://github.com/meshcore-dev/meshcore_py), [meshcore.js](https://github.com/meshcore-dev/meshcore.js).

## 5. MeshCrossway host-side software contract

Use native ESP-IDF/FreeRTOS services with bounded queues, dedicated receive loops, reconnect state machines and a responsive nonblocking LVGL task. Conceptual API (illustrative, **not an existing implemented C++ interface**):

```cpp
radio.connect();
radio.getCapabilities();
radio.getContacts();
radio.getChannels();
radio.sendMessage(message);
radio.sendPosition(position, routingPolicy);
radio.getDiagnostics();
radio.setConfiguration(config);
```

Each operation is implemented by a radio-specific adapter, and its result includes support/unsupported, pending, timeout, rejected, sent or acknowledgement information only when substantiated. **Capability discovery** must be based on real firmware identification and supported operations rather than guesses from radio type. Hide/disable UI actions unsupported by the installed version.

Model identifiers with a network discriminator and native ID (Meshtastic node ID, MeshCore public key/contact identifier). Store network, sender, recipient/channel, timestamp, native message ID, payload type and delivery state. Never collapse different networks' contacts based on matching labels.

Separate radio TX scheduling and host queues, prioritize human messages, cap tracking airtime and avoid stale location retransmission. For command/response transports, serialize outstanding requests as required while independently receiving unsolicited events. Parser safety requirements: length limits, partial frames, resynchronization, unknown command fields, timeouts, failed CRC/integrity if applicable, firmware reboot, duplicate events and SD backpressure.

## 6. UI feature and protocol compatibility matrix

Legend: **native API** = a likely suitable upstream function family exists; **adapter** = needs local translation/state/policy; **unproven** = critical behavior requires a version-specific test; **not native** = not universally interoperable.

| Feature | Meshtastic | MeshCore | Acceptance condition |
|---|---|---|---|
| Messages / inbound notifications | Native API | Native Companion API | bidirectional, duplicate-free, restart recovery |
| Private contacts | NodeDB / IDs | public-key contacts | native identities preserved |
| Groups/channels | native channel model | native channel model | do not claim equal privacy/routing semantics |
| Settings and diagnostics | config/telemetry APIs | Companion commands/events | feature-version checks and supported fields |
| Own GNSS position | position packet adapter | advertised coordinates / adapter | freshness and privacy |
| Team location, waypoints and POIs | native positions or custom payload | announcements or custom payload | interoperability labeled and measured |
| Time/distance-triggered tracking | host scheduler | host scheduler | per-radio airtime budget and consent |
| Strict zero-hop direct tracking | unproven per-packet hop limit | unproven path/fallback controls | over-the-air capture with repeater |
| Group direct tracking | unproven | unproven | no accidental flooding |
| Bulk GPX/map transfer | not suitable over LoRa | not suitable over LoRa | optional authenticated local BLE/Wi-Fi in later phase |
| Firmware flashing via UI | not implemented | not implemented | separate secure design required |

A shared UI does **not** promise feature-for-feature equivalence. Prefer upstream-native position encoding for interoperability with ordinary apps; label MeshCrossway-specific payloads as proprietary *application formatting* riding on compatible radio transports, not as a new universally readable message type.

## 7. RF coexistence and airtime

Both SX1262 transmitters can operate in the 915 MHz ISM region, potentially close together. A transmitting board can desensitize the other receiver, including through antenna coupling, regardless of UART independence. Assess antenna placement, RF isolation, transmit power, channel overlap, blocking, simultaneous transmission, matching and regional rules. Never power an RF transmitter without a suitable antenna/load.

A central TX arbiter can coordinate **host-originated** sends but cannot guarantee no overlap with automatic radio announcements, retries, acknowledgements or repeat traffic controlled by separate upstream firmware. A full mutual exclusion guarantee may require additional firmware observability/controls; do not claim one before measuring and proving it.

Track airtime and congestion separately for Meshtastic and MeshCore and reduce/skip live positions before starving human traffic. One-hop does not create a dedicated RF channel or eliminate collisions.

## 8. Security, resilience, upgrades

- Maintain independent native encryption, identity and network permissions; **no automatic cross-network forwarding**.
- Store sensitive logs/positions with an explicit retention and local protection policy. Never write private keys or raw location history to public issue reports.
- Recover one radio without rebooting UI or the other radio. Reserve individual reset and power-gating provisions.
- Handle UART unplug/garbage, radio reboot during a command, protocol revision changes and power loss while writing to SD.
- Track upstream repository, commit, tagged release, build environment, flags, firmware binary hash, PCB version and protocol field/command version in a reproducible manifest.

## 9. Firmware change decision gate

1. **Preferred:** unmodified upstream firmware and supported configuration.
2. **Next:** compile upstream source using board-specific pin, UART or feature flags; no protocol fork.
3. **Only after a reproducible blocker:** minimal isolated source patch, ideally proposed upstream, accompanied by compatibility tests and documented maintenance obligations.

Potential blockers: unsupported externally-controlled zero-hop routing, missing UART Companion mode for actual board, insufficient stats/capability exposure, RF transmission scheduling. These are **hypotheses**, not justification to fork today. Never rewrite Meshtastic/MeshCore radio stacks solely to implement the touchscreen.

## 10. Bench verification gates

1. Record PCB revisions, confirm continuity of host and radio pins, ensure no camera/SPI/I2C overlap.
2. Validate rail current, brownout, UART backpower and USB/shared-power sequencing; provide isolated boot/reset for each radio.
3. Capture framed Meshtastic PROTO traffic (startup, message TX/RX, telemetry/config, error and radio reboot) over UART1.
4. Compile/freeze MeshCore Companion external-UART build and capture commands/events over UART2.
5. Exercise simultaneous bidirectional messaging, burst input, packet loss, malformed data, interleaved notifications, independent reconnect and version mismatch.
6. Verify position encoding/decoding and third-party interoperability on both networks.
7. Verify actual zero-hop with a transmitter, intended recipient and separately placed repeater/sniffer; inspect for unexpected retry/fallback and group flooding.
8. Measure inter-radio blocking/collision and latency with both boards active and compare against each operating alone.
9. Load-test LVGL, microSD, I2C GNSS, both UARTs, tracking and optional BLE; check heap, DMA, frame rate and dropped packets.
10. Prove consent expiry, stale-fix suppression, reboot recovery and no silent direct-to-mesh fallback.

Do not advertise full feature coverage or emergency reliability before these gates pass.

## 11. Follow-on tasks

- Implement Meshtastic PROTO frame decoder, encoder and version-locked protobuf bindings.
- Implement MeshCore Companion frame decoder, encoder and version-locked command/event mapping.
- Add a host-side capability registry and self-test diagnostic screen.
- Add serial capture/replay fixtures and wire-protocol regression tests.
- Verify the selected Rev A XIAO D7/GPIO44 RX and D6/GPIO43 TX external UART pinout on actual hardware and capture startup/traffic behavior.
- Design RF coexistence tests and individual radio power/reset control before PCB freeze.

## Selected XIAO/Wio external UART interface

For both XIAO kits, Rev A uses **external D7/GPIO44 RX and D6/GPIO43 TX** as the API-UART pins: Waveshare J8-9/GPIO39 TX → A RX44, A TX43 → J8-11/GPIO40 RX; J8-16/GPIO17 TX → B RX44, B TX43 → J8-18/GPIO18 RX. Common GND, target 115200 8N1. **The Wio SX1262 SPI/GPIO on the internal B2B connector must not be wired to the Waveshare.** Confirm Meshtastic serial API binding and no debug contamination; MeshCore upstream already provides the serial Companion target. Radio power is separately selected at XIAO VBUS/5V and remains a bench-validation gate. See [authoritative wiring](hardware-wiring.md) and [official hardware sources](hardware-sources.md).
