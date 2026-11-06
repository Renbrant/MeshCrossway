# v0.10.0 vector-map memory and performance gates

**Scope:** Memory and performance limits for the v0.10.0 OpenFreeMap candidate. Host parsing is implemented; this document does not certify firmware build or device stability.

## Existing baseline (source audit)

- ESP32-S3 has 8 MiB external PSRAM; internal DRAM is shared by Wi-Fi, NimBLE, LVGL, FreeRTOS and drivers.
- Current PNG map tile cache in `meshcrossway_map_tiles.h`: 12 x 256 x 256 x 2 = **1,572,864 bytes** of decoded RGB565 PSRAM at maximum occupancy, excluding allocator metadata.
- Current frame buffers and PNG/RGBA decode scratch allocate additional memory; measure peak **during** decode, not only before/after.
- Map worker stack configured at 6144 bytes, core 0; touch task stack 3072 bytes, core 0; BLE initialization can defer map work.
- Existing `meshcrossway_memory_log.h` has opt-in `MCX_MEMORY_DIAGNOSTICS`, per-task stack minimum, internal/PSRAM heap, allocation-failure diagnostics. Avoid always-on expensive heap walking.

## Design rules

1. **Never allocate 12 raster RGB565 tiles and an equivalent vector cache simultaneously.** Choose one active renderer, or explicitly share a bounded arena.
2. **Keep raw PBF data on SD**; read at most one 512 KiB tile into PSRAM with bounded feature/command counts. The current endpoint's sampled tiles are raw PBF, so no gzip inflate buffer is allocated.
3. **Avoid a general-purpose JSON DOM for styles**. Parse TileJSON once, extract validated URL/attribution and use a fixed lightweight style/layer table. Glyph/sprite handling must have a bounded cache.
4. **Render only the viewport plus minimal margin**, not an entire offline area. Use tile-level clipping and discard geometry after drawing; do not retain every feature in RAM.
5. **One network request initially**. Reuse existing Wi-Fi/TLS plumbing where possible, but avoid shared blocking calls in BLE/LVGL tasks. Never allow background download to monopolize SDMMC.
6. **No new large internal-DRAM allocations.** Prefer SPIRAM for bulk tile payloads; reserve internal DMA memory for display/radio drivers. Fail gracefully if PSRAM reserve is insufficient.
7. **Pinch preview** scales existing frame while fingers move; recompute vector geometry only after the gesture settles. Verify dual-contact support on actual touch hardware first.
8. **No uncontrolled retries**; bounded exponential backoff and pause when radio initialization, SD errors or memory pressure occur.
9. **All map features are feature-flagged** until hardware tests pass; preserve PNG fallback and stable firmware image.
10. **Use streaming byte accounting** for OpenFreeMap offline operations. Both the 3,000-tile and 50 MB limits apply; <=4 concurrent requests, unique app User-Agent.

## Provisional budgets (engineering targets, not measured limits)

| Resource | Target | Gate |
| --- | --- | --- |
| Raw tile buffer | <=512 KiB per request | Reject/skip oversized tile safely; sample z14 was 424,280 bytes |
| Geometry scratch | <8 KiB PSRAM | One 512-point/scanline workspace, reused for each feature and tile |
| Vector decoded feature retention | 0 across tiles by default | Process/draw/discard one tile at a time |
| Vector glyph/sprite cache | <=256 KiB PSRAM initially | Bounded LRU |
| Total incremental vector working set | <=1 MiB PSRAM target | One tile + frame + geometry; measure provider switching and old/new frame overlap |
| Network parallelism | 1 initially | Never exceed provider maximum 4 |
| Map task internal heap allocation | No unbounded allocation | Allocation failure must not crash UI/BLE |
| LVGL thread safety | No direct LVGL calls from map/download worker | Queue and apply with LVGL lock |
| BLE + GNSS | No lost connectivity/watchdog under downloads | Hardware soak test |

The limits above are **starting guardrails**; change only with measured peak-memory, render-time and BLE/SD contention data.

## Instrumentation required

Log timestamp, source/zoom, compressed bytes, inflated bytes, features decoded, vertices drawn, frame render ms, tile cache hit/miss, PSRAM free/minimum, internal DRAM free/minimum, largest free block, per-task stack minima, Wi-Fi download bytes and retry count. Avoid storing raw map data or user position in diagnostic logs by default.

## Benchmark procedure

1. Capture v0.9.x raster baseline on identical hardware with both radios connected, Wi-Fi on, GNSS active, SD FAT32, remote viewer idle and active.
2. Repeat cold-cache and warm-cache tests at urban, rural and dense downtown views, zoom levels 8/12/14; repeat with SD nearly full.
3. Test first map, repeated pan, pinch, 30-minute offline navigation, background region download, power-cycle/resume, low battery and radio reconnect.
4. Record PSRAM and internal heap minimum and largest free block before, during and after each scenario.
5. Fail the build/release gate for watchdog reset, radio disconnection attributable to maps, persistent memory leak, SD corruption or loss of fallback.
6. Do not claim acceptable FPS/latency until real-device measurements exist.

## Build and publication policy

The PR may contain development sources and host tests, but **do not publish v0.10.0 as installable** until the firmware contains a working renderer/downloader, CI ESP-IDF build succeeds, version metadata matches the binary and basic device testing is complete. Stable releases remain immutable.
