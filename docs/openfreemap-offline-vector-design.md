# OpenFreeMap offline vector maps — design and implementation plan

**Status:** Design plus first firmware integration on the v0.10.0 experimental branch. The renderer and view-tile SD cache are implemented; offline-area download management, TileJSON pinning, labels and hardware performance acceptance remain open. See [v0.10.0 implementation notes](openfreemap-v0100.md).
**Goal:** All map downloads happen on the MeshCrossway controller via Wi-Fi and microSD. No website, desktop generator, cloud service, or mandatory account.
**Compatibility:** Keep the existing PNG/XYZ map path and current stable firmware unchanged until hardware acceptance.

## Source audit (2026-11-06)

- OpenFreeMap public vector tile service: https://openfreemap.org/ ; upstream https://github.com/hyperknot/openfreemap
- Official ToS (updated 2026-10-14): https://github.com/hyperknot/openfreemap/blob/main/website/content/policies/tos.md
- Each **offline download operation** must stay at or below **3,000 tiles AND 50 MB**, with at most **4 parallel requests**, and a unique application-identifying **User-Agent**. Larger downloads require prior contact with info@openfreemap.org. Do not split a larger requested download into batches merely to evade these limits. Policy is subject to change; check before release.
- OpenFreeMap TileJSON: https://tiles.openfreemap.org/planet/latest ; vector tiles use the advertised URL template, currently /planet/latest/{z}/{x}/{y}.pbf. Do not hardcode a server URL without validation against TileJSON.
- OpenFreeMap styles are MapLibre style JSON; rendering full MapLibre GL on ESP32-S3 is **not** assumed viable.
- OpenFreeMap does not supply routing, elevation, raster tiles, geocoding or satellite imagery.
- CAIRN reference: https://github.com/cvhviz/CAIRN/blob/main/docs/boards/wio-tracker-l2-pro.md ; from Cairn 119, long-press Map -> Download area, background Wi-Fi transfer, pause/resume after reboot, vector style selection and cache.
- CAIRN tool: https://github.com/cvhviz/CAIRN/blob/main/tools/maptiles.py ; MVT format 3 stores fetched compressed vector records, distinct from P8RLE raster format 1. Its embedded renderer implementation is not public in the inspected repository. Avoid claiming drop-in firmware compatibility.

## Current MeshCrossway audit

- `firmware/display-prototype/meshcrossway_map_tiles.h` reads/downloads PNG XYZ tiles, decodes using LodePNG, caches up to 12 decoded RGB565 tiles (about 1.5 MiB PSRAM), and builds a complete frame. The v0.10.0 candidate adds a separate OpenFreeMap PBF path with one-tile-at-a-time SD caching and a bounded renderer.
- `firmware/display-prototype/meshcrossway_touch_input.h` currently requests one touch point (`esp_lcd_touch_get_coordinates(..., 1)`). Two-point touch capability and coordinate transformations need hardware validation before implementing pinch.
- ESP32-S3, 8 MB PSRAM, 480x320 SPI display, LVGL 8, independent Meshtastic and MeshCore BLE sessions, GNSS, SDMMC 1-bit FAT32.
- Keep BLE responsive and preserve GNSS/tracks/node overlays. Do not change map configuration NVS struct in place; use a versioned extension/namespace with migration.

## Target UX

1. Open Map, long-press empty map background -> **Download offline area**.
2. Show rectangular region overlaid on map; user pans/zooms and optionally adjusts area.
3. Show source OpenFreeMap, zoom min/max, estimated tile count and estimated data size, available SD space, Wi-Fi state and required attribution.
4. If request exceeds either official limit, refuse and offer a smaller selection; no automatic circumvention by partitioning.
5. Start download; progress shows completed tiles, bytes, elapsed time, errors, pause/resume/cancel. Background task yields to radio, LVGL and GNSS.
6. On completion, show **Available offline**; cache and online fallback work with no manual SD transfer.
7. Existing raster map remains a selectable fallback. Offline maps never require Wi-Fi to display.

## Technical modules (current state)

| Module | Responsibility |
| --- | --- |
| Tile math | Existing Web Mercator projection and prototype tile math; no bulk-region selector yet |
| OpenFreeMap source | HTTPS latest tile endpoint and attribution; TileJSON discovery/version pinning remain open |
| MVT store | Atomic FAT32 individual files, 512 KiB cap, SD free-space reserve; no index/checksum/eviction yet |
| Download manager | Interactive viewport tiles only, one request at a time, cancellation on new view, retry backoff; no pause/resume region job or bulk-operation meter yet |
| MVT decoder | Bounded raw PBF protobuf parsing; no gzip inflate, bounded feature/command counts |
| Vector renderer | Water/land/building/transport/boundary colors, simple point symbols, scanline polygon fill and line strokes; labels and full style JSON remain open |
| `mcx_map_gestures` | Two-finger samples and pinch preview; defer expensive rerender until gesture settles |
| `mcx_map_ui` | Download area, progress, cancellation, map style, attribution and offline indicator |

### File format choice

Prefer a **native vector tile cache** of raw fetched gzip/PBF bytes, not PNG conversion. Store source metadata, tile z/x/y, compressed length, checksum and dataset/style version. Preserve original compression when present; do not recompress on ESP32. Atomic temp-file rename and a small journal protect against reset/power loss. For FAT32 performance, prototype grouped files/index rather than millions of individual tiny files. Consider CAIRN L2T MVT blocks only after documenting compatibility, license notices and benchmark; do not assume an undocumented embedded reader is available.

### Download policy / network safety

- Confirm the official ToS before shipping. Limit **per requested offline operation** to <=3,000 tiles AND <=50,000,000 bytes; use a conservative estimate and stop before crossing the byte cap. Limit active connections to 1 initially (never >4).
- Identify requests with a unique `User-Agent: MeshCrossway/<firmware-version> (offline maps; https://github.com/Renbrant/MeshCrossway)`. Do not include personal identifiers.
- HTTPS with certificate verification, timeouts, response-size caps, Content-Type and gzip/protobuf validation, redirect restrictions to approved hosts, rate limiting and exponential backoff. Honor HTTP 429/Retry-After.
- An already cached tile should not be re-fetched; avoid duplicate concurrent requests. Attribution must remain visible.
- Never use `tile.openstreetmap.org` for offline bulk download. Source configuration and map data licenses are separate from application source license.

### Rendering strategy and constraints

MVT encodes geometry and attributes; styles, glyphs and sprites are separate. First support roads, water, landuse and basic labels from an explicitly mapped subset of OpenMapTiles layers. Text shaping, icon atlases, style JSON interpretation, polygon winding and clipping require separate acceptance tests. Rendering is not a simple swap of PNG decoder.

Use a bounded PSRAM arena; avoid full-map vector retention at all zooms. Render visible tiles in a worker, update LVGL only under its lock, and keep radio task priority higher. Pre-render during idle or reuse previous frame during pan/pinch. Never block BLE scan/reconnect or map touch sampler on network I/O.

### Pinch-to-zoom dependency

Current touch sampler requests one coordinate. Verify the exact Waveshare touch controller and ESP-IDF driver support for two contacts on hardware. Add a two-contact event type while preserving single-touch queue semantics; LVGL 8 pointer input alone is not a complete multitouch pinch implementation. If the controller/driver only reports one contact, provide smooth one-finger drag and on-screen zoom as fallback.

## Incremental work packages

- **P0:** architecture and provider-policy research, tile math and host tests.
- **P1 (v0.10.0 candidate):** bounded raw PBF decoder/renderer, interactive-view SD cache, offline-only display mode and host tests. Physical memory/latency measurements remain pending.
- **P2:** explicit offline-area selector, strict 3,000-tile/50-MB byte accounting, progress/cancel/pause/resume journal, cache indexing and TileJSON version pinning.
- **P3:** labels/style improvements and comparison with the PNG baseline on cold/warm SD caches.
- **P4:** pinch preview, label placement, style options, attribution, GNSS/POI/track overlays, cancellation and stress tests.
- **P4:** hardware validation and performance evidence before changing the feature's experimental status.

## Acceptance criteria / test matrix

- The v0.10.0 source is experimental; v0.9.28 remains recommended stable. The production Flasher candidate is not hardware-certified.
- Tile math: poles, longitude wrap, antimeridian, z=0, z=14, bounds, integer overflow, exact counts.
- Downloader: 429/5xx/backoff, 404, truncated/gzip corrupt, TLS failure, Wi-Fi loss, SD removal/full, brownout, resume, cancel, no duplicate downloads, tile/byte caps.
- Renderer: malformed protobuf, huge feature counts, polygons crossing tile edge, label collisions, 480x320 display, light/dark palette, missing glyphs.
- Performance targets (to measure, not claims): BLE remains connected and messages delivered; no watchdog resets; PSRAM headroom and touch latency comparable to raster baseline; first map and pan/pinch times logged.
- Radio independence: neither BLE connection is bridged or affected by map download.
- FAT32 cards: verify 32 GB; gracefully reject exFAT; test corrupted filesystem and read-only conditions.

## Decisions still gated on measurement

- Native MVT store vs CAIRN L2T format-3 packaging.
- Tiny purpose-built vector renderer vs portable library with ESP-IDF footprint.
- Whether 480x320 SPI bandwidth permits real-time vector redraw or needs scaled preview.
- Whether the actual touch controller reports two points.
- Whether source TileJSON/style/glyph endpoints can be consumed without violating provider terms.

**This document is a design, not a claim that OpenFreeMap vector rendering or offline downloads already work on MeshCrossway.**
