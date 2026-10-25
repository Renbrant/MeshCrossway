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
