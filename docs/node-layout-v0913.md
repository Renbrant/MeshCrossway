# v0.9.14 node card layout

v0.9.14 is an experimental firmware candidate. The hardware-tested stable release remains v0.8.26.

The v0.9.12 screenshots show node names beside metadata that can wrap to multiple lines. Those text areas occupied the same vertical space, causing names, heard times and message totals to collide.

The previous release assigned each card a manually calculated Y position. On-device screenshots showed that the position calculation still allowed cards to collide. The new release makes the LVGL list container own vertical placement: it uses a column flow, fixed-height cards and a 6 px container gap. Each card retains separate fixed-height, single-line labels. This applies to Meshtastic and MeshCore nodes.

The ESP-IDF build and automated preflight passed before publication. Hardware confirmation is still required before considering this release stable.
