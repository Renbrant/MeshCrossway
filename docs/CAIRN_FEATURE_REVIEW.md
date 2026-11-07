# CAIRN feature audit and adoption recommendations — 2026-11-07

**Source:** [cvhviz/CAIRN](https://github.com/cvhviz/CAIRN), public main branch and Cairn 127 release (2026-11-03). Reviewed [README](https://github.com/cvhviz/CAIRN/blob/main/README.md), [board feature matrix](https://github.com/cvhviz/CAIRN/blob/main/docs/features.md), [Wio L2 Pro board documentation](https://github.com/cvhviz/CAIRN/blob/main/docs/boards/wio-tracker-l2-pro.md), [ThinkNode M9](https://github.com/cvhviz/CAIRN/blob/main/docs/boards/thinknode-m9.md), [T-Deck](https://github.com/cvhviz/CAIRN/blob/main/docs/boards/lilygo-t-deck.md), [compact repeater](https://github.com/cvhviz/CAIRN/blob/main/docs/boards/compact-repeater.md), [map tile tool](https://github.com/cvhviz/CAIRN/blob/main/tools/MAPTILES.md), [releases](https://github.com/cvhviz/CAIRN/blob/main/docs/releases.md).

**Evidence boundary:** CAIRN's public repository contains releases, documents and a Python raster tile builder; the complete current embedded firmware source is **not publicly available in that repository**. Feature claims below are from its maintainers' documentation/release notes, not independently reproduced on MeshCrossway hardware. In particular, the device-side OpenFreeMap vector tile renderer, embedded performance costs, streaming implementation, scheduling and BLE interaction are not inspectable.

## Architecture contrast

CAIRN is **MeshCore companion firmware** with local LoRa PHY/MAC and on-board touchscreen. MeshCrossway is a **controller** with independent Meshtastic and MeshCore BLE connections, Wi-Fi, GNSS and SD, with **no bridging**. Therefore:
- **Local radio metrics:** CAIRN reads RF chip registers directly; MeshCrossway can show only fields explicitly available from each remote radio over its respective BLE API.
- **Radio action/administration:** each feature must be capability-gated **per protocol** (MC vs MT), and require confirmation when transmitting or changing settings. Never fabricate radio spectrum data or send undocumented BLE commands.
- **Power saving:** controller may dim screen/clock and pause map networking, but must preserve both BLE connections and user-selected GNSS/recording; CAIRN's local LoRa deep-sleep numbers do not translate directly.

## Feature inventory and feasibility

| CAIRN documented capability | MeshCrossway starting point | Feasibility and recommendation | Priority |
|---|---|---|---|
| On-device OpenFreeMap vector map, Light/Dark/Night styles | SD-backed PNG raster/map-worker; v0.10.0 design PR #154 | **Adopt in experimental v0.10.0**, using bounded MVT decoder, geometry streaming, shared memory budget, raster fallback. CAIRN renderer code unavailable. | P0 |
| On-screen offline area download with pause/resume across reboot | Cache of fetched raster tiles | **Adopt** journaled download queue on FAT32, atomic tile writes, disk-space checks and provider limits. | P0 |
| Pinch zoom, hide controls, GPS recenter, fit neighbors, GNSS data shortcut | Current map gestures/toolbar/GNSS | **Adopt selectively** after validating two-contact touch reports from actual hardware; no full frame rerender on each touch. | P0/P1 |
| Unknown/out-of-map state, map coverage preview, map set chooser | Map cached-missing indicator | **Adopt** coverage tiles metadata, helpful empty state and offline selection preview. | P1 |
| Repeater discovery and remote repeater administration | MC repeater/node access, issue #159 | **Adopt staged** passive repeater view first, then on-demand supported admin commands after MeshCore BLE capability audit. | P1 |
| Traffic log, detailed packet view (RSSI/SNR/hops/size/airtime) | Chats and node histories | **Adopt where protocol exposes fields**, with bounded ring buffer per network and explicit Unknown values; avoid unsolicited LoRa traffic. | P1 |
| Spectrum floor/peak/channel graph and full-band scan | Radio diagnostics limited by BLE APIs | **Conditional only**: can show exposure from radio telemetry; raw band sweep requires radio firmware support; not presently transferable by copying UI. | P3 |
| Native LoRa advert/telemetry/packet paths | Meshtastic and MeshCore separate BLE protocols | **Do not copy** local RF logic, protocol-level message queues or application interoperability without independent protocol validation. | N/A |
| Network diagnostics page, read-only browser live view | Existing local HTTP companion and memory diagnostics | **Adopt** a read-only diagnostics section in existing web UI (BLE status both, GNSS, Wi-Fi, SD, FPS, memory high-water), off by default if sensitive. | P1 |
| Battery graph, charger state and runtime history | AXP2101 battery/status | **Adopt after calibration**, 1h/24h/7d compact sample series, avoid falsely precise runtime estimate; no unbounded logs. | P1 |
| Power saver, screen timeout, hibernate | Brightness, timeout | **Adopt conservative staged changes** to LCD backlight, GNSS polling and map worker; never silently break dual BLE or track recording. | P1 |
| OTA firmware update with trial boot and automatic rollback | Web Flasher, immutable tested releases | **Investigate OTA partition/flash constraints first**, A/B update is compelling but new bootloader/partition changes are high risk; preserve USB recovery. | P2 |
| Atomic settings/contact save with power-loss resilience | SD/NVS persistence | **Adopt transactional writes**: temp + flush + rename where FAT32 safe; verify journal recovery and free-space error handling. | P1 |
| QR code contacts and onboarding | MC channel QR idea issue #160 | **Adopt contact QR first**, using verified MeshCore contact QR format; channel secret QR separate, explicit reveal. No claims of QR scanning on controller (no camera). | P2 |
| Color themes, compact settings tiles, contextual notices, quick replies and character/byte counter | Existing dark/orange UI and on-screen keyboard | **Adopt selectively**: larger map labels, quick reply templates, nonintrusive actionable notices; avoid full UI rewrite. | P2 |
| Blocks/mutes by contact/channel | Node and chat filters | **Adopt as local user preference**, distinguish local hide vs on-radio blocking; keep each network independent and expose unblock list. | P2 |
| Wi-Fi/BLE scanner visualizations and proximity radar | Wi-Fi scanner and dual BLE manager | **Deferred**, BLE scan can disrupt two active links; a conservative on-demand scan list is better than constant radar animation. | P3 |
| microSD file manager, Wi-Fi transfer, USB mass-storage | SD track browser/export | **Adopt limited backup/restore and storage status first**; USB MSC requires unmount/exclusive file access to prevent corruption, not initial priority. | P2 |
| Music player, MP3 EQ, voice recorder, IR, ambient sensors | No equivalent hardware/audio architecture | **Do not prioritize**: consumes flash, RAM, CPU and UI complexity; device capabilities vary. | P4 |
| Multi-board UI via input abstraction and per-board build/test matrix | Fixed Waveshare ESP32-S3 core; radio choice modular | **Adopt architecture idea**, not CAIRN firmware: input abstraction and capability-gated builds may support M9/T-Deck/ESP32-P4 after dual BLE/Wi-Fi assessment. | P2 |
| Trial/tested/preview release labels; version-specific checksum releases | Stable/recovery Web Flasher | **Adopt** transparent hardware-tested vs compiled-only labeling and model checks. | P1 |

## Offline mapping implementation details

The public Python tool `tools/maptiles.py` creates **raster** `.l2t` maps from PNG/other sources, with P8RLE/RGB565. It is **not** the same implementation as CAIRN 119+ embedded OpenFreeMap vector rendering. The documented `.l2t` files group 16×16 tiles and use 512-byte aligned sectors and a 256-entry tile index; these are useful concepts for SD I/O but not an MVT implementation and must not be treated as free-to-copy firmware.

- Current MeshCrossway PNG RGB565 cache: up to **12 × 256 × 256 × 2 = 1,572,864 bytes** PSRAM, plus decode buffers. For vector maps, **replace** that working set instead of allocating an additional equivalent cache.
- Stream compressed PBF/vector bytes to SD; validate size, decompressed cap, layer/feature count and geometry lengths before rasterization. Decode at most one tile and discard feature geometries after clipping to viewport.
- Reuse the existing map task and GUI frame path. Use small bounded glyph and label caches, static styles and clipping. Keep dual NimBLE/Wi-Fi/GNSS responsive. Profile frame time, free/largest internal DRAM and PSRAM, stacks and BLE reconnects while panning and downloading.
- Support day/night style without duplicating geometry/cache entries; keep cached data style-neutral.
- Use streaming writes and durable download state; verify CRC/length/atomic rename before considering a tile complete.
- OpenFreeMap [Terms of Service](https://openfreemap.org/tos/) updated 2026-10-14: **both** <=3,000 tiles **and** <=50 MB per user download, <=4 parallel requests and a unique app User-Agent; we recommend **one** request at a time initially. Do not split a single intended large area into fake separate downloads to bypass terms; seek explicit provider permission or support self-hosted tiles for larger areas.
- Honor attribution `OpenFreeMap © OpenMapTiles © OpenStreetMap contributors` as appropriate.

## Acceptance tests and dependencies

1. Two concurrent BLE radios remain connected during 30-minute map pan/download, Wi-Fi scan and SD stress; no task watchdog.
2. Memory peak vs stable baseline measured on real Waveshare ESP32-S3 and high-water stacks retained after frequent style switches.
3. Map download survives power loss, full FAT32 card, bad tiles, 429/401/403 responses and Wi-Fi reconnect; resumes without corruption.
4. No cross-network radio command, credential sharing or message forwarding.
5. Live diagnostics use bounded buffers, avoid leaking location, keys and identifiers to unauthenticated browser clients.
6. OTA accepted only with measured flash partition support, rollback and recovery validation; no speculative partition change to stable firmware.
7. All features surfaced in UI only after available BLE field/capability is confirmed for the corresponding network.

## Proposed order

- **v0.10.0 core:** vector map, downloads on device, touch gesture, memory/SD budget.
- **v0.10.x hardening:** coverage UX, diagnostics and maps performance, bounded packet metadata.
- **Future:** repeater overview, battery history, QR contact, OTA investigation and cross-hardware abstraction.
- **Deferred:** raw spectrum scanning without radio firmware capability, persistent BT radar, audio/IR/media features.

## Further reading

- [CAIRN repository](https://github.com/cvhviz/CAIRN)
- [CAIRN 127 release](https://github.com/cvhviz/CAIRN/releases/tag/cairn-v127)
- [Features by board](https://github.com/cvhviz/CAIRN/blob/main/docs/features.md)
- [Wio Tracker L2 Pro UX/features](https://github.com/cvhviz/CAIRN/blob/main/docs/boards/wio-tracker-l2-pro.md)
- [Offline raster map tool specification](https://github.com/cvhviz/CAIRN/blob/main/tools/MAPTILES.md)
- [OpenFreeMap service and TOS](https://openfreemap.org/tos/)
- [MeshCrossway OpenFreeMap PR #154](https://github.com/Renbrant/MeshCrossway/pull/154)
- [MeshCrossway repeater issue #159](https://github.com/Renbrant/MeshCrossway/issues/159)
- [MeshCrossway channel QR issue #160](https://github.com/Renbrant/MeshCrossway/issues/160)
