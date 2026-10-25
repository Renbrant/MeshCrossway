# MeshCrossway UI Prototype v0

This folder contains a browser-based UI concept for the 3.5-inch MeshCrossway touchscreen.

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

> GitHub's normal file view displays the source code rather than executing the HTML. GitHub Pages can be enabled later if we want a permanent hosted demo URL.

## Static overview

![MeshCrossway UI prototype overview](ui-overview.svg)

## Important

This is a UI mockup only. It uses demo data and does not communicate with Meshtastic, MeshCore, GNSS hardware, storage, or the ESP32.

The prototype deliberately avoids claiming that queued messages were transmitted or delivered. The firmware implementation will need to map each protocol's real evidence into accurate message states.


## Selected bench hardware

The concept now targets a Waveshare ESP32-S3-Touch-LCD-3.5, two ESP32-S3/SX1262 LoRa V3 boards from the Aideepen two-pack (one per network), a SparkFun SAM-M10Q Qwiic GNSS, and onboard microSD. The radio-side UART proposal is RX GPIO4 / TX GPIO5, separate from programming USB. Headers, board routing, central firmware and the MeshCore UART build remain unverified. See the [wiring and shopping guide](../hardware-wiring.md). This simulator does not connect to or validate those boards.


## Navigation and sharing concept

The Navigation screen now includes interactive **POIs**, **saved tracks**, **track recording**, and a **share transport** choice.

- Compact waypoint/POI location messages can be represented through a chosen mesh network adapter, only where the native protocol supports it and the user explicitly selects a destination.
- A large GPX track should not be sent directly over LoRa. The interface offers a track summary for LoRa and reserves full GPX file exchange for a future authenticated Bluetooth companion channel.
- Bluetooth transport is a future development requirement, not a feature of the current browser simulator or a guarantee that both radio firmware implementations support it.
- The example coordinates and GPX metadata are fictional. The simulator does not access actual GNSS, retain recordings, or transmit anything.
- Design follow-ups include GPX import/export, track point simplification and bounds, POI category/icon management, offline basemaps, explicit location privacy, file integrity, transfer progress, interruption recovery, and battery impact.
