# Map Optimization Knowledge Base

**Project:** MeshCrossway  
**Document type:** Living engineering reference and experiment backlog  
**Created:** 2026-11-07  
**Status:** Research and test planning only — no optimization proposed here is considered implemented or validated unless explicitly marked as existing.

## 1. Purpose and working rules

This document centralizes methods for improving **map loading speed, offline storage, readability, responsiveness, and memory use** on the MeshCrossway ESP32-S3 controller. It is organized by **methodology**, not by firmware release or external project. It should grow as experiments are performed, retaining the outcomes of unsuccessful approaches as well as successful ones.

Rules:

1. Distinguish **EXISTING**, **CANDIDATE**, **DEFERRED**, and **REJECTED** solutions. A candidate is not an observed performance improvement.
2. Preserve the modular architecture: one display controller and independent Meshtastic and MeshCore radios over BLE; **no network bridging**.
3. Prioritize real performance on hardware, not nominal compression ratios or desktop-only benchmarks.
4. Make changes incrementally; compare with the same known baseline and preserve a rollback firmware.
5. Record actual measurements, software versions, input data, and hardware conditions before promoting a change.
6. Prefer a common tile interface over redesigning the existing renderer for each new file format.

## 2. Hardware, architecture, and existing baseline

### 2.1 Constraints

| Component | Relevant constraint |
|---|---|
| Controller | Waveshare ESP32-S3 Touch LCD 3.5-inch, 480 × 320 landscape |
| Memory | 8 MiB external PSRAM; internal heap also serves BLE, DMA, Wi-Fi, TLS, and task stacks |
| Storage | microSD via SDMMC 1-bit; FAT32 confirmed on a 32 GB card |
| UI | LVGL 8, responsive touchscreen, track/POI and node overlays |
| Connectivity | Concurrent BLE links to Meshtastic and MeshCore radios, plus optional Wi-Fi AP/STA and HTTPS |
| GNSS | GPS position, tracks, POIs, map centering, and moving markers |
| Firmware reference | v0.9.32 is an experimental raster-map candidate; v0.9.28 is identified as recommended stable in the firmware README at the time this document was created |

**Memory math (not measured runtime usage):**

- One 256 × 256 RGB565 tile = 131,072 bytes (128 KiB).
- Twelve such decoded tiles = 1,572,864 bytes (1.5 MiB), excluding metadata and allocator overhead.
- One 256 × 256 RGBA decode buffer = 262,144 bytes (256 KiB).
- A hypothetical full-screen 480 × 320 RGB565 frame = 307,200 bytes (300 KiB). The actual map viewport may be smaller.
- The existing online tile limit is 256 KiB **per compressed PNG**; this is not the same as decoded memory size.

### 2.2 Already implemented — do not reimplement by accident

- **EXISTING:** 256 × 256 PNG raster tiles with conventional XYZ addressing and Web Mercator projection.
- **EXISTING:** Offline tile directories under `/sdcard/tiles/{z}/{x}/{y}.png` and a provider-specific online cache under `/sdcard/mcx-map-cache/<source-hash>/{z}/{x}/{y}.png`.
- **EXISTING:** Dedicated, low-priority map worker, separate from LVGL drawing; a 6,144-byte worker task stack is configured in the map tile header.
- **EXISTING:** Bounded worker-owned LRU of **12 decoded RGB565 tiles in PSRAM**, and counters for cache hits, decode count, and last render duration.
- **EXISTING:** HTTPS download limits/backoff, bounded PNG validation/decode, temporary-file cache commits, SD free-space reserve, cancelation of obsolete map requests, and source-specific disk cache directories.
- **EXISTING:** LVGL allocations directed to PSRAM where appropriate; overlays use bounded PSRAM structures rather than one LVGL object per node.
- **EXISTING:** Pan preview remains interactive during tile loading; map/track coordinate caching avoids redundant projection; unnecessary invalidation is suppressed.
- **EXISTING:** Map grouping can be enabled/disabled and has host tests with up to 640 node-marker slots.
- **EXISTING:** Day/night appearance can reuse the same raster tiles.
- **EXISTING (v0.9.32 candidate):** Two-finger pinch zoom and a saved **1.25× larger map** option. That option magnifies the **entire** rendered raster, including roads, labels, and overlays, and crops the visible extent. It does **not** create larger typography inside source tiles and does not reduce download size.

Implementation references: [map tile worker](../firmware/display-prototype/meshcrossway_map_tiles.h), [LVGL allocator](../firmware/display-prototype/meshcrossway_lvgl_alloc.h), [map/GNSS design](maps-gps-v081.md), [render latency](map-render-latency-v087.md), [node grouping](map-grouping-v0924.md), [map controls/night mode](map-controls-v0925.md), [map UI v0.9.31](map-ui-v0931.md), and [TLS/DMA investigation](map-tls-dma-v0923.md).

## 3. Decision matrix — candidate methodologies

| ID | Methodology | Primary benefit to investigate | Main cost/risk | Priority |
|---|---|---|---|---|
| M01 | Indexed, single-file offline tile archives | Fewer FAT32 directory/file operations and easier distribution | New reader, index, corruption handling | High |
| M02 | PMTiles v3 raster reader | Portable, documented offline archives | Directory/Hilbert lookup and decompression | High |
| M03 | MBTiles raster reader | Compatibility with QGIS/GIS ecosystems | SQLite footprint, pages/cache and TMS row conversion | Medium |
| M04 | QOI or other alternate raster encoding | Potentially lower decode CPU/time | Conversion pipeline, file size, decoder and RAM | Medium |
| M05 | Two-level RAM/SD tile cache tuning | Fewer decodes and SD accesses | PSRAM pressure, cache pollution | High |
| M06 | Smarter compositing and invalidation | Lower latency and improved touch response | Stale frames or graphical artifacts | High |
| M07 | Raster label readability and UI scaling | More legible streets at 480 × 320 | Blur, reduced visible extent, data-set complexity | High |
| M08 | SD/FAT32 layout and sequential I/O | Better offline loading on inexpensive cards | Complex tooling, fragmentation, wear | Medium |
| M09 | Online fetch scheduling and cache integrity | Faster and more reliable interactive loading | Provider limits, TLS/DMA contention | Medium |
| M10 | Offline pack generation and distribution | Easier user setup and provider choice | Tile licenses and pack size | Medium |
| M11 | Vector rendering as a separate research track | Independent text sizing and style control | Major CPU, RAM, glyph, and pipeline complexity | Deferred |
| M12 | Performance instrumentation and regression harness | Objective decisions and safe changes | Engineering effort; diagnostic overhead | Required for every experiment |

## M01. Indexed single-file archives for raster tiles

**Status:** CANDIDATE.  
**Goal:** Replace thousands of small offline files with fewer large files while retaining ordinary PNG payloads and the current raster renderer.

**Method:** Generate a file containing a magic/version header, a bounded index mapping `(z, x, y)` to `(offset, length)`, and the original tile PNG byte streams. The device performs indexed lookup, seeks to the requested offset, reads only that PNG, and feeds the existing decoder.

**Possible layouts:**
- One archive per map region and zoom band (limits index size and reduces impact of damage).
- One file per zoom or geographic segment.
- A sorted compact index, optionally accompanied by a small manifest with bounds, zoom limits, provenance, attribution, and checksums.

**Design requirements:**
- No full archive extraction and no full index loading into internal RAM.
- Explicit maximums for file size, tile length, index count, offset arithmetic, and allocator use; validate offsets against file length.
- Cache a small bounded part of the index or use binary search when possible.
- Reject truncated/invalid files safely; fall back to other tile stores.
- Provide a reproducible desktop packer and verifier **before** writing the embedded reader.
- FAT32 has an approximately **4 GiB minus 1 byte per-file limit**; split large world/regional archives accordingly.
- A raw indexed pack **does not automatically reduce PNG payload bytes**. Expected gains are predominantly fewer filesystem metadata operations, easier transfers, and potentially better access locality; actual speed is unproven.

**Measure:** random and sequential seek time, p50/p95 cold-tile load latency, package byte size, file count, index RAM, cache hit ratio, corruption recovery, and migration effort.

**Go/no-go:** Keep only if SD access and real map responsiveness improve materially without threatening BLE/UI memory budgets. Prefer a documented standard if comparable.

## M02. PMTiles v3 offline raster reader

**Status:** CANDIDATE / preferred **standardized archive format to investigate**.  
**Goal:** Support one portable offline archive without adding a full database engine.

PMTiles v3 has a 127-byte header, root directory, optional leaf directories, metadata, and tile data; tiles are addressable by an index based on a Hilbert-curve tile ID. The root directory is contained within the first 16 KiB. It supports PNG tile payloads, among other types. The directory sections may be compressed independently of tile data.

**Proposed adapter:**
1. Add a read-only `TileStore`-style interface: `get(z, x, y) -> bounded tile bytes + provenance`.
2. Read and validate the header and zoom/bounds before allocation.
3. Convert XYZ coordinates to the specified tile ID; locate the directory entry with bounded index/decompression buffers.
4. Seek to the tile bytes. **Initial scope: PNG raster only**, without vector rendering or new image formats.
5. Feed bytes to the existing strict 256 × 256 PNG validation/decode path.
6. Cache recently used root/leaf index data where useful; measure the PSRAM cost.
7. Allow fallback to the existing offline XYZ and online cache paths, with deterministic precedence.

**Risks:** overflow in 64-bit offsets, malformed varints or directory lengths, directory compression overhead, complex index lookups on slow SD cards, and large archives on FAT32.

**Do not assume:** that adopting PMTiles automatically makes a PNG smaller, reduces image decode time, or provides writable per-tile cache updates. It is principally a **read-oriented archive**. Retain loose PNG files for incremental online cache writes unless measurements justify something else.

**Reference:** [PMTiles v3 specification](https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md), [PMTiles project](https://github.com/protomaps/PMTiles).

## M03. MBTiles / SQLite offline maps

**Status:** CANDIDATE / comparison baseline.  
**Goal:** Evaluate an interoperable file format widely supported by desktop map tools.

MBTiles stores tile data and metadata in a SQLite database. QGIS can generate an MBTiles raster tileset. An embedded reader would open the DB read-only and retrieve a tile by zoom/column/row.

**Important compatibility detail:** standard MBTiles tile rows are commonly stored in **TMS (bottom-origin)** coordinates, whereas the current runtime requests **XYZ (top-origin)** coordinates. For zoom `z`, transform the row as `(2^z - 1 - y)` when required by the actual dataset. Test multiple zoom levels and hemispheres; do not silently assume the convention.

**Tradeoffs:**
- Pros: interoperable tooling, one file, established indexes, extensible metadata.
- Cons: SQLite binary and page cache cost, file-lock/read behavior, page reads, higher implementation footprint than a minimal archive reader.
- Compare p95 access time, binary growth, internal heap/PSRAM high-water marks, SD read count, and failure behavior with identical PNG tiles.

**Reference:** [MBTiles specification](https://github.com/mapbox/mbtiles-spec), [QGIS raster tile generation](https://documentation.qgis.org/3.44/en/docs/user_manual/processing_algs/qgis/rastertools.html).

## M04. Tile encoding and decode-time tradeoffs

**Status:** CANDIDATE.

The current pipeline receives PNG, validates dimensions, decodes to RGBA, and keeps RGB565 tiles in a bounded cache. **Compressed file size, decode time, PSRAM peak, and visual quality are separate metrics.**

| Encoding | Possible benefit | Caveat |
|---|---|---|
| PNG (baseline) | Lossless, existing full compatibility | Deflate/decode CPU and temporary RGBA buffer |
| QOI | Simple lossless decoder; potentially faster decode | Often larger files than PNG, so SD/transfer cost may offset decode gains |
| JPEG | Small photographic tiles | Lossy street text and symbols; new decoder/visual artifacts |
| WebP / AVIF | Potentially smaller packages | Decoder complexity and memory cost on ESP32-S3 |
| Raw RGB565 | No image decompression on load | 128 KiB per tile; archive/SD bandwidth grows sharply |

**QOI experiment:** Convert the exact same licensed source tiles offline, retain the baseline PNGs, then measure **end-to-end tile availability**, not only a microbenchmark of the decoder. Check QOI implementation's allocation behavior: a small C header does not guarantee streaming or low peak memory. Consider a direct or scanline-based RGB565 path if supported, without unbounded intermediate RGBA allocations.

Avoid generating QOI/other files on the controller just to save them back to SD unless a measured use case warrants CPU, wear, and memory cost.

**Reference:** [QOI source/specification](https://github.com/phoboslab/qoi).

## M05. Multi-level caching and capacity tuning

**Status:** EXISTING foundations; CANDIDATE tuning.

Proposed lookup order:

    RAM decoded-tile LRU
        -> optional packed offline archive OR existing offline XYZ
        -> persistent online PNG cache
        -> permitted live HTTPS request
        -> explicit missing-tile placeholder

This is an **evaluation model**, not a claim that every optional source is currently supported.

Test the existing 12-slot RGB565 RAM LRU against smaller/larger configurations (e.g., 6 / 12 / 16), keeping all other inputs fixed. Track total PSRAM allocation, peak transient memory, eviction rate, redecodes, and BLE/TLS reliability. More cached tiles are **not always better**: they may starve concurrent map frames, tracks, BLE processing, or TLS allocations.

Candidate improvements:
- Prefer tiles needed for the current viewport over speculative work.
- On rapid zoom/pan, invalidate stale requests and avoid caching no-longer-useful images at the expense of the active view.
- Preserve cache keys that include provider/source, zoom, X, Y, and decode-relevant style semantics.
- Keep night/color transformations separate from source PNG storage when possible.
- Instrument SD cache hit/miss and decoded RAM hit/miss independently.
- Add a bounded, explicit offline archive selection mechanism rather than scanning the entire SD tree for every tile.

**Guardrail:** do not add automatic cache purges or rewrite existing offline files without an explicit design for retention, user-owned data, recovery, and provider policy.

## M06. Render composition, clipping, and invalidation

**Status:** EXISTING foundations; CANDIDATE improvements.

Preserve:
- A worker for SD/network/decode and LVGL-only UI mutations on the UI task.
- Bounded ownership transfer of finished frames.
- Generation IDs/cancelation of obsolete jobs.
- Pan preview while new tiles arrive.
- Reprojection of tracks/POIs only when geometry actually changes.
- Partial redraw/clipping of overlay geometry where applicable.
- Bounded node-group previews and avoidance of per-node LVGL widgets.

Test additional ideas **only after profiling**:
1. Dirty-region tracking: redraw only the affected viewport rectangle or overlay layer.
2. Differential overlay refresh: when just a node changes, do not recompose every raster tile.
3. Separate raster-base generation from marker/track generation so GPS status changes do not force background recomposition.
4. Avoid unnecessary re-decode when navigating between adjacent viewports.
5. Assess whether tile row/stripe compositing reduces peak transient frame buffers without causing visible seams.

**Risks:** stale overlays, inconsistent transforms, tearing/stripes, lost attribution, and increased synchronization complexity. The display uses partial updates, so "fewer CPU cycles" does not by itself prove smoother visual output.

## M07. Map readability, raster text, and zoom

**Status:** EXISTING 1.25× whole-map option; CANDIDATE improvements.

**Fundamental constraint:** Street names in PNG raster tiles are already pixels. The runtime cannot cleanly enlarge *only* their fonts without a different tile style or rendering labels itself. Reducing tile resolution does **not** inherently enlarge street names; it may simply blur or remove detail.

Methods to compare:
- **A. Whole-frame nearest/bilinear scaling:** existing 1.25× approach. Assess label size, crispness, CPU time, and how much visible geography is lost.
- **B. Differently styled raster tile set:** generate licensed offline tiles with larger road-name fonts, stronger contrast, and carefully chosen label density, then consume them as normal 256 × 256 PNGs. No new renderer required.
- **C. Source resolution / tile density variants:** compare the semantics of 256-pixel versus high-DPI assets and their logical tile size, without confusing pixel dimensions with geographic zoom. Never assume 512-to-256 downsampling improves text size.
- **D. Overlay-only accessibility:** enlarge MeshCrossway node labels, POIs, track annotations, and UI controls independently of the raster base.
- **E. Vector labels:** possible long-term solution, tracked in M11, not a small raster tuning change.

Use fixed test scenes: dense city center, suburban roads, rural maps, topographic maps, mixed MT/MC markers, day/night mode, and bright outdoor conditions. Compare text readability at normal viewing distance, not just screenshots magnified on a desktop.

Acceptance must include accurate tap/marker projection when a visual scaling transform is active and correct pinch behavior.

## M08. FAT32 / microSD storage and I/O patterns

**Status:** CANDIDATE benchmarking.

Thousands of tiny files may create directory metadata overhead and inconvenient PC-to-SD transfers. A packed file may improve both, but results depend on FAT32 allocation, card controller, fragmentation, and access pattern.

Benchmark:
- Existing loose `z/x/y.png` files versus a read-only indexed archive containing the identical PNG bytes.
- Cold random tile loads versus contiguous nearby tile loads.
- A freshly formatted FAT32 card versus a realistically used/fragmented card.
- At least two microSD models/speed classes if available, recording actual SDMMC 1-bit throughput.
- File open/close count, FAT directory traversals, seek/read syscall timing, total bytes read, and worst-case stalls.

Use buffered reads and aligned/chunked I/O only when measurements show an improvement. Do not assume that bigger buffers or read-ahead always improve latency; they can consume scarce memory and compete with other real-time tasks.

Test SD removal, reinsert, full media, interrupted writes, invalid filenames, path traversal, and archive corruption. Never format user media as an automatic recovery action.

## M09. Online requests, cancellation, and source compliance

**Status:** EXISTING protective behavior; CANDIDATE scheduling refinement.

Retain HTTPS certificate/hostname verification, request rate limits, retry/backoff behavior, per-provider cache isolation, atomic temp-file replacement, PNG validation, and the free-space reserve. Remember the hardware AES/DMA allocation regression documented in [map TLS/DMA investigation](map-tls-dma-v0923.md); do not increase concurrent TLS load merely to accelerate tiles.

Potential experiments:
- Prioritize the center/current viewport during user pan or zoom.
- Cancel or deprioritize stale generations early.
- Coalesce requests for the same tile.
- Avoid repeated failed requests to the same unavailable tile/provider.
- Compare modest request scheduling changes with **real** BLE traffic, Wi-Fi, and map interaction active.
- Respect source cache headers, attribution, rate limits, privacy expectations, and licensing.

**Critical service-policy distinction:** the public OpenStreetMap raster tile service prohibits bulk downloads and offline prefetching. Interactive viewport requests with compliant caching are different from generating an offline city pack. Offline archive creation **must use a source that explicitly permits it**, licensed data rendered locally, or self-hosted tiles. Never point an automated pack generator at the public OSM tile endpoint.

**Reference:** [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/).

## M10. Offline generation, packaging, and update workflow

**Status:** CANDIDATE user workflow.

Target user experience:
1. Select region, zoom range, map theme, and licensed tile source **on a computer**.
2. Generate ordinary raster XYZ tiles with QGIS or an equivalent tool.
3. Optionally package them into a verified PMTiles/MBTiles/indexed archive.
4. Copy the file to FAT32 microSD; verify size and checksum.
5. Select an available offline map on the controller; preserve existing online fallback policy and attribution.
6. Open map with Wi-Fi disabled; display accurate coverage/zoom limits and missing-area states.

Useful package manifest fields: archive format/version, geographic bounds, min/max zoom, tile type and logical dimensions, provider/attribution, source/license, date, byte size, SHA-256, and generation-tool version. Store metadata outside the runtime hot path or read it once.

Do not require all existing users to migrate. Loose XYZ directories must remain a supported fallback until a new implementation is proven.

For worldwide travel, prefer **regional archives and selected zoom bands** over one huge high-detail world file. Tile counts grow rapidly at high zoom; estimate size before generating.

## M11. Vector map engine (separate research)

**Status:** DEFERRED.

Vector maps allow independent label/font styling and selective rendering. They also require a much larger system: vector decoding, shape geometry, style evaluation, glyph/font atlases, collision detection, text placement, anti-aliasing, and memory management.

Do not bundle this into raster optimization work. Before reopening, require a proof of concept demonstrating:
- Memory/stack safety with both BLE radios and Wi-Fi active.
- Smooth pan/zoom and legible text on 480 × 320.
- No regression in tracks, POIs, node groups, touch, or low-memory behavior.
- A clear measured benefit over improved raster styles and the 1.25× option.

The default development path remains **raster**.

## M12. Measurement, tests, and observability

**Status:** REQUIRED for every methodology.

### 12.1 Baseline and controlled scenarios

Capture firmware commit/version, SDK configuration, display settings, map source and attribution, SD card model/format, tile dataset checksums, cache state, radio firmware versions, Wi-Fi state, and GNSS conditions.

Run at minimum:

| Scenario | What it reveals |
|---|---|
| Offline cold launch with empty RAM tile cache | SD + decode + first useful frame |
| Offline revisit with warm RAM cache | Cache lookup and redraw |
| Rapid pan across adjacent tiles | Evictions, cancellations, stalls, frame reuse |
| Repeated zoom in/out and pinch | Decode churn, scaling, touch latency |
| Online uncached viewport with both radios connected | HTTPS/TLS + BLE contention |
| Online revisits after reboot | Persistent cache reuse |
| Dense MT/MC node overlay and clustered/ungrouped views | LVGL, overlays, memory pressure |
| Route recording plus repeated POI/map interaction | GNSS/SD contention |
| Night/day switching; large-label mode on/off | Processing overhead and visual quality |
| Missing/corrupt tile, invalid archive, SD removal/full | Safe failure and recovery |

### 12.2 Metrics

Record at least:
- Time to first useful map image; p50/p95 per-tile lookup, read, decode, composition, and end-to-end request.
- Number of SD opens, seeks, read calls, bytes read; filesystem and image decode errors.
- Decoded RAM cache hit/miss/evictions and persistent cache hit/miss.
- Free/minimum internal heap, DMA-capable heap, PSRAM, largest free block, allocation failures; **distinguish total free bytes from largest contiguous allocation**.
- Map worker, UI, BLE/NimBLE, touch, and other relevant stack high-water marks.
- Input-to-visible-update latency, missed gestures, dropped touch events, LVGL task time, watchdog/reset causes.
- TLS retries, provider responses/backoff, BLE disconnects/reconnects, chat-message responsiveness.
- Archive size, file count, pack-generation time, PC-to-SD copy time, and startup index memory.
- Human-readability result for street names and node labels.

Existing diagnostics and previously observed failures are documented in [memory investigation](memory-diagnostics-v0919.md), [cluster layout bounds](map-cluster-bounds-v0920.md), and [map TLS/DMA investigation](map-tls-dma-v0923.md). Compilation and host simulations are prerequisites, **not replacements** for testing on the ESP32-S3.

### 12.3 Suggested experiment record template

For each experiment, append a dated entry using this structure:

    Experiment ID:
    Date:
    Methodology:
    Hypothesis:
    Baseline firmware commit:
    Candidate firmware commit:
    Hardware, SD and radio conditions:
    Tile source/dataset and checksum:
    Cold/warm/offline/online scenario:
    Before measurements (p50/p95; memory minima):
    After measurements (p50/p95; memory minima):
    Visual or operational regressions:
    Reproduction commands / test artifact links:
    Result: PASS / FAIL / INCONCLUSIVE
    Decision: ADOPT / REWORK / REJECT / RETEST
    Notes:

Never write "faster", "smaller", or "uses less memory" without quantifying the comparison and identifying the test environment.

## 4. Suggested experimental order

**E01 — Measurement baseline (no functional modification).** Instrument existing raster XYZ PNG workflow and collect reproducible cold/warm/offline/online measurements. Confirm available diagnostics and record the v0.9.32 1.25× cost. **Gate:** reliable benchmark reproduction.

**E02 — Archive feasibility, same PNGs.** On a desktop, package a fixed legal tile sample into a simple indexed archive and PMTiles, then compare file count, bytes, random-read patterns, and index size. No firmware changes are necessary for initial desktop evaluation. **Gate:** clear benefit and feasible bounded reader design.

**E03 — Read-only archive adapter.** Prototype a PMTiles raster-only reader or a very small indexed archive reader behind the existing map worker, with loose-XYZ fallback and strict malformed-file handling. **Gate:** hardware latency, RAM, and regression results beat or match baseline without new instability.

**E04 — Cache tuning.** Compare 6/12/16 tile RAM caches and bounded index caches; choose based on **overall** memory/latency with dual BLE, not only tile hit rate.

**E05 — Label readability.** Compare current 1.25× magnification against purpose-built larger-label 256px raster tiles, with a controlled map dataset. **Gate:** objectively better legibility without excessive blur or footprint.

**E06 — Decoder experiment.** Benchmark QOI versus the exact PNG sample end-to-end (read + decode + compose + cache). **Gate:** meaningful improvement after file-size and PSRAM costs are included.

**E07 — Optional broader formats.** Reassess MBTiles and more complex codecs only if simpler approaches fail or interoperability clearly justifies the cost.

Experiments should be independently reversible. Never replace the currently recommended stable firmware as part of a research trial.

## 5. Open questions

- Are the limiting factors on the device **FAT32 metadata seeks, compressed read bandwidth, PNG decode CPU, or LVGL redraw**? Determine by timing each stage.
- How many tiles does a typical screen transition really reuse with the current 12-tile LRU?
- Would a direct RGB565 output path measurably reduce peak memory compared with the existing RGBA-to-RGB565 sequence?
- What is the smallest viable PMTiles directory/index cache for a practical regional archive?
- How do large regional PMTiles archives behave on FAT32 cards near the per-file limit?
- Can a different **licensed raster style** solve most street-name readability complaints without runtime scaling?
- Does online tile scheduling increase pressure on the internal DMA-capable heap or NimBLE stack under dual-radio load?
- Which metrics can be added to the current diagnostic ring without changing timing enough to skew results?
- What backward-compatible map source selection UX fits the 480 × 320 local screen and the remote HTTP UI?

## 6. Conclusions to preserve

1. **A single-file tile archive is primarily an I/O/management optimization, not automatic image compression.**
2. **PMTiles is the first standardized archive worth testing**, but a minimal indexed pack may be a useful implementation/control baseline.
3. **MBTiles is interoperable** yet can carry unnecessary SQLite overhead on a memory-constrained controller; measure rather than assume.
4. **QOI may trade larger files for faster decoding**; only an end-to-end benchmark can decide.
5. **Raster street-label typography is baked into source tiles.** Lowering tile resolution alone is not a reliable path to larger labels.
6. **PSRAM availability does not eliminate internal DMA/BLE/TLS constraints**; stable dual-radio communication outranks map micro-optimizations.
7. **Keep dynamic online cache writes and read-only offline packages separate** unless a specific benchmark supports combining them.
8. **Preserve and enhance the existing map worker and LVGL renderer** before considering a new vector engine.
9. **Do not bulk-download from public OSM tile servers** to generate offline maps.
10. An experiment is complete only after reproducible measurements and physical hardware regression tests.

## 7. Reference material

### Public format and tooling references
- [PMTiles v3 specification](https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md)
- [PMTiles project and tooling](https://github.com/protomaps/PMTiles)
- [MBTiles specification](https://github.com/mapbox/mbtiles-spec)
- [QGIS XYZ tile generation (directory and MBTiles)](https://documentation.qgis.org/3.44/en/docs/user_manual/processing_algs/qgis/rastertools.html)
- [QOI image format](https://github.com/phoboslab/qoi)
- [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/)

### MeshCrossway internal source/history
- [Map worker and caches](../firmware/display-prototype/meshcrossway_map_tiles.h)
- [Map/GNSS baseline](maps-gps-v081.md)
- [Map rendering latency](map-render-latency-v087.md)
- [Map grouping](map-grouping-v0924.md)
- [Map controls and raster night mode](map-controls-v0925.md)
- [Map controls v0.9.31](map-ui-v0931.md)
- [Memory investigation](memory-diagnostics-v0919.md)
- [Bounded cluster previews](map-cluster-bounds-v0920.md)
- [TLS/DMA issue and mitigation](map-tls-dma-v0923.md)

---

**Maintenance rule:** This is the **single index of map-optimization methods and experiments**. Keep version-specific implementation notes in their historical documents, but update the methodology status and experiment outcome here whenever new information becomes available. A future method should receive a new stable ID (M13, M14, etc.) rather than rewriting the history of a prior experiment.
