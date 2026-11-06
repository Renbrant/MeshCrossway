# MeshCrossway controller firmware

**Current stable: v0.9.28.** This directory contains the ESP32-S3 controller firmware for the non-B Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0, using ESP-IDF 5.3.2 and LVGL 8.4 through the pinned Waveshare BSP. The historical directory name `display-prototype` remains for build compatibility; the software now includes real dual-radio BLE, messaging, GNSS and storage.

**Current test candidate: v0.9.29.** Expands local color emoji coverage while preserving the stable v0.9.28 image. [Coverage](../../docs/emoji-coverage-v0929.md) · [Test procedure](../../docs/emoji-v0929-validation.md).

Install the exact accepted image with the [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/). The publisher restores checksum-pinned bytes rather than recompiling the stable source. v0.9.25 is previous stable recovery and v0.9.16 remains legacy recovery. See the [stable record](../../docs/stable-v0928.md).

## Available functions

- Independent simultaneous Meshtastic PhoneAPI and MeshCore Companion BLE clients; native messages, channels, pairing, saved radio identities and reconnects. No cross-network forwarding.
- Home radio/Wi-Fi/GNSS status, readable bounded node rows, distance/last-heard sorting and manual list refresh.
- Street/topographic maps, cached SD tiles, local street night view, GNSS clock, tracks, POIs, folders/previews and GPX download.
- Per-network map shape, size and color; optional grouping; ID badges; automatic dismissal of zoom controls.
- **Color → By last heard / age**: green <24h, yellow 1–7d, gray >=7d or unknown. **Keep days**: 0–365, default 30, with 0 disabling expiry. Cleanup protects favorites, local identities, retained messages/drafts and unknown timestamps. [Controls, storage migration and tests](../../docs/node-age-v0928.md).
- Quick-panel brightness, screen timeout, automatic 180-degree landscape orientation, saved lock and manual flip. To correct an inverted automatic reference, use **FLIP 180**, hold the readable position hands-off for 1–2 seconds, then enable **AUTO ROTATE**. [Orientation procedure](../../docs/rotation-reference-v0927.md).
- Bluetooth/Wi-Fi power settings, local browser messages/radio settings/track tools, manual screenshots, memory snapshots and bounded console diagnostics. Continuous remote touchscreen control is not enabled.

Cached maps work offline; tiles not already cached need Internet access through Wi-Fi. Local GNSS needs the configured receiver. Live position sharing remains future work. Hardware compatibility and battery/RF qualification remain specific to the tested setup.

## Build for development

Use ESP-IDF **5.3.2**, with this pinned upstream board revision:

```sh
git clone https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5.git vendor/ESP32-S3-Touch-LCD-3.5
git -C vendor/ESP32-S3-Touch-LCD-3.5 checkout d9ff7837148b47b33edadc4b87775c341cf35bbe
git -C vendor/ESP32-S3-Touch-LCD-3.5 submodule update --init --recursive
python firmware/display-prototype/apply_to_waveshare.py
cd vendor/ESP32-S3-Touch-LCD-3.5/examples/esp-idf/02_lvgl_example
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_SERIAL_PORT flash monitor
```

The patcher is repeatable and fail-fast. It configures the landscape LCD/touch path, copies project headers, pins the LVGL port, and applies the accepted memory/network defaults. A development rebuild is not a substitute for the checksum-pinned stable binary. Do not use a 3.5B BSP/pinout interchangeably.

## Source map

| Area | Entry points |
|---|---|
| UI/navigation | `meshcrossway_ui.h`, `meshcrossway_navigation_ui.h` |
| Separate BLE/protocol adapters | `meshcrossway_ble.h`, `meshcrossway_meshtastic_protocol.h`, `meshcrossway_meshcore_protocol.h` |
| Maps and preferences | `meshcrossway_map_tiles.h` |
| Age colors/retention | `meshcrossway_node_age.h` and protocol prune/import methods |
| Orientation | `meshcrossway_orientation.h`, `meshcrossway_orientation_policy.h`, `meshcrossway_orientation_sensor.h` |
| SD history/recovery | `meshcrossway_sd_history.h`, `meshcrossway_meshtastic_store.h`, `meshcrossway_history_store.h` |
| Browser services/diagnostics | `meshcrossway_health.h`, `meshcrossway_memory_log.h` |
| BSP integration | `apply_to_waveshare.py` |

Run `python scripts/preflight_display.py` and the relevant protocol/UI/storage suites before a PR. CI also checks the real ESP-IDF image, identity, stack budgets, archive manifests and installer safety. Runtime memory snapshots distinguish internal RAM, PSRAM and DMA-capable memory (a subset of internal RAM); deleting nodes alone does not free fixed table capacity. Continue measured evidence in [issue #137](https://github.com/Renbrant/MeshCrossway/issues/137).
