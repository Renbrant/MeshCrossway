> **Browser design reference, not current firmware specification.** The physical LVGL code on `main` is v0.7.6; new proposed swipes, Settings navigation and chat indicators are tracked in [open v0.7.7 PR #54](https://github.com/Renbrant/MeshCrossway/pull/54). This HTML demo may intentionally lag native behavior. [Implementation status](../current-status.md).

# MeshCrossway UI Prototype — Landscape Side Console

> **Current architecture (2026-10-28):** The UI will connect via two concurrent BLE Central links to independent MX-M Meshtastic and MX-C MeshCore Radio Pods. Hardware remains separate; archived UART/PCB implementation references below are not current. [Approved design](../modular-architecture.md).

This folder contains a **browser-based UI concept** for the 3.5-inch MeshCrossway touchscreen. It is separate from the device's ESP-IDF/LVGL firmware and does not establish that all pictured screens are implemented on hardware. **Older README screenshots were removed from the repository front page and preserved in the [historical UI gallery](archived-readme-gallery.md)**.

## Landscape UI v5 — dual collapsible sidebars

The current interactive simulator targets the **480 × 320 landscape screen**. The v5 visual language takes the approved MeshCrossway field-console concept as its reference: **high-contrast dark slate surfaces, individually colored vector action icons, cyan active-state accents, topographic-inspired map styling and protruding edge handles** for both collapsible panels.

- **Left sidebar:** global Home, Chats, Map, Radios and More navigation. The edge chevron collapses the sidebar to icons to reclaim map/content width.
- **Center:** the current workspace, with more usable vertical area because the bottom navigation bar has been removed.
- **Right sidebar:** context-sensitive actions. The Map page displays the full suite of POI, track, GNSS and sharing functions. Other pages display relevant controls; its chevron collapses the pane for a larger workspace.
- **Top quick-settings shade:** tap the top status bar (or swipe down from the header on a touch device) to expand demo toggles for GPS, Bluetooth, Wi-Fi, sound and brightness. Tap Close to retract it.

**Simulation only:** quick-setting switches alter the browser preview, not real hardware radios, GNSS or peripherals. The side panels, drawer and touch targets need physical touchscreen usability validation.

## Goals

- Validate the basic information architecture before firmware implementation.
- Keep Meshtastic and MeshCore identities visibly distinct.
- Prioritize large touch targets for a 480 × 320 display.
- Make radio health, GNSS validity, message provenance, and message state easy to understand.
- Keep the initial experience useful without a phone or internet connection.

## Screens in this prototype

- **Home** — unified inbox, network status, quick actions, and current GNSS position.
- **Messages** — combined history while preserving the source network on every conversation.
- **Position** — GNSS status and a lightweight spatial view concept.
- **Radios** — independent health and connectivity for Meshtastic and MeshCore adapters.
- **Setup** — early device-level settings concept.
- **Compose** — explicit network and destination selection before a message is queued.

## Open the prototype

Open [the interactive GitHub Pages prototype](https://renbrant.github.io/MeshCrossway/) or open [index.html](index.html) locally in a browser.

> GitHub's normal file view displays the source code rather than executing the HTML. The current demo is published through GitHub Pages.

## Static overview — historical browser mockup

![Historical MeshCrossway Home, Map and Tools menu mockup](ui-overview-current.svg)

The interface groups POIs, waypoints, saved tracks, track recording, live-location sharing, GNSS sky, team positions and saved paths under **Map → Tools**. **More** contains general device controls and settings. This archived overview is illustrative and may differ from the live simulator or current physical firmware. Open the browser simulator for its currently deployed behavior.

## Important

This is a UI mockup only. It uses demo data and does not communicate with Meshtastic, MeshCore, GNSS hardware, storage, or the ESP32.

The prototype deliberately avoids claiming that queued messages were transmitted or delivered. The firmware implementation will need to map each protocol's real evidence into accurate message states.


## Selected development hardware — modular BLE architecture

The approved system has **three detachable, independently powered units**, each with its **own rechargeable battery and power-management circuit**:

- **MeshCrossway Core:** Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0, with UI, onboard microSD and planned u-blox MAX-M10S GNSS; it will act as a **BLE Central** for two simultaneous radio connections.
- **MX-M Pod:** Seeed XIAO ESP32-S3 + Wio-SX1262, running **Meshtastic with BLE PhoneAPI**, with its own battery and LoRa antenna.
- **MX-C Pod:** a separate Seeed XIAO ESP32-S3 + Wio-SX1262, running **MeshCore BLE Companion**, with its own battery and LoRa antenna.

**No wired UART, integrated shared carrier PCB, or shared battery connects the three units in the current design.** Earlier UART pinouts, 5 V selectors and the old Rev A KiCad files remain historical references only. BLE integration and real radio operations are still under development. See the [approved modular architecture and software-first plan](../modular-architecture.md).

## Archived UI screenshots

The former main-README screenshots and screen descriptions are retained in the **[historical UI gallery](archived-readme-gallery.md)**, separately from the project's main introduction. They are design references, not current device screenshots or an accurate statement of implemented functionality.


## Navigation and sharing concept

The Navigation screen now includes interactive **POIs**, **saved tracks**, **track recording**, and a **share transport** choice.

- Compact waypoint/POI location messages can be represented through a chosen mesh network adapter, only where the native protocol supports it and the user explicitly selects a destination.
- A large GPX track should not be sent directly over LoRa. The interface offers a track summary for LoRa and reserves full GPX file exchange for a future authenticated Bluetooth companion channel.
- Bluetooth transport is a future development requirement, not a feature of the current browser simulator or a guarantee that both radio firmware implementations support it.
- The example coordinates and GPX metadata are fictional. The simulator does not access actual GNSS, retain recordings, or transmit anything.
- Design follow-ups include GPX import/export, track point simplification and bounds, POI category/icon management, offline basemaps, explicit location privacy, file integrity, transfer progress, interruption recovery, and battery impact.


## Live position sharing concept

The Navigation screen includes an explicit opt-in **Live share** configuration:

- Share with one selected contact or a selected group/channel; identify the chosen radio network.
- Configure a time interval (30 seconds to 30 minutes) and a displacement threshold (0 to 500 meters).
- Select **time OR movement** or **time AND movement** criteria. These are proposal-level semantics, not guaranteed by either protocol.
- Set a mandatory automatic stop duration (15 minutes to 8 hours), and pause/resume/stop explicitly.
- Keep sharing disabled by default; require a valid fresh GNSS fix and network-specific feasibility/airtime restrictions before any real transmission.
- Avoid broadcasting exact location to a public channel without a privacy warning and explicit consent. Group membership, encryption, and delivery guarantees are protocol-specific.

This browser UI only changes simulation state. It does not poll actual GNSS, schedule transmissions, or share coordinates. Firmware implementation must add transmission budgets, minimum intervals, deduplication, geodesic distance calculation, monotonic timers, persisted consent/session policy, and per-protocol encoding and capabilities.


For the consolidated implementation specification (including session intervals, movement thresholds, privacy, transport choices, and limits), see [Navigation and sharing](../navigation-and-sharing.md).
