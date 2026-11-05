# v0.9.24: optional node grouping on the map

Map → Options now includes **Group nearby nodes**. Turn it off to draw individual MT and MC markers at their projected coordinates, with their existing size, icon, color and name settings. Tapping a marker follows the individual-node detail path. Turn it on to restore the bounded nearby-group preview. Existing installations keep grouping enabled until the preference is changed.

The choice uses the existing map NVS configuration and its deferred commit, surviving reboot. The blob layout/size is unchanged: schema 4 uses the remaining flag bit. Schemas 1–3 migrate with grouping enabled while retaining their supported map/style settings. Firmware predating schema 4 will not understand the new map blob and may revert map preferences to defaults on downgrade; Wi-Fi credentials, Bluetooth bonds and SD history are separate.

The disabled path builds one single-node drawing entry per existing marker in a bounded linear pass. It reuses the PSRAM overlay arrays and creates no per-node LVGL widgets, new worker or additional marker database. Co-located nodes retain their real coordinates; the option does not spread/jitter positions or declutter overlapping labels. The existing maximum marker count remains unchanged. Overlay invalidation includes the persisted flag, so the mode changes when the existing map overlay refreshes.

This release preserves the v0.9.23 mbedTLS software-AES fix, HTTPS/certificate checks, RX memory optimization, diagnostic budgets and accepted node-list/manual-refresh behavior. v0.9.16 remains the immutable stable recovery; v0.9.24 is a candidate until physical validation.

Validation: real-LVGL tests exercise the switch off/on, actual configuration save/reload through the mock NVS API, schema migration and style preservation, and all 640 marker slots co-located. Off must produce 640 individual entries at unchanged positions and open a single-node detail; on must restore one group. Existing bounded four-row group, map download/cache/cancellation, navigation, chat, touch and layout regressions are retained. Full ESP-IDF compilation and generated configuration checks precede publication.

Physical acceptance: disable **Group nearby nodes**, confirm individual MT/MC icons at both wide and close zoom levels, tap a marker, leave/reopen the page and reboot to verify the saved switch. Re-enable it and confirm numeric groups return. Repeat uncached map pan/zoom with both radios active and capture console if there are download or responsiveness problems.
