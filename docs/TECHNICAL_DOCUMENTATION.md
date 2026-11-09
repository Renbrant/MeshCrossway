# MeshCrossway — Technical Documentation Hub

This is the entry point for engineering documentation, historical firmware releases, research, and planned work. The [main README](../README.md) is intentionally a short project overview.

> **Documented does not mean implemented.** Check each document's date, version, acceptance status, and linked issue/PR before treating it as a released capability.

## Android beta and version strategy

**Android 0.1.0 is a public beta for testing, not version 1.0.0.** It preserves the accepted alpha07 functionality. Small changes use 0.1.1, 0.1.2, etc.; substantial changes use 0.2.0. The first production release is reserved for 1.0.0 after core scope and stability acceptance. Controller firmware versions remain independent.

- [Android beta installation, capabilities, limits and validation](android-apk.md)
- [Download the 0.1.0 beta APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.1.0/MeshCrossway-0.1.0.apk) · [Release notes and checksums](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0)
- [Android source and build guide](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0/android)
- [Preserved Android development history](android-development-history.md)

The Android app uses the phone directly with the two radios; the controller, its SD storage, external GNSS and local browser companion described below belong to the dedicated hardware track.

## Controller firmware versions and release records

- [Current firmware development and operating guide](../firmware/display-prototype/README.md)
- [Current stable v0.10.5 acceptance and recovery](stable-v0105.md)
- [Previous stable v0.9.28 acceptance and recovery](stable-v0928.md)
- [Previous stable v0.9.25](stable-v0925.md)
- [Historical stable v0.8.26](stable-v0826.md)
- [Historical stable v0.8.17](stable-v0817.md)
- [Project status and release history](https://renbrant.github.io/MeshCrossway/project-status/)
- [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/)
- [v0.10.0 OpenFreeMap experimental work (PR #154)](https://github.com/Renbrant/MeshCrossway/pull/154) — **not a validated stable firmware**; the experimental planning files remain on that PR branch until merged.

## Features and implementation details

## What works in the controller

| Capability | Available in controller v0.10.5 baseline |
|---|---|
| **Two independent radios** | Simultaneous Meshtastic and MeshCore BLE sessions, native messages, channels, separate identities, pairing and reconnection. No bridging between networks. |
| **Touchscreen and local browser** | Home dashboard, network filters, conversations and radio settings. The browser companion runs on the device's local Wi-Fi network and includes messages, settings and track tools. |
| **GNSS and maps** | Local GNSS position, satellites/accuracy, clock/timezone, street/topographic maps and local OpenStreetMap night rendering. Cached SD tiles remain usable offline; uncached tiles require Wi-Fi. |
| **Tracks and POIs** | Record/pause/resume tracks, save POIs, organize folders, preview routes and download GPX through the browser companion. |
| **Readable node lists** | Bounded recent-node lists, fixed row spacing, sorting by last heard or distance, and explicit manual refresh. |
| **Map customization** | Independent format, size and color for each network; circles, squares, crosses or ID badges; optional grouping; zoom controls close automatically after use. |
| **Colors by last reception** | Optional per network: green under 24 hours, yellow from 1 to under 7 days, gray at 7 days or more/unknown. This indicates reception age, not guaranteed reachability. |
| **Configurable local retention** | Keep nodes for 0–365 days (default 30; 0 disables expiry). Protect favorites, local radios, retained conversations/drafts and unknown timestamps. Filter stale imports and save through the SD worker. |
| **Display and connectivity controls** | Automatic 180-degree landscape orientation, saved rotation lock, manual flip/recalibration, brightness and screen timeout; Bluetooth and Wi-Fi power controls. |
| **Diagnostics and storage recovery** | Console memory logs, internal RAM/PSRAM/DMA snapshots, allocation and touch diagnostics, bounded SD work and recovery archives. Cleanup reduces stale records; fixed table capacity remains allocated. |
| **Live position sharing** | Not implemented; remains future work. |

Start with the [current firmware guide](../firmware/display-prototype/README.md). Recent operating details: [node age and retention](node-age-v0928.md), [orientation correction](rotation-reference-v0927.md) and [map controls](stable-v0925.md). Existing photos and screenshots retain their original version labels; they are historical evidence, not new v0.9.32 captures. Tested configurations do not establish compatibility with every radio board or firmware version.

## Remote interface and track backup

The companion page is hosted by MeshCrossway and accessed from a browser on the same local network. It lets the user work with messages and network-specific radio views, organize tracks into folders, review a route preview and download GPX files as portable backups. The track journal remains stored on the device's microSD.

The remote page complements the touchscreen; radio messages still use their native MeshCore or Meshtastic connection. No automatic forwarding, conversion, synchronization or cross-network sending occurs.



## Hardware architecture and technical references

## Hardware architecture

The implemented software architecture uses three independent devices: a touchscreen Core and two LoRa Radio Pods. Two radio links are operating; the final detachable Pod packaging, power design, RF and environmental qualification remain in progress.

| Unit | Selected hardware | Role |
|---|---|---|
| **MeshCrossway Core** | Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0, 480 × 320 touchscreen | LVGL interface, local storage, GNSS integration and two BLE Central connections |
| **MX-M Radio Pod** | User-selected Meshtastic radio that supports BLE PhoneAPI; Heltec V3 tested | Native Meshtastic radio using its BLE PhoneAPI |
| **MX-C Radio Pod** | User-selected MeshCore radio that supports BLE Companion; Heltec V3 tested | Native MeshCore radio using its BLE Companion interface |

There is no required XIAO model: users can choose compatible radio hardware for each network. The selected devices must expose the corresponding native BLE API with compatible firmware; Heltec V3 is the current development-tested setup, and other model/firmware combinations require validation. Each unit has its own battery and power system. The Core communicates with each Pod wirelessly over BLE; the LoRa radios do not connect to the Core over UART, SPI or GPIO. Each Pod must continue native LoRa operation if the Core is off or out of range.

The two BLE links use separate protocol adapters: Meshtastic protobuf PhoneAPI over BLE GATT and MeshCore Companion binary protocol over BLE GATT. Pairing, identities, settings and messages stay separate. The Core does not relay or bridge between networks.

**Hardware still being validated:** exact battery and charger choices, battery protection and monitoring, GNSS antenna and power integration, antenna separation, RF coexistence, endurance and final enclosure fit. The optional mechanical dock is not required for BLE operation. The previous integrated Rev A UART/PCB design is archived and is not the current assembly baseline.

- [Approved modular architecture and validation plan](modular-architecture.md)
- [Hardware source list](hardware-sources.md)
- [Archived Rev A wiring](hardware-wiring.md)
- [Archived Rev A PCB layout](pcb-layout-rev-a.md)



## Research, experiments and performance analysis

- [Hardware platform alternatives: ESP32-P4, T-Deck, ThinkNode and mobile apps](HARDWARE_PLATFORM_EVALUATION.md)
- [Map rendering latency and memory investigation](map-render-latency-v087.md)
- [Maps, GNSS and tile architecture](maps-gps-v081.md)
- [Radio firmware and native protocol boundaries](radio-firmware-strategy.md)
- [Meshtastic vs MeshCore protocol comparison](meshtastic-vs-meshcore.md)
- [Regional mesh map coverage](mesh-maps-and-regional-check.md)
- [BLE coordinator memory investigation](ble-coordinator-memory-v095.md)
- [Memory allocation origin investigation](allocation-origin-v0921.md)
- [Future integrations registry](future-integrations/README.md)
- [Archived UI concepts](ui-prototype/README.md)

## Planned improvements and issues

GitHub Issues is the source of truth for requested features, investigations and unresolved bugs. Open issues are **proposals or work in progress, not released functionality**.

[Browse all open issues](https://github.com/Renbrant/MeshCrossway/issues?q=is%3Aissue%20is%3Aopen) · [Open pull requests](https://github.com/Renbrant/MeshCrossway/pulls) · [Contributing](../CONTRIBUTING.md)

### Recently updated open issues (snapshot)

- [#160: Feature: Secure QR-code sharing and import of MeshCore channels](https://github.com/Renbrant/MeshCrossway/issues/160)
- [#159: Feature: Advanced MeshCore repeater management and network visibility](https://github.com/Renbrant/MeshCrossway/issues/159)
- [#158: Feature: Favorite nodes and private notes across Meshtastic and MeshCore](https://github.com/Renbrant/MeshCrossway/issues/158)
- [#157: Feature: Configurable quick replies for Meshtastic and MeshCore chats](https://github.com/Renbrant/MeshCrossway/issues/157)
- [#48: Expandir catálogo de emojis do MeshCrossway (93 → cobertura dos 272 glyphs comuns, com Unicode completo)](https://github.com/Renbrant/MeshCrossway/issues/48)
- [#37: Roadmap: implementar mensagens, Nodes, mapas offline e diagnósticos nativos no MeshCrossway](https://github.com/Renbrant/MeshCrossway/issues/37)
- [#46: [PLANEJAMENTO] Histórico persistente e seguro no microSD — mensagens, dados locais e criptografia por dispositivo](https://github.com/Renbrant/MeshCrossway/issues/46)
- [#47: [PROJETO / SEM BURN] Chave física eFuse HMAC_UP por ESP32-S3 para microSD criptografado](https://github.com/Renbrant/MeshCrossway/issues/47)
- [#44: Follow-up: restore simultaneous MX-C + MX-M BLE sessions on boot with fully independent protocol state](https://github.com/Renbrant/MeshCrossway/issues/44)

This snapshot is informational and may become outdated; use the live GitHub Issues page for current priority and status.

## Detailed reference index (moved from the main README)

### Technical documentation

| Topic | Reference |
|---|---|
| v0.10.5 recommended controller baseline | [Acceptance and recovery](stable-v0105.md) |
| v0.9.28 previous stable | [Acceptance, binary pin and recovery](stable-v0928.md) |
| Current user features and test procedure | [Firmware guide](../firmware/display-prototype/README.md) · [Age colors and retention](node-age-v0928.md) |
| Rotation, lock and manual correction | [Orientation notes](rotation-reference-v0927.md) |
| v0.9.25 previous stable recovery | [Previous stable record](stable-v0925.md) |
| v0.8.26 historical stable image | [Stable release record](stable-v0826.md) |
| v0.8.17 historical stable image | [Historical release record](stable-v0817.md) |
| v0.8.18 node map and activity ordering | [Feature notes](node-map-and-recent-activity-v0818.md) |
| v0.8.20 Home GPS card spacing | [Feature notes](home-gps-layout-v0820.md) |
| v0.8.20 Configurable map node markers and compact toggles | [Map and display notes](map-node-marker-settings-v0820.md) |
| v0.8.21 Adjustable map node marker size and quick access | [Feature notes](map-node-size-v0821.md) |
| v0.8.22 Configurable map node colors | [Feature notes](map-node-color-v0822.md) |
| v0.8.23 Separate node icon and color settings by network | [Feature notes](map-node-style-v0823.md) |
| v0.8.24 Meshtastic and MeshCore settings navigation redesign | [Candidate notes](settings-navigation-v0824.md) |
| v0.8.25 map fallback centering when local GPS has no current fix | [Candidate notes](map-node-centering-v0825.md) |
| v0.8.26 remote MeshCore chat sender names | [Stable release record](stable-v0826.md) |
| v0.9.1 compact settings pages with back navigation on device and remote | [Candidate notes](settings-navigation-v091.md) |
| Simultaneous BLE radios and per-network settings | [Dual-radio implementation notes](dual-radio-settings-v0816.md) |
| Device and browser operation screenshots | [Field evidence](field-evidence-v0816.md) |
| Modular Core and Radio Pod design | [Modular architecture](modular-architecture.md) |
| Native firmware and protocol boundary | [Radio firmware strategy](radio-firmware-strategy.md) |
| Maps and map-rendering investigation | [Maps/GPS notes](maps-gps-v081.md) · [Map rendering latency](map-render-latency-v087.md) |
| Track organization, route preview and GPX backup | [Field evidence](field-evidence-v0816.md) · [Maps/GPS notes](maps-gps-v081.md) |
| Meshtastic and MeshCore comparison | [Protocol comparison](meshtastic-vs-meshcore.md) |
| Community maps and regional coverage | [Maps and regional check](mesh-maps-and-regional-check.md) |
| Future integrations | [Integration registry](future-integrations/README.md) |
| Archived interface concepts | [UI prototype gallery](ui-prototype/README.md) |

The **LilyGO T-Deck Plus** is an optional future central-console candidate, not the current hardware baseline. Any replacement would still need to support two independent BLE radio connections and the same separation between Meshtastic and MeshCore. See the [T-Deck Plus feasibility plan](alternative-central-lilygo-tdeck-plus.md).



## Maintenance convention

- README: what the project does, why it exists, screenshots, basic setup, install, contribution.
- This hub: index of firmware versions, technical topics, research, and future work.
- Detailed research stays in separate, linked Markdown files; do not duplicate long technical analyses in README.
- Issues: track work not yet delivered; PRs: implementation and review; release notes: only verified capabilities.
- Historical documents are preserved at their existing paths so inbound links continue working.
