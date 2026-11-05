![MeshCrossway — Meshtastic and MeshCore modular three-device system](docs/images/meshcrossway-modular-hero.jpg)

# MeshCrossway

<p align="center">
  <a href="https://renbrant.github.io/MeshCrossway/"><img alt="Project website" src="https://img.shields.io/badge/PROJECT%20SITE-OPEN-1f6feb?style=for-the-badge"></a>
  <a href="https://renbrant.github.io/MeshCrossway/flasher/"><img alt="Web Flasher" src="https://img.shields.io/badge/WEB%20FLASHER-INSTALL-orange?style=for-the-badge"></a>
</p>
<p align="center">
  <a href="https://github.com/Renbrant/MeshCrossway/tree/main/docs"><img alt="Project documentation" src="https://img.shields.io/badge/DOCUMENTATION-READ-444444?style=for-the-badge"></a>
  <a href="https://github.com/Renbrant/MeshCrossway/blob/main/docs/stable-v0826.md"><img alt="Recommended stable firmware v0.8.26" src="https://img.shields.io/badge/STABLE%20FIRMWARE-v0.8.26-2ea44f?style=for-the-badge"></a>
  <a href="https://github.com/sponsors/Renbrant"><img alt="Donate or become a sponsor" src="https://img.shields.io/badge/DONATE%20%2F%20SPONSOR-SUPPORT%20THE%20PROJECT-ff8a24?style=for-the-badge"></a>
</p>

**A portable, offline-first touchscreen communicator for Meshtastic and MeshCore.** MeshCrossway brings two independent LoRa networks, messaging, GNSS and track tools into one interface. It does not bridge or mix traffic between the networks.

> **Project status — v0.9.16 stable; v0.9.21 candidate.** The accepted recent-node layout/manual refresh and v0.9.20 bounded map previews are preserved. v0.9.21 attributes allocation failures through bounded console records, following the 124-byte internal/DMA failure observed on v0.9.20. Physical validation is pending. See the [diagnostic procedure](docs/allocation-origin-v0921.md) and [memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137).

## Support the project

Developing MeshCrossway takes time and ongoing investment in hardware parts and AI model tokens. If you'd like to help cover these costs, you're welcome to make a one-time donation or become a project sponsor through [GitHub Sponsors](https://github.com/sponsors/Renbrant).


## See it in operation

## See it in operation

### v0.9.8 candidate running on the device

These field photos show the MeshCrossway touchscreen hardware running the v0.9.8 candidate, including its home status dashboard, map and message views.

<p align="center">
  <img src="docs/site/media/v0.9.8-field/01-assembled-device.jpg" width="49%" alt="Assembled MeshCrossway touchscreen console with external GNSS antenna">
  <img src="docs/site/media/v0.9.8-field/02-home-status.jpg" width="49%" alt="v0.9.8 home dashboard showing connected Meshtastic and MeshCore radios, GPS and Wi-Fi">
</p>
<p align="center">
  <img src="docs/site/media/v0.9.8-field/03-map-view.jpg" width="49%" alt="MeshCrossway map view with nearby network nodes">
  <img src="docs/site/media/v0.9.8-field/04-messages-view.jpg" width="49%" alt="MeshCrossway message list showing received messages and hop counts">
</p>

### Earlier v0.8.16 device and local browser companion screenshots

These v0.8.16 screenshots document the touchscreen and the device's local browser companion. The browser interface is served by the device over the local network; it is not a cloud service or a web simulator.

<p align="center">
  <img src="docs/site/media/v0.8.16/01-device-home-radio-gps.png" width="49%" alt="Device home screen with Meshtastic and MeshCore connected, local GPS and Wi-Fi status">
  <img src="docs/site/media/v0.8.16/02-device-network-nodes.png" width="49%" alt="Device node list with Meshtastic network filter">
</p>
<p align="center">
  <img src="docs/site/media/v0.8.16/03-device-message-list.png" width="49%" alt="Messages displayed on the MeshCrossway device">
  <img src="docs/site/media/v0.8.16/04-device-conversation.png" width="49%" alt="Conversation view on the MeshCrossway device">
</p>
<p align="center">
  <img src="docs/site/media/v0.8.16/05-remote-tracks-map.png" width="49%" alt="Local browser companion showing track folders, saved tracks and route preview">
  <img src="docs/site/media/v0.8.16/06-remote-messages.png" width="49%" alt="Local browser companion showing conversations, groups, nodes and message composer">
</p>

## Hardware 3D renders

The renders below show the committed case design. The Waveshare ESP32-S3 enclosure also has printable STL/3MF exports, and the 10,000 mAh battery case has an editable FreeCAD source model.

<p align="center">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20144917.png" width="49%" alt="MeshCrossway hardware 3D render, view 1">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145119.png" width="49%" alt="MeshCrossway hardware 3D render, view 2">
</p>
<p align="center">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145203.png" width="49%" alt="MeshCrossway hardware 3D render, view 3">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145300.png" width="49%" alt="MeshCrossway hardware 3D render, view 4">
</p>
<p align="center">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145317.png" width="49%" alt="MeshCrossway hardware 3D render, view 5">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145336.png" width="49%" alt="MeshCrossway hardware 3D render, view 6">
</p>

- [Open the Waveshare enclosure STL in GitHub's 3D viewer](<hardware/3D/Waveshare esp32-s3 case.stl>)
- [Download the Waveshare enclosure 3MF](<hardware/3D/Waveshare esp32-s3 case.3mf>)
- [View the 10,000 mAh battery case FreeCAD source](<hardware/3D/Case bat 10000mA.FCStd>)

## What works today

| Capability | Current status |
|---|---|
| **Dual radio connections** | Meshtastic and MeshCore can connect simultaneously through separate BLE sessions. Network identities, settings and traffic remain independent. |
| **Touchscreen interface** | Home status, radio connection cards, node lists, network filters, message lists and conversations are shown working in the v0.8.16 screenshots. |
| **Remote browser companion** | Running on the device's local network. It provides separate network views for messages, conversations, groups, nodes and radio settings. |
| **Track workspace** | The browser shows track folders, saved tracks and route previews. GPX downloads provide a portable backup of tracks. |
| **Radio settings shortcuts** | Radio cards on Home open their own settings; the selected radio can also be configured through the browser interface. |
| **Device maps and saved routes** | The v0.8.17 map and remote route-loading improvement is confirmed working well on the device. v0.8.18 added optional Meshtastic and MeshCore node markers when positions are available. |
| **Live position sharing** | Not implemented; remains future work. |

See the [v0.8.16 field evidence](docs/field-evidence-v0816.md) and [dual-radio implementation and validation notes](docs/dual-radio-settings-v0816.md). Current compatibility evidence comes from the tested hardware and does not establish compatibility with every radio board or firmware version.

## Remote interface and track backup

The companion page is hosted by MeshCrossway and accessed from a browser on the same local network. It lets the user work with messages and network-specific radio views, organize tracks into folders, review a route preview and download GPX files as portable backups. The track journal remains stored on the device's microSD.

The remote page complements the touchscreen; radio messages still use their native MeshCore or Meshtastic connection. No automatic forwarding, conversion, synchronization or cross-network sending occurs.

## Hardware architecture

The selected product direction is three independently powered, detachable devices: a touchscreen Core and two LoRa Radio Pods. This modular design is the target architecture; mechanical, power, RF and environmental validation is still in progress.

| Unit | Selected hardware | Role |
|---|---|---|
| **MeshCrossway Core** | Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0, 480 × 320 touchscreen | LVGL interface, local storage, GNSS integration and two BLE Central connections |
| **MX-M Radio Pod** | Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | Native Meshtastic radio using its BLE PhoneAPI |
| **MX-C Radio Pod** | Second Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | Native MeshCore radio using its BLE Companion interface |

Each unit has its own battery and power system. The Core communicates with each Pod wirelessly over BLE; the LoRa radios do not connect to the Core over UART, SPI or GPIO. Each Pod must continue native LoRa operation if the Core is off or out of range.

The two BLE links use separate protocol adapters: Meshtastic protobuf PhoneAPI over BLE GATT and MeshCore Companion binary protocol over BLE GATT. Pairing, identities, settings and messages stay separate. The Core does not relay or bridge between networks.

**Hardware still being validated:** exact battery and charger choices, battery protection and monitoring, GNSS antenna and power integration, antenna separation, RF coexistence, endurance and final enclosure fit. The optional mechanical dock is not required for BLE operation. The previous integrated Rev A UART/PCB design is archived and is not the current assembly baseline.

- [Approved modular architecture and validation plan](docs/modular-architecture.md)
- [Hardware source list](docs/hardware-sources.md)
- [Archived Rev A wiring](docs/hardware-wiring.md)
- [Archived Rev A PCB layout](docs/pcb-layout-rev-a.md)

## Known issue and next steps

The current software work adds categorized, independent radio settings on top of the stable v0.8.26 baseline. Hardware work continues on the three-device modular design and its independently powered Core and Pods.

1. Restore device map tiles and saved-route loading without regressing the working dual-radio connections.
2. Expand compatibility and recovery checks across supported radio boards and native firmware builds.
3. Validate battery protection, charging, autonomy, GNSS integration and microSD behavior on the target hardware.
4. Check enclosure fit, antenna spacing and RF coexistence in separated and docked configurations.
5. Verify GPX backup and restoration behavior against retained track data.

## Technical documentation

| Topic | Reference |
|---|---|
| v0.8.26 stable image | [Stable release record](docs/stable-v0826.md) |
| v0.8.17 historical stable image | [Historical release record](docs/stable-v0817.md) |
| v0.8.18 node map and activity ordering | [Feature notes](docs/node-map-and-recent-activity-v0818.md) |
| v0.8.20 Home GPS card spacing | [Feature notes](docs/home-gps-layout-v0820.md) |
| v0.8.20 Configurable map node markers and compact toggles | [Map and display notes](docs/map-node-marker-settings-v0820.md) |
| v0.8.21 Adjustable map node marker size and quick access | [Feature notes](docs/map-node-size-v0821.md) |
| v0.8.22 Configurable map node colors | [Feature notes](docs/map-node-color-v0822.md) |
| v0.8.23 Separate node icon and color settings by network | [Feature notes](docs/map-node-style-v0823.md) |
| v0.8.24 Meshtastic and MeshCore settings navigation redesign | [Candidate notes](docs/settings-navigation-v0824.md) |
| v0.8.25 map fallback centering when local GPS has no current fix | [Candidate notes](docs/map-node-centering-v0825.md) |
| v0.8.26 remote MeshCore chat sender names | [Stable release record](docs/stable-v0826.md) |
| v0.9.1 compact settings pages with back navigation on device and remote | [Candidate notes](docs/settings-navigation-v091.md) |
| Simultaneous BLE radios and per-network settings | [Dual-radio implementation notes](docs/dual-radio-settings-v0816.md) |
| Device and browser operation screenshots | [Field evidence](docs/field-evidence-v0816.md) |
| Modular Core and Radio Pod design | [Modular architecture](docs/modular-architecture.md) |
| Native firmware and protocol boundary | [Radio firmware strategy](docs/radio-firmware-strategy.md) |
| Maps and map-rendering investigation | [Maps/GPS notes](docs/maps-gps-v081.md) · [Map rendering latency](docs/map-render-latency-v087.md) |
| Track organization, route preview and GPX backup | [Field evidence](docs/field-evidence-v0816.md) · [Maps/GPS notes](docs/maps-gps-v081.md) |
| Meshtastic and MeshCore comparison | [Protocol comparison](docs/meshtastic-vs-meshcore.md) |
| Community maps and regional coverage | [Maps and regional check](docs/mesh-maps-and-regional-check.md) |
| Future integrations | [Integration registry](docs/future-integrations/README.md) |
| Archived interface concepts | [UI prototype gallery](docs/ui-prototype/README.md) |

The **LilyGO T-Deck Plus** is an optional future central-console candidate, not the current hardware baseline. Any replacement would still need to support two independent BLE radio connections and the same separation between Meshtastic and MeshCore. See the [T-Deck Plus feasibility plan](docs/alternative-central-lilygo-tdeck-plus.md).

## Safety, contributing and license

MeshCrossway is experimental and is not certified for safety-critical use. Do not rely on it as the sole communication device in a rescue or emergency.

Report issues with the board revision, firmware version and reproducible details. Do not post private keys or personal location histories. Original MeshCrossway code and documentation are under the [MIT License](LICENSE); compiled firmware and vendor assets include third-party components under their own licenses.

See the [licensing policy](docs/licensing.md), [third-party notices](THIRD_PARTY_NOTICES.md), [contribution guide](CONTRIBUTING.md) and [image credits](docs/images/README.md). MeshCrossway is independent of the suppliers and protocol projects referenced here.

## Contributing

Contributions are welcome across firmware, hardware, documentation, testing, and interface design. To get involved, open a [GitHub issue](https://github.com/Renbrant/MeshCrossway/issues) to introduce yourself or propose an area to work on; use a pull request for concrete changes. Please read the [contribution guide](CONTRIBUTING.md) first. For larger changes, open an issue before starting so the work can be coordinated.
