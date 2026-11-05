# v0.9.19 memory timeline diagnostic

Tracks [issue #137](https://github.com/Renbrant/MeshCrossway/issues/137). This candidate adds evidence collection to v0.9.18. It does not enable new network allocators, resize task stacks, alter core affinity or change node-list behavior. v0.9.16 remains the tested recovery release.

## Build evidence

The normal ESP-IDF build now produces `build/memory-report/size.txt`, `components.txt`, `files.txt` and `effective-config.json`. The PR's `ble-display-debug-symbols` artifact retains those reports, ELF, map, effective sdkconfig, dependency lock, stack-usage files and generated main.cpp. Confirm actual native display width/buffer dimensions in the generated source and `[MCX-LCD]` boot output. A rotated screen resolution is insufficient to calculate DMA reservations.

## Runtime evidence

`MCX_MEMORY_DIAGNOSTICS=1` is enabled explicitly for the application component. A fixed 32-record, 2,944-byte PSRAM ring is allocated once after the display starts and before GNSS/navigation initialization. The logger has no internal-memory fallback and creates no task. Allocation failure disables it and prints a serial notice.

The existing BLE coordinator samples periodically and drains the console queue between its normal operations. It emits up to four rows per poll, rate-limited to at most one batch per second (normally one poll per three seconds). Before Bluetooth starts, the coordinator polls once per second even when Bluetooth is disabled. Blocking BLE operations may delay output. Capture points query aggregate free bytes for three capabilities; they do not scan the largest block or perform heap tracing. Sampling duration is recorded. Event captures also run at startup, page changes, map rendering, BLE GAP connect/disconnect and Wi-Fi disconnect/IP acquisition. These snapshots describe timing, not allocation ownership.

Application stack watermarks are collected from each task itself at most every 30 seconds, or reused from existing measurements. The observed slots are UI, SD history, navigation, GNSS, BLE coordinator, NimBLE host (GAP callbacks), map worker, HTTP (requests) and touch. Unobserved slots are `4294967295`; they must not be interpreted as free RAM. This does not inventory SDK/system-task stacks. Idle HTTP/NimBLE tasks may retain older observations until their next request/callback.

Output uses the existing serial console with the prefix `[MCX-MEMLOG]`. There is no diagnostic filesystem I/O and no SD-card requirement. Open the Web Flasher serial console or another serial monitor before resetting the controller and save the complete output on the computer. Samples include their capture uptime, so delayed printing does not change their timestamps. The pre-display boot output remains the existing BSP/ESP-IDF log.

A fixed queue prevents UI/network/BLE callbacks from waiting for serial transmission. If producers fill it, the oldest row is overwritten and `dropped` increases. A short/failed console write retains the row for retry and increments `output_errors`. Console output itself can affect timing; compare an unchanged v0.9.18 run with the same console attached. USB disconnects, abrupt resets and bounded queue overflow can lose records. Reject malformed lines and deduplicate retries by `(boot, sequence)`. This is a diagnostic stream, not persistent storage on the device.

CSV has no header. Schema (all numeric values decimal, memory/stack values in bytes):

```
[MCX-MEMLOG] version,boot,sequence,uptime_ms,event,detail,internal_free,dma_free,psram_free,allocation_failures,last_failed_bytes,last_failed_caps,sample_us,dropped,output_errors,ui_stack,history_stack,nav_stack,gnss_stack,ble_coordinator_stack,nimble_host_stack,map_stack,http_stack,touch_stack
```

Event codes:

| Code | Event | Detail |
| --- | --- | --- |
| 0 | Periodic sample | 0 |
| 1 | Logger ready, after display initialization | ESP reset reason |
| 2 / 3 | Before / after BLE controller preparation | 0 |
| 4 / 5 | Before / after Wi-Fi startup call | 0 |
| 6 / 18 | Before / after HTTP startup call | 0 |
| 7 / 8 | Before / after NimBLE initialization | After: initialization return code |
| 9 | Page changed | Numeric page index |
| 10 / 11 | Before / after map render | 0 |
| 12 | BLE GAP connect/disconnect callback | Protocol in high 16 bits, GAP event type in low 16 bits |
| 13 | Wi-Fi disconnected | Disconnect reason |
| 14 | Wi-Fi obtained IP | 0; no address/SSID stored |
| 15 / 16 / 17 | Before GNSS / after GNSS / after navigation startup | 0 |

Counters are diagnostic-only. Allocation-failure metadata is an approximate concurrent snapshot. Failed allocation count is also available through existing runtime health diagnostics. The existing manual memory panel remains the way to inspect largest free blocks/minimum heap watermarks; the timeline does not periodically walk heaps to obtain them.

## Physical procedure

1. Retain the unchanged v0.9.18 measurements and recovery firmware. Install this candidate without erasing settings/bonds. Confirm the boot log says `v0.9.19` and `[MCX-MEMLOG]` reports the PSRAM ring.
2. Wait at least two minutes after boot, then exercise MT alone, MC alone and both together. Open nodes, sort, manually refresh and scroll; exercise channels/unread counts and chats.
3. Open maps, load cached/uncached tiles, pan/zoom, record/pause/save a track and use remote status/chat/manual capture. Switch Wi-Fi to the phone hotspot and back; power-cycle one radio at a time.
4. Allow at least 30 seconds after each stage so queued rows can drain. Save the full console log after a two-hour mixed run. Correlate with matching ELF/reset breadcrumbs if a reset occurs. Repeat at least three cold boots; extend a selected candidate to an overnight run as described in issue #137.
5. Compare free internal/DMA memory at equivalent stages, event deltas, stack minima and sample overhead. DMA overlaps internal memory. PSRAM includes the diagnostic ring itself. Report connection drops, map recovery and touch latency alongside memory values.

No hardware stability or memory savings are claimed by the host tests. Follow issue #137 to decide the next single configuration experiment after examining these measurements.
