# MeshCrossway UI Prototype — Field Console

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

> GitHub's normal file view displays the source code rather than executing the HTML. The current demo is published through GitHub Pages.

## Static overview

![MeshCrossway UI prototype overview](ui-overview.svg)

## Important

This is a UI mockup only. It uses demo data and does not communicate with Meshtastic, MeshCore, GNSS hardware, storage, or the ESP32.

The prototype deliberately avoids claiming that queued messages were transmitted or delivered. The firmware implementation will need to map each protocol's real evidence into accurate message states.


## Selected bench hardware

The concept now targets a Waveshare ESP32-S3-Touch-LCD-3.5, two ESP32-S3/SX1262 LoRa V3 boards from the Aideepen two-pack (one per network), a Uputronics u-blox MAX-M10S (3 V variant) GNSS breakout with external antenna, and onboard microSD. The radio-side UART proposal is RX GPIO4 / TX GPIO5, separate from programming USB. Headers, board routing, central firmware and the MeshCore UART build remain unverified. See the [wiring and shopping guide](../hardware-wiring.md). This simulator does not connect to or validate those boards.


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
