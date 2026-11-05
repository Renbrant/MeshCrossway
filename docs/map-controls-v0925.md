# v0.9.25: map controls, night presentation and MeshCore positions

## Behavior

- After a zoom slider release or keyboard commit, the zoom bar stays for two seconds and then closes. Beginning another drag cancels the pending close; the next release restarts the interval. Opening the bar alone does not arm the timeout. The existing LVGL-owned map poll handles dismissal; no new task/timer is created.
- Map Options contains **MESHTASTIC** and **MESHCORE** sections, each ordered **Format → Size → Color**. Size is independent per network, including bounded ID-tag dimensions. A selected manual color also applies to ID tags; Auto retains recent-activity coloring. Shared visibility/grouping settings remain above the sections.
- **Street / OpenStreetMap night** is a local dark presentation of the regular OpenStreetMap tiles. It inverts lightness while retaining hue, with a dark blue-gray palette, during frame composition in the existing map worker. It is not a separate upstream tile style/provider. SD tiles and the RAM tile LRU stay unmodified and are shared with day mode, so existing offline maps work without a second download/cache. Tracks, POIs and marker overlays keep their configured colors. The additional per-pixel processing needs normal physical responsiveness testing.
- MeshCore contact `flags` are no longer interpreted as a GPS-present mask. Both map projection and fallback centering accept valid nonzero-sentinel coordinates independently of favorite/permission flags, retaining geographic range checks. Contacts without a supplied position still cannot be plotted. The earlier `flags & 0x10` condition incorrectly hid ordinary contacts, including those with flags=0. The upstream companion code serializes contact flags separately from gps_lat/gps_lon.

## Persistence and resources

Map configuration schema 5 appends two one-byte fields (MeshCore size and local night presentation); the aligned blob grows from 328 to 332 bytes. The old prefix offset is guarded at compile time. NVS load accepts the exact historical prefix for schemas 1–4 and the current blob size. Migration preserves style/visibility/grouping and initially copies the old shared size to MeshCore; night mode starts off. New choices use the existing deferred NVS commit. Downgrading to older firmware may reset map preferences because the old loader does not understand schema 5; credentials, radio bonds and SD history are separate.

No new marker database, tile cache, per-node widgets, worker or timer is added. Individual/grouped drawing uses the existing bounded arrays. The v0.9.23 software-AES HTTPS fix, RX memory optimization, certificate/hostname verification and diagnostics are preserved. v0.9.16 remains immutable stable recovery.

## Validation

Real-LVGL host regressions cover the zoom timeout before/after two seconds and holding a drag; independent MT/MC control updates in section order; day→night→day rendering from the same cached tiles with no HTTP request or cache-color mutation; MeshCore flags=0 with a valid coordinate and last-four-ID tag, plus an invalid coordinate excluded; old-prefix migration and save/reload of independent size, night and grouping. Local preview inspection checks the MeshCore tag is visible and the map settings remain scrollable. Full ESP-IDF/config/stack and existing map/GNSS/navigation/chat/touch tests precede publication.

Physical acceptance: set MT Cross/red and MC Name/last-four-ID with a different size/color; verify MC contacts with positions appear. Test zoom dismissal and re-drag, day/night/offline mode, setting persistence after reboot, optional grouping, and uncached downloads with both BLE radios connected. Do not infer long-term stability from compilation alone.

Reference: [MeshCore companion contact serialization](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) (`writeContactRespFrame`; flags and GPS fields are independent).
