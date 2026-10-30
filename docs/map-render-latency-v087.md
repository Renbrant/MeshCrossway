# Map rendering latency — v0.8.7

This candidate addresses three observed software bottlenecks:

- Every GPS poll invalidated the complete map canvas even when nothing changed.
- Every LCD draw stripe repeated Mercator projection for every track vertex.
- A pending map request blocked drag input, and release reset the preview image
  to its original position before the requested frame was available.

The UI now caches integer screen coordinates for track segments and POIs in a
bounded PSRAM allocation. Projection happens when map geometry changes, outside
the draw callback. Stripe rendering skips segments outside its clip rectangle.
Status-only navigation revisions do not invalidate this geometry cache.

Map polls no longer invalidate the canvas unconditionally. Visibility flags are
changed only when needed: LVGL 8 invalidates objects even when its hide/show
setter repeats the existing state. Navigation panels defer map cache work while
they cover the centre column.

Dragging remains enabled while map tiles load. The shifted image and overlays
remain at their preview position after release. The next accepted frame replaces
that preview; stale or mid-gesture results do not jump the image. Accumulated
preview offsets are bounded to keep LVGL coordinates in range. Newly exposed
areas may still show the map background until their tiles have loaded.

The pinned display driver, SPI clock, DMA buffer sizes, and BLE settings are
unchanged. Partial-buffer LCD drawing can still expose a physical stripe update;
this change reduces avoidable rendering work and input blocking, rather than
claiming tear-free hardware output.

Real-LVGL tests verify no display transfers during repeated idle polls, retained
pan previews, another gesture during an outstanding request, and no reprojection
on repeated stripe redraws or status-only updates. Physical frame-rate and
screen-transfer latency still need validation on the Waveshare.
