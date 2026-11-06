# OpenFreeMap integration — v0.10.0 candidate

**Status:** Implemented in the v0.10.0 candidate source. GitHub Actions compiled the ESP-IDF firmware and passed the archive preflight; physical-device acceptance is still pending. **v0.9.28 remains the recommended stable firmware.**

## What is integrated

- `Map Options` now offers **Vector / OpenFreeMap (SD cache)** alongside existing raster providers. The legacy PNG path remains intact.
- The firmware requests OpenFreeMap PBF vector tiles over HTTPS and draws them with a small bounded MVT reader and a built-in color palette for water, land, buildings, transport, boundaries and place points. This is not a full MapLibre renderer; it does not yet draw text labels, honor provider style JSON, or implement collision handling.
- Tile payloads are capped at 512 KiB and held one at a time in PSRAM. Rendered features are discarded after each tile. Downloaded tiles are validated before an atomic write to `/sdcard/mcx-map-vector/latest/{z}/{x}/{y}.pbf`.
- Viewed tiles are cached automatically on the SD card. **Online OFF** displays cached tiles only. The vector source is limited to zoom 14, matching OpenFreeMap's published maximum.
- The map worker remains separate from LVGL, yields between tiles and aborts work when the requested view changes or BLE initialization takes priority. OpenFreeMap attribution is shown in the map and credits view.

## Deliberate first-candidate limits

This implementation does **not** yet provide a user-selected rectangular offline-area download, a progress/pause/resume download manager, version-pinned TileJSON metadata, place labels, gzip inflation, pinch zoom, or multi-tile pack indexing. It only retains tiles the user has already viewed. A requested bulk-download flow must apply OpenFreeMap's per-operation ceiling of 3,000 tiles **and** 50 MB; it must not evade those limits by splitting requests. The current interactive viewport fetches a small number of visible tiles at a time.

Cached tile data is grouped by the `latest` endpoint and can become older than the service's current dataset. Clearing `/sdcard/mcx-map-vector` removes this cache if the card is manually inspected; no in-app cache reset is provided yet.

## Checks and measurements

- Host test `tests/ofm_mvt_test.cpp`: valid point tile rendering, truncated protobuf rejection and payload-size rejection.
- A dense Denver PBF tile from OpenFreeMap (z10/x213/y388, 120,853 bytes) parsed successfully with the host renderer during development.
- CI built the v0.10.0 ESP-IDF image with the pinned BSP. Before publishing as a hardware-tested release, verify on the device: map render in online/offline modes; cold/warm SD behavior; zoom 14; maximum PBF; SD absent/full; Wi-Fi loss, 404/429/5xx and cancellation; BLE reconnect and messages during pan; internal/PSRAM minima, touch latency and watchdog stability.

The Web Flasher candidate must stay **experimental** until those device checks are completed. Do not alter the immutable v0.9.28 stable image.
