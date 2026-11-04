# v0.9.12 node-list rendering stability

v0.9.12 is an experimental firmware candidate. The hardware-tested stable release remains v0.8.26.

The v0.9.11 field log showed the ESP task watchdog firing while `taskLVGL` was inside LVGL object redraw. Node-list construction was adding up to 12 cards every millisecond, causing bursts of invalidated display work while Meshtastic and MeshCore nodes were loaded.

The unified Meshtastic/MeshCore list now adds three cards per 25 ms LVGL timer interval. The MeshCore-only list uses the same smaller timed batches for nodes and keeps faster batches for channels. This gives LVGL regular time to refresh the screen between node batches.

The ESP-IDF build and automated preflight should pass before publication. Hardware confirmation is still required before considering this release stable.
