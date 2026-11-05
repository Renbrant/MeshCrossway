# v0.9.20: bounded map-cluster previews

Follow-up to [memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137), based on the supplied v0.9.19 field console log.

## Field evidence

The complete observed boot contains 65 valid memory rows covering uptime 723–307,159 ms. No allocation failure, diagnostic-ring drop or console-output failure was recorded. Memory sampling took at most 186 microseconds in the supplied records. Internal free memory reached 3,651 bytes during concurrent activity and ended at 6,823 bytes; PSRAM reached 4,485,712 bytes and ended at 5,012,784 bytes. These sparse samples cannot capture every transient minimum or establish long-term absence of leaks.

The observed minimum unused task stacks were UI 832 bytes, history 3,096, navigation 4,228, GNSS 2,136, BLE coordinator 3,744, NimBLE host 376, map worker 1,524, HTTP 4,360 and touch 1,132. UI and NimBLE have little measured headroom; do not reduce their stacks based on this run.

At console times 10:23:48, 10:23:53 and 10:23:58 the task watchdog reported IDLE1 starvation with taskLVGL executing on CPU1. The touch queue subsequently reached 64 entries and recorded one overflow. The device continued reporting afterward. An earlier boot boundary reports USB_UART_CHIP_RESET, which is a different reset cause from a task-watchdog panic; the log alone does not identify who initiated that USB/UART reset.

Address resolution using the v0.9.19 ELF retained in successful [preflight run 42059704524](https://github.com/Renbrant/MeshCrossway/actions/runs/42059704524), artifact `ble-display-debug-symbols`, maps the first watchdog stack to:

- `0x4214C022`: `lv_txt_utf8_next`, lv_txt.c:605
- `0x4207AD55`: `lv_txt_get_next_word`, lv_txt.c:189
- `0x420717CB`: `lv_draw_label`, lv_draw_label.c:160
- `0x42070882`: `lv_obj_redraw`, lv_refr.c:148
- `0x42070F6A`: `refr_area`, lv_refr.c:692
- `0x420711EA`: `_lv_disp_refr_timer`, lv_refr.c:324

This supports a text-layout/rendering bottleneck at the reported interaction; it does not prove that every historical watchdog/reset has the same cause.

## Change

The old cluster dialog assembled up to 48 descriptions into one wrapping label using a static 5,400-byte buffer. The generic dialog's deferred flex pass also enabled wrapping and content-sized height. A dense zoomed-out group therefore created a large repeated text-layout/rendering workload.

The new preview creates at most four fixed-size cards, each with two short clipped labels. The dialog uses fixed positions and disables the generic flex/wrap pass for this screen. It displays the total group size and an explicit zoom action, centered on the cached cluster's screen centroid, advancing up to two zoom levels within the provider limit. Close works without requesting new tiles. The zoom button is disabled at the provider limit.

Opening the preview only reads the existing bounded overlay cache and stops after four matching nodes. It does not fetch radio data, read the SD card or create a widget for every contact. Marker clustering itself and the protocol databases keep their existing bounds. At the maximum zoom level, co-located nodes remain represented by a bounded preview; it is deliberately not an exhaustive group browser.

One pointer click now opens only the first matching cluster. The large static text buffer is removed; only small zoom-target state is retained. Exact net static-RAM savings should be taken from the generated build report.

## Validation

Host real-LVGL regression coverage populates all 640 available protocol node slots into one group, repeatedly opens it at body widths 276 and 400, checks constant widget count and bounds, single-line clipped text, control-character sanitization, and no additional map request on opening. It checks explicit zoom behavior. Existing map, GNSS, navigation, radio/chat and touch integration tests continue to run.

Physical acceptance: repeat the same zoomed-out cluster interaction and rapid navigation from the v0.9.19 log, capture the console, confirm prompt open/close/zoom, no new taskLVGL watchdog, and no touch overflow. Compare equivalent internal-memory stages. The previous node-page layout/manual refresh remains the reference behavior. v0.9.16 stays the stable recovery image.

## Cached radio synchronization: separate protocol work

The current SD stores already contain MeshCore contacts/channels/history and Meshtastic node/channel/history data, associated with radio identity. Offline restoration exists. Reconnection still uses a complete bootstrap: MeshCore clears the contact table and sends command 4 without a `since` argument; Meshtastic sends its random PhoneAPI `want_config_id` and waits for config completion before merging history and receiving the message backlog.

The [official MeshCore Companion protocol](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol) supports an optional `since` value and returns `most_recent_lastmod` in end-of-contacts. A correct incremental implementation needs an identity-bound, complete committed cache plus a durable synchronization watermark. Advance that watermark only after a valid complete exchange and successful cache commit. Validate advertised counts and behavior against the radio firmware. Handle deletion, table replacement, clock rollback/reset, cache truncation/capacity, unsupported responses and interrupted transfer; retain a controlled full-refresh fallback. A partial recent-node list alone is insufficient as a complete incremental database.

The current Meshtastic adapter does not implement an equivalent modification-watermark query. A `want_config_id` nonce identifies a configuration transaction; it is not a cache revision. Any optimization must be checked against the actual Heltec/T-Deck firmware and native PhoneAPI capabilities, with cached values clearly distinguished from live authoritative settings. Do not skip identity, channel/security or live-message initialization based on an SD snapshot.

This map fix leaves both protocol bootstraps unchanged so its performance effect can be evaluated independently.
