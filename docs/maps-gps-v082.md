# v0.8.2 — map workspace and BLE startup retest

The owner confirmed v0.8.1 renders real maps and the correct local GPS position.
The same hardware showed `NimBLE: ESP_FAIL (-1)` before scanning. No serial log
from that failure is available, so its underlying cause is not established.

## Map controls

The map fills the central 286-pixel-high workspace. There is no internal title,
technical subtitle, persistent GPS/tile footer or bottom button row. The global
GPS/satellite indicator remains. The right rail, expanded or collapsed, provides
Zoom, Center GPS, Map options, GNSS and Map details before the other map views.

Zoom toggles a slider **over** the map. Adjusting it previews the previous image
at the requested scale and displays the requested level. Only releasing it loads
the final view; intermediate levels do not initiate map requests. The slider
stays open until Zoom is pressed again. Rebuilding the rails preserves that open
state. The old image is a preview, not new map detail; the position marker is
hidden until the matching view is ready. Pan and GPS follow do not start competing
requests during a slider drag. New requests supersede old results.

Attribution stays visible as small text with a white text halo, transparent
background and no full-width black strip. Tapping opens provider/source credits
and the OSM copyright URL/ODbL information. Configured provider attribution is
preserved. Guidance checked against the OSMF Attribution Guidelines and Tile
Usage Policy; no attribution toggle or hidden-only credit is introduced.

## Loading

An twelve-slot LRU holds decoded RGB565 tiles in PSRAM (1.5 MiB with 16-bit LVGL).
Revisiting those tiles skips PNG decoding and SD reads. Each PNG is decoded once
per cache miss, reusing the validation output for drawing. The persistent PNG
cache and user `/tiles/z/x/y.png` packs remain compatible. Missing files still
need network time on a first online view; no bulk/neighbor/zoom prefetch is added.
Map details shows loading/errors, tile counts, cache hits, PNG decode count and
last composition time. No measured hardware speed-up is claimed yet.

## BLE mitigation and diagnosis

ESP-IDF 5.3.2's pinned NimBLE port maps failures from HCI initialization to
`ESP_FAIL`, which can conceal allocation failures. This does not prove that the
owner's failure was memory exhaustion. The candidate selects the SDK-supported
external-PSRAM allocation mode for NimBLE host buffers; controller/DMA/task stacks
remain internal. Generated sdkconfig must confirm this choice before publication.

A staged initializer reports controller init, controller enable or host/HCI
failure separately, preserving the SDK result and internal-memory diagnostics.
It follows the pinned SDK startup order and cleans up the controller on enable
or host failure. It never repeats a partially initialized host. BLE initialization
runs on core 1 and the map worker yields/cancels HTTP work before it; no bonds,
Wi-Fi settings, messages, eFuses or protocol security are reset or downgraded.
Only one radio remains active; MT and MC networks remain strictly separate.

SDK references:
- https://github.com/espressif/esp-nimble/blob/32b2124f54238e70ecd4c72a753698babdd939f7/porting/nimble/src/nimble_port.c
- https://github.com/espressif/esp-idf/blob/v5.3.2/components/bt/host/nimble/Kconfig.in
- https://github.com/espressif/esp-idf/blob/v5.3.2/components/bt/host/nimble/esp-hci/src/esp_nimble_hci.c
- https://osmfoundation.org/wiki/Licence/Attribution_Guidelines
- https://operations.osmfoundation.org/policies/tiles/

## Validation and hardware acceptance

Host tests cover real LVGL slider behavior/release-only requests, full-height
map geometry, transparent attribution/credits, preview/reset, PNG composition,
cache reuse without a decode, corrupt persistent-cache repair after RAM cache
clear, HTTP backoff, SD absence, cancellation, GPS settings and startup failure
injection/cleanup. Existing touch, chat/protocol and configuration tests apply.
Full ESP-IDF preflight and publication checks are required before installation.

Install **without Erase**. Keep the owner-confirmed v0.7.8 recovery bytes unchanged.
On the Waveshare, verify:

1. Cold boot and BLE scan/pair/reconnect, both before and after opening maps.
2. Slider on/off from either rail mode; drag through many levels and release.
   The map must load only the final target without delayed extra zoom steps.
3. Full map area in all four rail combinations; readable attribution over light
   and dark maps, with credits accessible and no footer/buttons.
4. Online first load and cached/offline revisits, GPS follow/stale fix, touch and
   chat traffic during map use; restart preserves GPS/map settings and SD history.
5. Watch internal heap/DMA, stacks, resets and the actual BLE startup stage if it
   still fails. Compilation is not evidence that the physical BLE bug is fixed.
