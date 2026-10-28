![MeshCrossway — Meshtastic and MeshCore modular three-device system](docs/images/meshcrossway-modular-hero.jpg)

MeshCrossway is an **open-source, offline-first modular communication system** with **three independent, battery-powered devices**: one touchscreen **Core** and two detachable LoRa **Radio Pods**, one for **Meshtastic** and one for **MeshCore**. The Core connects to both Pods over **two separate Bluetooth Low Energy (BLE) links**, bringing messaging, navigation, GNSS positioning, tracks and field diagnostics into a single interface without requiring cellular coverage or the internet.

> **Development status — October 28, 2026:** the **three-device, independent-battery BLE architecture** is the approved direction. The Waveshare touchscreen firmware **v0.5.2 passed the owner’s display/menu stability test**; **v0.4.0 remains recommended recovery**. The **v0.5.3 touchscreen Wi-Fi setup release is experimental and awaits physical validation** ([firmware status and test steps](docs/firmware-status.md)), but the two-Pod BLE integration and real radio features are **not yet implemented**; the web UI still contains simulated data. Firmware v0.5.0/v0.5.1 has a known display regression and is **not recommended for installation**. The former integrated Rev A carrier/UART/one-battery design is archived as an alternative historical variant, **not the current build plan**. This is not a certified rescue/emergency device.

## Current hardware — three independent BLE devices

![Approved MeshCrossway modular topology: one Core and two autonomous Radio Pods, each with its own battery](docs/images/architecture/modular-ble-topology.svg)

**Three separate units, three batteries, two simultaneous BLE links, and two independent LoRa networks.** The three devices may be carried separately or attached to an optional mechanical dock. **There is no electrical wiring, shared carrier PCB or shared battery between the Core and either Pod** in the current software-first design. BLE remains the Core-to-Pod data link even when the units are docked.

| Device | Selected hardware | Independent power | Role and wireless connection |
|---|---|---|---|
| **MeshCrossway Core** | **Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0** (480 × 320 touchscreen) | **Own protected rechargeable battery**, charging/power management to be validated | Central LVGL UI, microSD, planned MAX-M10S GNSS; **BLE Central** connected to both Pods |
| **MX-M Radio Pod** | **Seeed XIAO ESP32-S3 + Wio-SX1262** B2B kit | **Own protected rechargeable battery**, charging circuit and power switch | Native **Meshtastic** radio with **BLE PhoneAPI Peripheral**; its own SX1262 and LoRa antenna |
| **MX-C Radio Pod** | **Second Seeed XIAO ESP32-S3 + Wio-SX1262** B2B kit | **Own protected rechargeable battery**, charging circuit and power switch | Native **MeshCore BLE Companion Peripheral**; its own SX1262 and LoRa antenna |

**Selected radio kit:** [Seeed Studio XIAO ESP32-S3 & Wio-SX1262](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html), **quantity 2**. The XIAO controls its own SX1262 through the kit's **internal board-to-board (B2B) connection**. The Core talks to each XIAO **wirelessly over BLE**, not via UART, SPI or GPIO wires.

**Two BLE connections, two different APIs:** Meshtastic uses **protobuf PhoneAPI over BLE GATT**; MeshCore uses its **Companion binary protocol over BLE GATT**. These are distinct protocol adapters, not an interchangeable serial stream. Concurrent pairing, reconnection, packet handling and responsiveness remain real-hardware validation tasks. The Pods must retain their native LoRa operation when the Core is off or out of BLE range.

**Three independent power systems:** Each enclosure requires its own battery protection, charging, charge-state/level monitoring and power switching as appropriate. Final battery chemistry/capacity, charger choices, mounting and endurance are **not yet approved**. A future docking charger, if adopted, must charge the three cells through **separate protected charging paths**; battery terminals are never directly paralleled. This modular design also allows a **larger Core battery** without being constrained by a shared carrier PCB.

**GNSS:** The [u-blox MAX-M10S breakout](docs/hardware-sources.md) remains a proposed **Core-side** location/time source; its exact power connection and antenna require validation. Waveshare onboard microSD is the planned local storage. Each LoRa Pod has its **own antenna**, with coexistence testing required even when carried close together.

**Mechanical concept:** The Core and Pods are **detachable**; an optional physical dock can hold them together, but **docked UART and dock charging are future options**, not V1 prerequisites. Firmware and BLE interoperability come **before** new enclosure, charging-board or PCB work.

**Reference:** [Approved modular architecture, BLE roles and test plan](docs/modular-architecture.md). The former [integrated Rev A UART wiring](docs/hardware-wiring.md), [PCB layout](docs/pcb-layout-rev-a.md) and KiCad files are retained **unchanged as an archived variant**; they are not assembly instructions for the current design.

## What MeshCrossway is designed to do

| Capability | Planned experience | Current state |
|---|---|---|
| Dual network messaging | Unified inbox; clear Meshtastic / MeshCore source and reply network | Simulated UI |
| Navigation | GNSS map, POIs, waypoints, recorded tracks and GPX files | Simulated UI and specification |
| Live location sharing | Per-network audience, movement/time triggers, automatic expiry | Design only |
| Direct Only tracking | Attempt zero-relay position updates without silently flooding | Requires firmware and RF tests |
| Radio diagnostics | Independent connection, packet, status and route information | Simulated UI |
| Offline storage | Message history, configuration, GPX, local assets | Planned |
| Future bulk transfer | Authenticated BLE/Wi-Fi for larger files and map assets | Future |
| Future protocol option | Reticulum evaluation, separate from the two V1 radios | Research only |
| Future ATAK interoperability | Evaluate ATAK-CIV gateway, CoT positions and controlled event sharing | Research only |

No automatic cross-network bridging is performed. Each radio keeps native identities, security, channel models and routing rules.

## How the modular system works

The Waveshare **Core acts as a BLE Central with two independent Peripheral connections**. It maintains one Meshtastic PhoneAPI (protobuf) adapter for **MX-M** and one MeshCore Companion (binary) adapter for **MX-C**. The two LoRa networks are handled **on their own XIAO ESP32-S3 + SX1262 Pods**, using their respective native firmware and separate antenna/battery systems.

1. **Core:** touchscreen navigation, unified inbox, GNSS and microSD plans, local configuration and per-network status; Core battery powers **only the Core**.
2. **BLE link A:** Core ↔ **MX-M** via the Meshtastic GATT PhoneAPI. MX-M operates on the Meshtastic LoRa network with its **own battery**.
3. **BLE link B:** Core ↔ **MX-C** via the MeshCore Companion GATT API. MX-C operates on the MeshCore LoRa network with its **own battery**.
4. **Separation:** the Pods are **not** electrically tethered to the Core, do not exchange packets directly by BLE, and should continue native radio operation when disconnected from the Core. The Core does not automatically bridge one mesh network to the other.

The Core firmware will need bounded BLE queues, independent reconnect logic, secure pairing and non-blocking LVGL integration. **Two concurrent BLE radio sessions are a design target, not an already verified firmware feature.** The v0.5.2 baseline keeps Wi-Fi AP+STA enabled and remote screenshots/secondary touch input disabled; v0.5.3 adds experimental touchscreen Wi-Fi provisioning on that baseline.

Each Pod should use a verified upstream BLE-enabled firmware target. In particular, MeshCore must use a **BLE Companion** build rather than an old serial-only build; on Meshtastic ESP32, enabling Wi-Fi may disable Bluetooth. Radio frequencies, antennas, duty/airtime policies and close-proximity RF interference require regional and physical testing.

[Implementation specification](docs/modular-architecture.md) · [Native radio firmware strategy](docs/radio-firmware-strategy.md) · [Protocol documentation (legacy UART sections are historical)](docs/radio-communication-architecture.md)

## Future integrations and collaborations

Explore the [future integrations registry](docs/future-integrations/README.md), including [Colorado Mesh-Client](docs/future-integrations/colorado-mesh-client.md), MeshMapper, ATAK, Reticulum and upstream protocol projects. These are research opportunities, not committed features or partnerships.

## Alternative central console: LilyGO T-Deck Plus

The **LilyGO T-Deck Plus** is being evaluated as an **optional portable replacement for the Waveshare central display/controller**, keeping MeshCore and Meshtastic on independent radio nodes (including compatible Heltec V3 devices) linked by BLE. Its screen, keyboard and trackball would provide the MeshCrossway UI; a dedicated firmware build would aim to support the installed Launcher Web Flasher workflow. **Candidate only — BLE multi-peer operation, firmware packaging and on-device compatibility still require hardware validation.** The existing modular reference architecture remains unchanged.

[Read the T-Deck Plus alternative central-controller feasibility plan](docs/alternative-central-lilygo-tdeck-plus.md).

## Design and documentation

| Topic | Detailed documentation |
|---|---|
| **Approved modular hardware / two BLE links / three batteries** | [Three-device modular architecture](docs/modular-architecture.md) |
| **Former integrated hardware and UART wiring (archived)** | [Rev A wiring and historic shopping guide](docs/hardware-wiring.md) |
| **Archived PCB variant — not the new baseline** | [Rev A 4-layer PCB reference](docs/pcb-layout-rev-a.md) |
| **Manufacturer hardware sources** | [Official source registry](docs/hardware-sources.md) |
| **Power and charging (current concept)** | [Independent batteries and charge-domain requirements](docs/modular-architecture.md#power-and-charging) |
| **Archived shared-carrier power design** | [Rev A 5 V/VBUS study](docs/power-architecture.md) |
| **Radio APIs and BLE transports** | [Meshtastic BLE PhoneAPI + MeshCore BLE Companion](docs/modular-architecture.md#ble-implementation) |
| **Legacy UART transport research** | [Older PROTO/Companion serial design](docs/radio-communication-architecture.md) |
| **Firmware responsibilities** | [Upstream firmware, custom builds and fallback strategy](docs/radio-firmware-strategy.md) |
| **Navigation and live sharing** | [Positions, GPX, routing policies and airtime](docs/navigation-and-sharing.md) |
| **Meshtastic versus MeshCore** | [Illustrated comparison and technical analysis](docs/meshtastic-vs-meshcore.md) |
| **Maps and regional checks** | [Community coverage and map services](docs/mesh-maps-and-regional-check.md) |
| **Future Reticulum** | [Future integration feasibility](docs/reticulum-future-integration.md) |
| **Future ATAK / CoT** | [ATAK-CIV gateway and interoperability research](docs/atak-future-integration.md) |
| **Interface concepts and archived screenshots** | [UI documentation and historical gallery](docs/ui-prototype/README.md) |
| **Project baseline** | [Current V1 technical reference](docs/project-reference.md) |

## Next validation milestones

1. Confirm hardware revisions for the **two Seeed XIAO ESP32-S3 + Wio-SX1262** kits; install compatible **Meshtastic BLE PhoneAPI** and **MeshCore BLE Companion** firmware, respectively.
2. Verify that **each Pod connects to its native phone app independently**, retains LoRa operation without the Core, and reports available radio/battery telemetry.
3. Make the **Waveshare Core establish and maintain both BLE connections at once** using separate protocol adapters, with secure pairing, automatic recovery and a responsive touchscreen.
4. Decode/send real messages separately on Meshtastic and MeshCore; validate identity, acknowledgments, queues and per-network diagnostics without automatic cross-network bridging.
5. Validate the **three separate battery/charging systems**, current consumption, autonomy, GNSS integration, microSD and Wi-Fi/BLE coexistence where needed.
6. Measure RF coexistence, packet reception and antenna separation in both **separated and optionally docked** configurations.
7. **Only then** design optional Pod carriers, enclosures, secure dock/charging and a future modular PCB revision. Keep the older integrated KiCad variant archived and unchanged.

**This project is experimental.** Do not rely on it as the sole communication device in a rescue or safety-critical situation.

## Contributing and license

Open an [issue](https://github.com/Renbrant/MeshCrossway/issues) with board revision, firmware version and reproducible details. Do not post private keys or personal location histories.

MeshCrossway original code and documentation are provided under the [MIT License](LICENSE). Third-party product imagery, upstream firmware and documentation retain their separate licenses; see [image credits and technical-diagram sources](docs/images/README.md). MeshCrossway is independent of the suppliers and protocol projects referenced here.
