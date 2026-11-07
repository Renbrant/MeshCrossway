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
| M13 | Desktop-preprocessed raw RGB565 raster tiles | Eliminate device-side PNG decode for offline tiles; investigate lower cold-load latency | 128 KiB/tile, SD throughput and DMA buffer constraints | Medium — benchmark first |
| M14 | Direct vector geometry / MVT Protocol Buffers and heading-up | Smaller regional map datasets in some cases; flexible text/style and map bearing | Parsing, styling, labels, CPU and PSRAM; major architecture change | Deferred — isolated proof of concept |
| M15 | Display-bus optimization / LovyanGFX feasibility | Lower LCD transfer overhead and better scheduling if existing BSP is the bottleneck | Driver/touch/PMU/DMA compatibility and UI regressions | Medium — measurements first |
| M16 | Incremental PNG scanline-to-RGB565 decoder | Retain existing PNG packs, potentially avoid full RGBA decode buffer | Decoder integration, PNG compatibility, working buffers | High — first embedded prototype |
| M17 | Split-image/strip decompression (SPNG, SQOI, SJPG) | Bounded temporary decode memory | Offline converter, SD adapter and no built-in image rotation/zoom | Medium |
| M18 | LZ4/heatshrink blocks over RGB565 raster | Balance raw RGB565 cold-load CPU gains with smaller SD files | Compression ratio varies; block index, memory and decode CPU | Medium |
| M19 | Indexed/palette raster tiles (4-bit/8-bit) | Lower raw tile storage and possibly fast decode | Quantization can damage labels/roads; palette conversion | Medium |
| M20 | Hybrid raster base + locally drawn labels/selected features | Larger, selectable street labels without full-vector basemap replacement | Offline feature indexing, text layout, collision and synchronization | Medium — targeted legibility prototype |
| M21 | Dual-core tile decode/composition scheduling | Potentially overlap SD reads, decode and rendering | Severe BLE/UI contention risk; secure TLS and task ownership | Low until profiling |


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

## M13. Desktop-preprocessed raw RGB565 raster tiles (.bin)

**Status:** CANDIDATE. **Scope:** offline map data and a read-only tile source behind the existing worker, not a replacement for the network cache.

**Method:** On a desktop, transform properly licensed source raster tiles into 256 × 256, 16-bit-per-pixel RGB565 tiles. Store as fixed-size `.bin` files or pack them into an indexed container. Each tile then contains already formatted pixels rather than compressed PNG image data. A reader checks the format, bounds, row layout, endianness and version, reads the RGB565 payload, and passes it to the existing composition path with no PNG/DEFLATE decoding. Define an explicit on-disk format (e.g. version, magic, dimensions, pixel order, optional checksum), and distinguish **raw payload only** from **LVGL-specific binary files** with a display/image header.

**Quantitative costs:** One raw 256 × 256 RGB565 image occupies **131,072 bytes / 128 KiB** before metadata, whether it contains blank terrain or complex streets. A 12-tile cache occupies **1.5 MiB** of PSRAM before metadata. The existing MeshCrossway worker **already caches decoded RGB565** in PSRAM: this technique primarily saves the decode path on **cold/offline cache misses**, not the cost of redrawing a tile already in RAM. Pre-processing adds a desktop workflow and generally increases SD capacity consumption and read bandwidth compared with PNG. If the pre-conversion is performed once on a PC, the controller avoids conversion CPU and transient RGBA decoding for those offline files.

**DMA accuracy:** Raw RGB565 can be closer to the LCD's native transfer representation and can *potentially* avoid a per-pixel conversion before display. However, **a .bin file is not automatically DMA-ready**. The current UI composes tiles with overlays in an LVGL pipeline; drawing still requires correct compositing, clipping, pixel/byte order, flush-buffer ownership, transfer size/alignment and synchronization. DMA capability depends on the ESP32-S3 SPI/LCD peripheral, ESP-IDF configuration and whether the buffer is allocated in DMA-compatible memory. PSRAM DMA/EDMA has constraints; a **small aligned internal DMA-capable staging/stripe buffer** may be required. Do not assume that a pointer into PSRAM or an SD buffer can be submitted directly. Benchmark copying/flush cost and ensure DMA buffers cannot be overwritten until the transfer completes.

**Implementation approach:**
1. Create a reproducible desktop tile converter with fixed RGB565 byte order, explicit version/checksum, a compact manifest and preservation of attribution/license information. Handle alpha compositing consistently with the existing white/background treatment.
2. Keep the existing PNG XYZ reader and HTTPS cache intact; add a read-only raw-tile adapter as an optional source for offline packages. Consider M01 indexed archives to reduce the number of large individual `.bin` files.
3. Reuse the current worker, request-generation cancellation, 12-slot bounded LRU, PSRAM allocation policy and result ownership rules. Do not create a second competing map task or duplicate cached tiles.
4. Validate invalid/short/truncated files, wrong dimensions/order, SD removal, FAT32 card sizes and disconnected Wi-Fi; never confuse raw binary tiles with existing PNG files.
5. Compare the **same map imagery and viewport** in PNG and raw RGB565 on the board. Record cold SD read+decode+compose p50/p95, bytes read, first usable frame, PSRAM peak, internal heap/DMA minimum, BLE performance and total region size. Also compare warm-cache performance: it may be essentially unchanged.
6. If investigating direct LCD DMA, prototype it **independently from the tile format change** to separate gains from hazards.

**External engineering example:** [0015/map_tiles](https://github.com/0015/map_tiles) demonstrates **RGB565 offline tiles**, a **bounded LRU** and **FreeRTOS background tile loading** in its v2 navigation layer. Its file layout includes a small LVGL image header rather than assuming every `.bin` is headerless raw pixels. **It targets LVGL 9.x**; MeshCrossway uses **LVGL 8.x**, so study the principles and data layout rather than integrating the component without adaptation. See also the [offline preparation examples](https://github.com/0015/OfflineMapDownloader). Its background-cache idea is already partially present in MeshCrossway; the novel research question is whether **desktop-predecoded storage** helps.

**Decision gate:** Adopt only if lower cold-map latency/reduced decode CPU outweigh substantially larger offline packages, SD bandwidth and any new memory/DMA constraints. **No hardware improvement has been measured yet.**

## M14. Direct vector geometry, MVT/Protocol Buffers, and heading-up display

**Status:** DEFERRED — standalone research/prototype; complements the broad M11 vector-engine discussion.

**Methodology families (do not conflate them):**
- **Direct custom geometry**: preprocess roads/areas into a simplified `.vec` (or similar) file for an embedded renderer. A parser can be tailored to a limited set of geometry types, zoom ranges and style rules.
- **Mapbox Vector Tiles (MVT) in PBF**: encode geometries, tags and layer information with **Protocol Buffers** inside tiled payloads. Requires parsing PBF commands, decoding geometry deltas, applying per-zoom styles and rendering.
- **Offline vector packages** (e.g. vector PMTiles): an *archive/container* can hold vector tiles but does not remove the need for a working renderer. M02 raster PMTiles is a separate, much smaller experiment.

**Potential benefits:** For particular regions, zoom ranges, feature selections and styling, vector data can use less SD storage than raster tiles repeated across zoom levels; actual size depends on geometry density, generalization and compression. Roads, boundaries and labels can be styled, made larger and prioritized independently, addressing street-name readability more directly than magnifying a finished raster. A vector pipeline can recompute positions around a chosen bearing for a **heading-up/track-up display**, keeping symbols or labels upright while map geometry rotates.

**Heading-up nuance:** Vector geometry makes arbitrary-angle rotation and label-aware orientation *architecturally natural*, but **does not guarantee low CPU cost on ESP32-S3**. Transformation math, geometry clipping, line joins, antialiasing, polygon fills, label collision detection, font/glyph memory and redraw frequency still matter. Conversely, a raster map *can* be rotated by software resampling or hardware assistance; it may produce blur and extra frame-copy costs, but rotation is not inherently impossible. Compare both approaches rather than assuming vector rotation will be faster.

**Recommended bounded prototype:** Parse a preselected small region offline; reduce geometry precision and eliminate unused layers before writing to SD. Prefer one bounded feature/vertex working set, streamed/incremental decode, zoom-level-of-detail geometry, clipped shapes and precomputed glyph atlases. Define limits for malicious or invalid PBF/geometry length, vertex count and nesting; bound labels and collision checks. First render roads/water/areas without text; add labels, markers and heading-up only after collecting peak PSRAM/heap, task-stack and frame-time evidence. Keep all rendering in the map pipeline without blocking BLE tasks and preserve the current raster release untouched.

**Reference and format distinction:** [ihayri/ESP32-S3-Offline-GPS-Maps](https://github.com/ihayri/ESP32-S3-Offline-GPS-Maps) is a useful **embedded direct-vector parsing/rendering example**: its README describes `chernik.vec` and `forests.vec` local geometry in FFat plus offline rendering of roads/forest areas. **That source is not evidence of MVT/PBF/Protocol Buffers support**; verify its custom `.vec` parser separately. For interoperable vector tiles study the [Mapbox Vector Tile specification](https://github.com/mapbox/vector-tile-spec) and [Protobuf](https://protobuf.dev/) with a deliberately bounded MCU parser.

**Experiment plan:** (a) compare a small custom-vector dataset and MVT/PBF built from equivalent legal source data; (b) record regional archive sizes vs raster PNG/PMTiles for the *same* zoom/coverage; (c) compare north-up vs heading-up at fixed bearing and during continuous GNSS heading updates; (d) measure p50/p95 rendering and touch latency, memory peaks, BLE stability, font legibility, labels-at-rotation, clipping and visual correctness; (e) check license and attribution requirements.

**Decision gate:** Keep **DEFERRED** until a hardware prototype demonstrates an objective benefit and fits internal/DMA heap, PSRAM, watchdog, touch and dual-BLE constraints. This must **not silently revive or replace** the previously deferred major vector rewrite.

## M15. Optimizing the LCD transfer pipeline and investigating LovyanGFX

**Status:** CANDIDATE for profiling, **not approval to replace the current display driver**.

**Problem hypothesis:** Even with fast map tile decoding, the display may be bounded by **SPI transfer time**, flush-call count, LVGL draw/invalidation work, color conversion or CPU contention. A framework with efficient DMA transfers, batched drawing, asynchronous flushes and correct bus locking can sometimes reduce per-frame overhead. A faster renderer cannot exceed the fundamental SPI bit-rate limit for a given number of LCD pixels.

**Reference:** [LovyanGFX](https://github.com/lovyan03/LovyanGFX) is an open-source ESP32 graphics framework with SPI, parallel-bus and LVGL integration examples. [IceNav-v3](https://github.com/jgauchia/IceNav-v3) is a GPS mapping project combining **LVGL and LovyanGFX**, using raster/vector map presentation and supporting varied SPI/parallel displays; its documentation explicitly notes special handling when **display and SD share an SPI bus**. This is an **engineering reference, not a proven speedup for MeshCrossway**. Verify its GPL-3.0 licensing before considering reuse of any code; learning architectural techniques does not require copying that code.

**MeshCrossway-specific reality check:** The Waveshare controller uses an ST7796 SPI LCD, ESP-IDF 5.3.2 and the pinned Waveshare BSP/LVGL 8.4 driver. Its SD card uses **SDMMC 1-bit**, not the same SPI bus as the screen in the current configuration. The two LoRa radios are **separate BLE-connected modules**, not SPI transceivers on the controller's display bus. Therefore a claim that LovyanGFX will "fix SPI contention with the radio" is **not applicable as stated**. Possible indirect contention still exists in CPU, PSRAM/memory bandwidth, DMA-capable internal heap, scheduling and Wi-Fi/BLE coexistence; these must be measured and attributed correctly.

**Evaluation method:**
1. Instrument current LVGL flush callback count/area/bytes, SPI clock and transfer time, queue depth, invalidated pixel area, frame pacing, touch latency, CPU busy time, internal DMA heap and PSRAM bandwidth with radio connections held constant.
2. First tune **existing** LVGL invalidation, draw-buffer/stripe sizing, bus transaction setup, queueing and transfer schedule, without changing the BSP. Preserve verified display, touch, backlight/AXP2101 and SD behavior.
3. Only if the current display driver is the measured bottleneck, make an isolated prototype of a **LovyanGFX flush backend**, retaining LVGL 8 and exactly the same UI/map assets. Confirm ST7796 panel support and correct pins, SPI clock/mode, RGB565 byte order, rotation, flush-complete callback, DMA alignment and resource cleanup.
4. Ensure new/old SPI transactions are serialized correctly. Avoid starving the LVGL task, touch and radio BLE callbacks; avoid invalid buffer reuse while DMA is active.
5. Compare identical static map redraws, rapid pan, dense node markers, GPS follow, night mode, Wi-Fi HTTPS downloads and dual BLE traffic. Track FPS/visible update latency, transfer throughput, power, RAM minima, largest DMA block, task watermarks, watchdogs and resets.
6. Retain the current BSP backend as a reversible fallback. No full framework replacement without clear **hardware** evidence.

**Decision gate:** If profiling shows the bottleneck is **PNG decode, SD I/O or label layout**, changing the display framework may provide little value. Consider LovyanGFX only if measured LCD transfer overhead is significant and a compatible flush-backend prototype preserves all controller services.

## M16. Incremental PNG scanline decoding directly into RGB565

**Status:** CANDIDATE; **recommended first implementation-oriented experiment**.

**Finding:** [bitbank2/PNGdec](https://github.com/bitbank2/PNGdec) is an Apache-2.0 decoder designed for embedded systems. It supports callback-based scanline decoding and conversion of each decoded line to RGB565. It can be built in C/C++ and supports standard PNG features except interlacing. The decoder's own documentation indicates a minimum memory requirement around 48 KiB in a constrained configuration, but the **actual ESP32-S3 heap capability and temporary buffers must be measured**, not inferred from marketing benchmarks.

**Fit to MeshCrossway:** The existing `meshcrossway_map_tiles.h` uses LodePNG to decode a PNG into a **256 × 256 RGBA intermediate buffer (256 KiB)**, then converts that buffer into the existing **128 KiB RGB565 LRU slot**. A row-at-a-time path could populate the RGB565 slot as the decoder produces pixels, potentially avoiding the full RGBA allocation and a separate conversion traversal. The SD PNG cache, normal XYZ pack structure, HTTPS validation/caching, 12-slot PSRAM LRU and LVGL compositor could all remain unchanged.

**Important qualifications:**
- PNGdec is not a drop-in guaranteed faster decoder. The DEFLATE and PNG filtering work still occur; latency depends on actual map tiles and whether line conversion saves sufficient copies.
- Understand PNGdec's working memory, allocation mode, reentrancy, thread safety, supported alpha/transparency modes and treatment of indexed PNGs. Match the present background compositing, pixel ordering, error behavior and reject interlaced samples if necessary.
- Preserve PNG validity and strict upper bounds. Do not disable CRC/integrity checks just to claim performance improvements; first run a controlled, documented experiment on verified offline assets.
- Run decode on the **existing map worker**, not the LVGL task; ensure cancellation, PSRAM ownership and incomplete-cache-slot cleanup are safe.
- Benchmark PNG with labels/transparent pixels and offline/online cached variants; compare **peak internal heap, PSRAM, largest contiguous block, p50/p95 per tile and actual visual equality**. A lower temporary memory footprint may be more valuable than higher FPS.

**Reference:** [PNGdec](https://github.com/bitbank2/PNGdec), [PNGdec README](https://github.com/bitbank2/PNGdec/blob/master/README.md).

**Decision gate:** Test before changing any packaging or UI. Retain the LodePNG fallback until the scanline path is validated on both real radios with concurrent HTTPS and map gestures.

## M17. Split/strip-encoded image tiles (SPNG, SQOI, SJPG)

**Status:** CANDIDATE; compare after M16.

**Finding:** Espressif's [esp_lv_decoder](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/tools/esp_lv_decoder.html) supports **split PNG (SPNG)**, **split QOI (SQOI)** and **split JPEG (SJPG)** as well as regular image formats. Its documentation lists compatibility with **ESP-IDF >= 5.3** and **LVGL >= 8.3, < 10**, potentially matching the MeshCrossway **ESP-IDF 5.3.2 / LVGL 8.4** build. Examples use 16-line strips; only each strip is decoded during drawing. The split formats **do not support LVGL image zoom/rotation directly**.

**Possible approach:**
1. Use the computer to generate equivalent licensed source tiles as PNG XYZ, SPNG, SQOI and (only when image quality permits) SJPG.
2. Measure storage/packing overhead, per-strip index/headers, SD seeks and actual working buffers. Segmenting PNG independently can lose cross-row compression opportunities; file sizes are not inherently smaller.
3. Evaluate whether an SD-file reader can feed the **existing worker/compositor**. Espressif's canned build workflow uses `esp_mmap_assets` / packaged assets, which is not automatically a runtime microSD map-pack solution.
4. Compare two models separately: streaming strips into RGB565 cache tiles vs drawing strip-by-strip with no full decoded tile cache. The second changes the rendering architecture and could increase decoding on each redraw.
5. Keep zoom/pan/pinch behavior in the **map view geometry**, not depend on unsupported LVGL image transformations.

**Reference:** [Espressif decoder documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/tools/esp_lv_decoder.html), [ESP32-S3 compatible decoder example](https://github.com/espressif/esp-iot-solution/blob/master/examples/display/gui/lvgl_decode_image/README.md).

**Decision gate:** Proceed only if a strip workflow offers meaningful improvements beyond the simpler M16 scanline decoder at acceptable I/O and compatibility cost.

## M18. Lightly compressed RGB565 blocks: LZ4 and heatshrink

**Status:** CANDIDATE.

**Problem:** M13 raw RGB565 skips PNG decoding but consumes **128 KiB per 256 × 256 tile**, often substantially more than its PNG equivalent. Test lightweight **lossless** compression of offline RGB565 tiles to reduce SD bytes without returning to the full PNG pipeline.

**Alternatives:**
- [LZ4](https://github.com/lz4/lz4): fast block-oriented decompressor (library BSD-2-Clause). Prefer **independent bounded blocks** so the MCU never needs to allocate a whole region or archive to decode one tile. Desktop throughput figures **do not extrapolate** to the ESP32-S3.
- [heatshrink](https://github.com/atomicobject/heatshrink) or its [ESP32-oriented fork](https://github.com/BitsForPeople/esp-heatshrink): incremental decoding with small configurable work memory; potentially useful on particularly memory-sensitive paths, though it may spend more CPU than LZ4.

**Experimental variants:** Whole-tile LZ4 blocks vs rows/16–32-row strips; no compression (M13); PNG baseline; QOI (M04); and optional 8-bit indexed pixels (M19). Store explicit tile sizes, dimensions, codec, version, checksum and bounded offsets in the manifest/index. Stream into an existing RGB565 cache slot or one bounded staging buffer; avoid duplicate full-tile copies. Decompress only when a tile is requested, do not compress on-device during routine navigation.

**What can go wrong:** Some tiles are complex/high-entropy or have little redundancy; LZ4/heatshrink can be larger than hoped. More SD reads versus decode CPU is a real tradeoff. Block framing, corruption checks, endian handling, cache effects and sparse reads need test coverage.

**Measurements:** Identical input tile imagery and 50–200 representative tiles across rural/urban/terrain styles; package bytes, compression ratios (p50/p95 and worst), cold lookup latency, decode CPU, internal/DMA heap, PSRAM peak and warm-cache performance. Compare against PNG with **no claim of compression win until measured**.

**Decision gate:** Accept only if smaller packages *and* real end-to-end latency/memory benefits survive hardware testing.

## M19. Paletted/indexed raster tiles (4-bit or 8-bit)

**Status:** CANDIDATE.

**Method:** Quantize properly licensed raster tiles to a limited palette on a desktop; store **per-pixel indices** and a small tile/shared palette, either raw or as indexed PNG and/or indexed archive blocks. LVGL 8 supports 1-, 2-, 4- and 8-bit indexed images, but the current MeshCrossway worker uses **its own RGB565 tile cache/compositor**, so it would need a bounded index-to-RGB565 conversion or a separate index-aware composition path.

**Size math, excluding palette/headers:** A 256 × 256 tile in raw **8-bit indices = 64 KiB** vs RGB565 **128 KiB**; **4-bit indices = 32 KiB**. A local RGBA8888 256-color palette costs **1 KiB**. Actual indexed PNG size can be much smaller/larger depending on deflate and map content. If the map is rendered into a standard RGB565 frame, palette expansion still costs CPU; no automatic DMA direct-draw claim is justified.

**Map-specific strengths:** Flat areas and a constrained cartographic style can quantize well. A **purpose-built street style** with a small designed palette may make labels larger/clearer and keep road/terrain contrast reliable. A generic photograph/satellite image or dither-heavy palette can compress poorly or visibly damage fine text, anti-aliased edges, thin roads and symbols.

**Test plan:**
1. Prepare representative dense-city, countryside, topo and night tiles; compare original PNG, indexed 8-bit, indexed 4-bit and RGB565.
2. Check **human readability** of street labels at 480 × 320 before optimizing bytes.
3. Measure p50/p95 SD reads, conversion time, RAM peaks, overall package size and reuse under map/night settings. Account for palette changes that may invalidate the decoded cache.
4. Preserve exact geographic tile boundaries, coordinates and provider attributions.

**Reference:** [LVGL 8.x indexed image formats](https://lvgl.io/docs/open/8.3/overview/image), [PNGdec indexed-PNG decoding](https://github.com/bitbank2/PNGdec).

**Decision gate:** Prioritize label quality; do not trade readable maps for superficial size gains.

## M20. Hybrid map: existing raster base with independent label/feature overlays

**Status:** CANDIDATE / TARGETED PROOF OF CONCEPT. **Not a full MVT renderer.**

**Goal:** Address the underlying complaint that **raster street names are too small** while avoiding wholesale map-engine replacement. Retain the existing PNG XYZ basemap, then draw a **small, selected set of important place names, road labels, and/or navigation features** from a desktop-generated georeferenced sidecar. Font size, contrast, language and orientation become independently adjustable.

**Method:**
- Desktop generation from licensed OSM regional extracts or local data produces a compact **position-indexed** and **zoom-banded** record set (e.g. name, Unicode string ID, type/importance, bounding box or line geometry, min/max zoom).
- Stream only the visible viewport's records from SD or an indexed archive. Avoid loading the entire area's labels into PSRAM.
- Project labels using existing Web Mercator routines and current pan/zoom geometry; clip against viewport and reserve a **bounded, fixed-size label/glyph set**.
- Render labels above base imagery but **below GPS, track/POI and radio-node overlays**; use a small label-placement budget, readability-aware priorities and clipping.
- To avoid *duplicate* baked-in labels from the raster source, pair this with an optional **label-free raster style** or constrain the overlay to emergency/POI names not present in the raster base.
- Document offline licensing/attribution and safe fallbacks to plain raster when sidecar data is unavailable.

**Alternative upstream research:** [OrcMaps](https://github.com/hardcoreerik/orcmaps) is an early-stage ESP32-class **offline PMTiles/MVT streaming architecture** with host-tested PMTiles reader, MVT parsing, feature translation and styling. Its base geography / application-overlay separation is relevant as architectural inspiration, but **OrcMaps is not yet an established, hardware-validated drop-in for MeshCrossway**. Its source is **AGPL-3.0-only** (with a separate commercial-license option): do **not copy its implementation** into the MeshCrossway repository without an explicit licensing decision. Its planned overlay/cache subsystems are not proof of a finished production renderer. [Planetiler](https://github.com/onthegomap/planetiler) is an upstream desktop generator for legal OSM-derived vector packs and feature processing.

**Comparison:** (a) existing 1.25× full-image enlargement, (b) custom raster with larger baked-in labels (M07), (c) label sidecar and bounded overlay, (d) full vector M14. Compare label readability and p95 input latency, memory, overdraw and tile alignment, multi-language handling, and whether road names overlap radio-node markers.

**Decision gate:** Continue only if it improves label usability without making the map slower or more complex than dedicated larger-label raster tiles.

## M21. Concurrent decode/composition and multi-core scheduling

**Status:** LOW PRIORITY until profiling; **the present map worker is already asynchronous**.

**Reference:** [CelliesProjects/OpenStreetMap-esp32](https://github.com/CelliesProjects/OpenStreetMap-esp32) uses LovyanGFX, PNGdec and concurrent work on both ESP32 cores to fetch/decode/composite OSM raster maps, with reusable PSRAM tiles. This is a useful **pipeline design example, not a safe drop-in**: its README warns that it uses **`WiFiClientSecure::setInsecure()`**, disabling TLS certificate validation. MeshCrossway must **retain its current verified HTTPS certificate/hostname checks**.

**Potential experiment:** Separate I/O, decode and composition phases with explicit ownership and bounded queues; optionally overlap SD read of the next *offline* tile with decoding of an already-read tile. If introducing a new task, enforce strict peak memory/stack, core-affinity, cancellation, race-free PSRAM object lifetimes and deterministic LVGL ownership. Do not run LVGL APIs off-thread, and do not create a second HTTPS fetcher bypassing backoff/provider policies.

**Why risky:** Dual BLE, Wi-Fi HTTPS, TLS allocations, SD card access and touch rendering already compete for internal heap, CPU and PSRAM. More parallelism can produce **higher latency, DMA pressure, radio stalls or watchdogs** instead of speedups. On-screen throughput might remain SPI-limited.

**Gate:** First use M12 profiling to prove idle capacity and an actual pipeline bottleneck, then prototype *one* bounded asynchronous change with both radios active. If memory/task scheduling regress, reject even if a desktop benchmark appears faster.

## 4. Suggested experimental order

**E01 — Measurement baseline (no functional modification).** Instrument existing raster XYZ PNG workflow and collect reproducible cold/warm/offline/online measurements. Confirm available diagnostics and record the v0.9.32 1.25× cost. **Gate:** reliable benchmark reproduction.

**E02 — Archive feasibility, same PNGs.** On a desktop, package a fixed legal tile sample into a simple indexed archive and PMTiles, then compare file count, bytes, random-read patterns, and index size. No firmware changes are necessary for initial desktop evaluation. **Gate:** clear benefit and feasible bounded reader design.

**E03 — Read-only archive adapter.** Prototype a PMTiles raster-only reader or a very small indexed archive reader behind the existing map worker, with loose-XYZ fallback and strict malformed-file handling. **Gate:** hardware latency, RAM, and regression results beat or match baseline without new instability.

**E04 — Cache tuning.** Compare 6/12/16 tile RAM caches and bounded index caches; choose based on **overall** memory/latency with dual BLE, not only tile hit rate.

**E05 — Label readability.** Compare current 1.25× magnification against purpose-built larger-label 256px raster tiles, with a controlled map dataset. **Gate:** objectively better legibility without excessive blur or footprint.

**E06 — Decoder experiment.** Benchmark QOI versus the exact PNG sample end-to-end (read + decode + compose + cache). **Gate:** meaningful improvement after file-size and PSRAM costs are included.

**E07 — Raw RGB565 offline A/B (M13).** Generate the same small licensed tile set as PNG and desktop-preprocessed RGB565; test cold read/compose, total SD bytes/size, warm-cache speed, PSRAM, DMA heap and BLE coexistence. **Gate:** measured gain with acceptable storage cost. Keep LCD DMA experiments separate from storage-format experiments.

**E08 — Display-flush profiling (M15).** Quantify existing LVGL flush cost and SPI throughput first. Attempt an isolated alternative backend only if this is a confirmed bottleneck. **Gate:** equivalent visuals/touch/PMU with better physical latency and no stack or DMA regression.

**E09 — Direct vector/heading-up study (M14).** Desktop/custom-geometry and MVT/PBF proof of concept followed by a tightly bounded ESP32-S3 hardware test, without replacing the raster pipeline. **Gate:** demonstrable storage/heading-up/label value at acceptable frame-time and heap cost.

**E10 — Streamed PNG decoder (M16).** Before any new offline format, compare current LodePNG RGBA intermediate to PNGdec scanline→RGB565 into the same cached tile, with regression cases for alpha, indexed colors and malformed PNG. **Gate:** stable PNG interoperability plus lower memory or latency on hardware.

**E11 — Encoded strips / compressed RGB565 (M17, M18).** Generate fixed licensed sample data and compare sliced SPNG/SQOI/SJPG, raw RGB565, LZ4 block RGB565 and heatshrink, with SD read, working memory, cache and on-screen latency measurements.

**E12 — Palette and label usability (M19, M20).** Compare color-indexed tiles and a small georeferenced label sidecar against the current 1.25x zoomed raster and larger-label raster style. **Gate:** improved label readability without UI/BLE regressions.

**E13 — Multi-core pipeline (M21).** Profile first; proceed only with a bounded experimental task/queue that retains existing HTTPS security and map worker ownership. **Gate:** measured physical improvement without increased watchdog, DMA or Bluetooth risk.

**E14 — Optional broader formats.** Reassess MBTiles and more complex codecs only if simpler approaches fail or interoperability clearly justifies the cost.

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
- For equal geographic coverage, how much larger are raw RGB565 offline packages and how much lower is actual cold-load latency than PNG?
- Is the current display flush limited by SPI bandwidth, small transactions, invalidated pixel count, DMA staging, or CPU rendering?
- Can a heading-up vector prototype maintain legible upright labels and stable dual BLE without significant input latency?
- How much peak RAM can a PNG scanline-to-RGB565 path save compared with LodePNG's RGBA intermediate, without losing PNG compatibility?
- Do split-image tile formats remain useful once a full RGB565 LRU is already kept warm, or do repeated strip reads dominate?
- Which cartographic styles tolerate 4/8-bit color quantization without damaging street labels?
- Does a small georeferenced name/feature sidecar improve readability more than fully re-rendered vector tiles or improved raster styles?
- With two BLE radios and Wi-Fi active, does parallel tile decode improve end-to-end latency, or just intensify internal heap/PSRAM contention?

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
10. **Raw RGB565 eliminates PNG decoding on offline cold loads, not all rendering costs or DMA staging**; its 128 KiB/tile storage penalty must be measured.
11. **Vector data enables heading-up rendering by geometry transformation but does not guarantee cheap rotation or label placement** on ESP32-S3. Custom `.vec` parsing and standardized MVT/Protobuf are distinct technologies.
12. **LovyanGFX is a potential display-backend optimization, not a radio-SPI contention fix in the current modular design.** Profile and isolate the LCD backend first.
13. **Before replacing PNG, evaluate incremental scanline→RGB565 decode** because it can preserve all existing offline datasets while avoiding a full RGBA intermediate.
14. **Split-image and LZ4/heatshrink are separate ways to trade decode memory, SD bytes and CPU**; no codec is inherently optimal for all map styles.
15. **Indexed 4/8-bit raster and independent georeferenced label overlays deserve legibility testing**, not just compression benchmarks.
16. **Early-stage offline map engines and concurrent multicore demos are research references**, not production readiness claims; review licenses and never copy insecure HTTPS settings.
17. An experiment is complete only after reproducible measurements and physical hardware regression tests.

## 7. Reference material

### Public format and tooling references
- [PMTiles v3 specification](https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md)
- [PMTiles project and tooling](https://github.com/protomaps/PMTiles)
- [MBTiles specification](https://github.com/mapbox/mbtiles-spec)
- [QGIS XYZ tile generation (directory and MBTiles)](https://documentation.qgis.org/3.44/en/docs/user_manual/processing_algs/qgis/rastertools.html)
- [QOI image format](https://github.com/phoboslab/qoi)
- [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/)
- [0015/map_tiles — RGB565 offline tiles, LRU cache, FreeRTOS background loader (LVGL 9)](https://github.com/0015/map_tiles)
- [0015 OfflineMapDownloader — desktop map preparation examples](https://github.com/0015/OfflineMapDownloader)
- [ESP32-S3-Offline-GPS-Maps — embedded custom vector geometry example (.vec, not verified MVT/PBF)](https://github.com/ihayri/ESP32-S3-Offline-GPS-Maps)
- [Mapbox Vector Tile format specification](https://github.com/mapbox/vector-tile-spec)
- [Protocol Buffers documentation](https://protobuf.dev/)
- [LovyanGFX — display bus/graphics driver](https://github.com/lovyan03/LovyanGFX)
- [IceNav-v3 — LVGL and LovyanGFX embedded GPS mapping reference](https://github.com/jgauchia/IceNav-v3)
- [ESP-IDF 5.3.2 SPI master driver](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/peripherals/spi_master.html)
- [ESP-IDF ESP32-S3 external RAM / DMA constraints](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/external-ram.html)
- [PNGdec — incremental scanline decoder and RGB565 conversion](https://github.com/bitbank2/PNGdec)
- [Espressif esp_lv_decoder — split-image PNG/QOI/JPEG](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/tools/esp_lv_decoder.html)
- [LZ4 — fast block compression](https://github.com/lz4/lz4)
- [heatshrink — bounded incremental decompression](https://github.com/atomicobject/heatshrink)
- [LVGL 8 indexed color formats](https://lvgl.io/docs/open/8.3/overview/image)
- [OrcMaps — early-stage bounded offline PMTiles/MVT research (AGPL-3.0)](https://github.com/hardcoreerik/orcmaps)
- [Planetiler — vector tile/offline pack generation](https://github.com/onthegomap/planetiler)
- [OpenStreetMap-esp32 — concurrency/PNGdec/LovyanGFX reference; insecure TLS warning](https://github.com/CelliesProjects/OpenStreetMap-esp32)

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

**Maintenance rule:** This is the **single index of map-optimization methods and experiments**. Keep version-specific implementation notes in their historical documents, but update the methodology status and experiment outcome here whenever new information becomes available. A future method should receive a new stable ID (M22, M23, etc.) rather than rewriting the history of a prior experiment.
