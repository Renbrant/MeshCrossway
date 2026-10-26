# MeshCrossway

**V1 hardware (selected 2026-10-26):** Waveshare ESP32-S3 3.5-inch touchscreen + **2 × Seeed XIAO ESP32S3 / Wio-SX1262** (Meshtastic and MeshCore) + u-blox MAX-M10S GNSS. **[Current BOM and wiring](docs/hardware-wiring.md)**. Former LoRa V3 boards are superseded.

![MeshCrossway concept: one carrier PCB with two XIAO/Wio radios inside a nearly invisible transparent enclosure](docs/images/meshcrossway-hero.webp)

*Temporary product visualization. The protected 1S LiPo battery is mounted **behind the PCB**. Exact component geometry, module footprints and enclosure CAD remain unverified; the render is not a fabrication drawing.*

**Two mesh networks. One portable terminal.**

MeshCrossway is an open-source, standalone touchscreen communicator being designed to access **Meshtastic** and **MeshCore** through separate LoRa radios. The goal is simple: communication, positioning and navigation in one portable device, even without cellular service or internet.

> **Project status: design and bench-prototype planning.** The interactive UI is a simulator with sample data. Firmware, radio integration, hardware wiring and field performance are **not yet validated**. This is not a certified emergency communications device.

## Meshtastic or MeshCore?

**[Read the complete Meshtastic vs MeshCore guide →](docs/meshtastic-vs-meshcore.md)**

Start with an illustrated introduction for newcomers, learn from real user experiences, then explore the technical details. The guide progresses from everyday choices and regional maps to routing, security, radio behavior and a complete 44-criterion comparison.

- [Part 1: Illustrated basics and choosing a network](docs/meshtastic-vs-meshcore.md#part-1-start-with-the-basics)
- [Part 2: User experiences and the community video](docs/meshtastic-vs-meshcore.md#what-users-report)
- [Part 3: Technical deep dive](docs/meshtastic-vs-meshcore.md#part-3-technical-deep-dive) and [full comparison table](docs/meshtastic-vs-meshcore.md#complete-comparison-table)
- [Find maps and check your region](docs/mesh-maps-and-regional-check.md), including LetsMesh, Meshtastic maps and Denver/Colorado coverage resources

## Explore the interface

[![Launch the MeshCrossway UI simulator](https://img.shields.io/badge/OPEN-INTERACTIVE%20UI%20SIMULATOR-007acc?style=for-the-badge)](https://renbrant.github.io/MeshCrossway/)

[![Current MeshCrossway Home, Navigation and Map Tools interface](docs/ui-prototype/ui-overview-current.svg)](https://renbrant.github.io/MeshCrossway/)

**[Try the interactive touchscreen simulator →](https://renbrant.github.io/MeshCrossway/)**

A browser preview of the planned **480 × 320** interface. The static illustration highlights the current Home, Map and Map → Tools navigation. The link opens the full interactive simulator, including chats, live-location sharing, POIs, tracks and diagnostics.

## Planned capabilities

- **Two independent mesh networks:** unified inbox and conversations with clear Meshtastic/MeshCore identification, without automatically bridging their protocols.
- **Messaging:** private conversations, channels, quick replies and local history.
- **Navigation:** GNSS positioning, points of interest, waypoints, track recording and GPX management.
- **Location sharing:** send a position or POI, share with one person or a group, and configure tracking by time, movement and session duration.
- **Direct Tracking:** proposed one-hop tracking to reduce repeater traffic, subject to native-protocol support and real-world validation. One-hop transmissions still consume shared airtime.
- **Offline-first:** local microSD storage, with future map downloads and optional Bluetooth transfer of larger files.
- **Field tools:** radio diagnostics, connection health and team-position views.

## Key functions illustrated

These are **illustrative screens with simulated data**, based on the planned touchscreen UI. They are not screenshots of operating hardware. **[Explore the interactive simulator](https://renbrant.github.io/MeshCrossway/)** for clickable examples.

### Unified messaging

[![Unified Meshtastic and MeshCore conversation preview](docs/ui-prototype/readme/messaging.svg)](https://renbrant.github.io/MeshCrossway/)

Conversations from both networks appear in one inbox, while every contact and message retains its original network identity. Replies require an explicit destination.

### Navigation and map tools

[![Navigation dropdown menu preview](docs/ui-prototype/readme/navigation.svg)](https://renbrant.github.io/MeshCrossway/)

The **Map → Tools** menu brings POIs, saved tracks, recording, team positions, sharing and GNSS tools together without crowding the map.

### POIs and waypoints

[![POI and waypoint interface preview](docs/ui-prototype/readme/pois.svg)](https://renbrant.github.io/MeshCrossway/)

Save trailheads, checkpoints, water sources and other locations. Compact coordinates could be shared over a selected LoRa network, subject to protocol capability and airtime limits.

### Tracks and GPX

[![Recorded track library preview](docs/ui-prototype/readme/tracks.svg)](https://renbrant.github.io/MeshCrossway/)

Record and review tracks locally. Full GPX transfer is planned through an authenticated Bluetooth companion in a future phase rather than sending large files over LoRa.

### Live location sharing

[![Location sharing configuration preview](docs/ui-prototype/readme/live-location.svg)](https://renbrant.github.io/MeshCrossway/)

Choose a person or group, transmission interval, movement threshold and automatic expiration. **Direct Only** is the proposed default to avoid repeater forwarding, but it still occupies shared radio airtime. Strict zero-hop behavior must be validated with both protocols.

### Radio status and diagnostics

[![Independent radio status preview](docs/ui-prototype/readme/radios.svg)](https://renbrant.github.io/MeshCrossway/)

Separate health information for Meshtastic and MeshCore, with planned diagnostics for link status, packet logs and routing behavior.

### Device settings

[![Device appearance and quick controls preview](docs/ui-prototype/readme/settings.svg)](https://renbrant.github.io/MeshCrossway/)

Centralized preferences for display appearance, device controls, local storage and privacy—separate from the navigation tools.

## Integrated PCB and transparent case

![Exploded view concept: both Seeed radio modules and GNSS mounted to one carrier PCB with protected LiPo behind the assembly](docs/images/architecture/meshcrossway-exploded.webp)

The planned device has **one custom carrier PCB**, hosting the Waveshare display assembly, two independent XIAO ESP32S3 + Wio-SX1262 radio kits and MAX-M10S GNSS breakout. The **battery goes behind the carrier**, enclosed by a nearly invisible transparent rear shell. The illustration is a conceptual visualization, not verified physical fit or CAD. [See current wiring and assembly illustrations](docs/hardware-wiring.md).

## Illustrated system architecture

![System overview](docs/images/architecture/system-overview.svg)

The two radios remain independent. For exact electrical wiring and integration details, see the [wiring diagram](docs/hardware-wiring.md) and [API architecture](docs/radio-communication-architecture.md).

## System overview

```mermaid
flowchart TD
    UI["Touchscreen & MeshCrossway firmware"] --> HOST["ESP32-S3 controller"]
    GPS["u-blox GNSS"] --> HOST
    SD["Onboard microSD"] <--> HOST
    HOST <-->|UART| MT["Meshtastic radio"]
    HOST <-->|UART| MC["MeshCore radio"]
    MT <--> A["Meshtastic network"]
    MC <--> B["MeshCore network"]
```

The central controller handles the UI, GNSS, navigation, storage and sharing policy. Each radio runs its own upstream protocol firmware, with board-specific configuration or compilation where required. Source patches are a last resort.

## Project documentation

| Area | Details |
|---|---|
| **Hardware & purchasing** | [Selected equipment, specifications, shopping links, prices and wiring](docs/hardware-wiring.md) |
| **Power** | [Battery charging, 3.3 V architecture and electrical verification](docs/power-architecture.md) |
| **Firmware architecture** | [Radio firmware policy, processing responsibilities and tests](docs/radio-firmware-strategy.md) |
| **Navigation & sharing** | [GPS, POIs, GPX, group tracking, Direct Only and congestion controls](docs/navigation-and-sharing.md) |
| **MT vs MC** | [Illustrated introduction, user experiences and complete technical comparison](docs/meshtastic-vs-meshcore.md) |
| **Future: Reticulum** | [Concepts, integration options and evaluation plan](docs/reticulum-future-integration.md) — a future candidate beyond the initial MT + MC design |
| **Maps & local coverage** | [Map directory, data limitations and regional field checks](docs/mesh-maps-and-regional-check.md) |
| **UI prototype** | [Simulator features, status and limitations](docs/ui-prototype/README.md) |
| **Technical reference** | [Detailed project decisions, roadmap, equipment and supporting references](docs/project-reference.md) |

## Development priorities

1. Verify electrical interfaces, available power and both radio UART connections.
2. Prove independent messaging with upstream Meshtastic and MeshCore builds.
3. Implement the LVGL touchscreen interface, GNSS and persistent storage.
4. Validate tracking, routing restrictions, congestion limits and privacy on real hardware.
5. Evaluate offline maps, Bluetooth GPX transfer, enclosure and field performance.

Contributions and technical discussions are welcome in [GitHub Issues](https://github.com/Renbrant/MeshCrossway/issues). Please include hardware revisions, firmware versions and reproducible steps; never publish private keys or sensitive location history.

## License

Original project code and documentation: [MIT License](LICENSE). Third-party software, hardware documentation and product images retain their respective licenses. See [image sources and limitations](docs/images/README.md).

MeshCrossway is an independent project and is not affiliated with or endorsed by the manufacturers or upstream projects referenced here.


## Radio interfaces and protocol integration

See the [radio communication architecture and API compatibility study](docs/radio-communication-architecture.md) for the UART wiring, Meshtastic Protobuf framing, MeshCore Companion API, firmware build policy, radio capability negotiation, RF coexistence, failure handling and acceptance tests. This is a design and source-review document, **not** a verified hardware integration.
