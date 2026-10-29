# MeshCrossway v0.7.11 — application stability audit

Audit date: 2026-10-29. Baseline: v0.7.10 / `81e55a5`.
Scope: the physical Waveshare ESP32-S3 application, its BLE Companion client,
LVGL UI, SD journal, Wi-Fi, manual captures, reboot behavior and release checks.
Meshtastic and MeshCore remain independent; no bridge or cross-network sending
is introduced. Maps/GNSS and other preview-only features remain outside this fix.

## Evidence and confidence

The owner reports intermittent resets, Bluetooth returning off, failed sends,
temporary unresponsiveness and inconsistent SD status. A screenshot shows
`Message (up to 0 UTF-8 bytes)` with an enabled SEND button. The submitted SD
archive validates as four complete transactions containing five messages.

There is no reset backtrace or crash dump in those attachments. Accordingly,
memory pressure, burst overflow and excessive redraw/replay are established
code risks, not a proven identification of every physical reset. The release
adds measurements needed to distinguish allocation failure, watchdog, panic,
brownout and other reset reasons on the actual device. A crash breadcrumb is
the activity interrupted by a reset; it is not itself proof of the cause.

## Findings, corrections and verification

| Area | Finding | v0.7.11 behavior / verification |
|---|---|---|
| Dynamic UI memory — high | v0.7.10 moved static chat buffers to PSRAM, but inherited `LV_MEM_CUSTOM=y` with generic malloc/realloc. Small UI objects, text and styles could still occupy or migrate into internal RAM needed by BLE, stacks and SD DMA. | LVGL uses an explicit PSRAM allocator for both allocation and reallocation. Failure retains the old realloc block; there is no internal-RAM fallback. LCD DMA buffers retain their existing internal allocation. Production allocator compiled into actual LVGL host tests; repeated 96-message rendering verifies bounded live allocations. Generated sdkconfig must select this allocator. |
| RX bursts — high | A 16-frame mailbox, draining four frames per 50 ms tick, left little room for contact-sync bursts or delayed UI processing. Overflow disconnected the session. | Bounded 128-frame PSRAM mailbox, up to 16 frames per tick, peak/overflow counters. Real transport test queues and drains 100 frames without fault, then verifies explicit rejection beyond capacity. No silent packet loss or automatic replay. |
| UI work — medium | Every client generation could reconstruct lists and chat cards, including generations caused by command/telemetry traffic. | Automatic reconstruction is limited to four times per second and deferred while a finger is pressed. UI latency and stack low-water are measured. This bounds redraw frequency, not a physical frame-rate guarantee. |
| Send state — high | An offline/reset session has no negotiated payload. Composer showed zero bytes while leaving SEND enabled and returned one generic error. | Useful local draft limit, disabled SEND while disconnected/synchronizing/restoring/invalid-clock/invalid-destination/full-queue, explicit reason, and live re-enablement after reconnection. Draft remains local. The final send boundary independently repeats network/radio/destination checks. |
| Bluetooth after reboot — high | Boot resume depended entirely on an already bonded peer. Explicit enabling without a usable saved peer was not persisted independently. | Successful BLE initialization stores the enabled intent in NVS from the init worker. Boot retains the existing 12-second startup grace, then enables BLE for saved intent or migrates an existing bonded profile. Only a known authenticated peer is auto-selected; fresh devices stay opt-in and deliberate disconnect pauses retries for that session. Production resume functions tested with fake NVS/time. |
| SD write workload — high | Each update replayed and hashed the entire journal. Incoming captures bypassed the intended failed-write backoff. Many tiny transactions could also accumulate considerable restart replay CPU before the byte quota. | Last confirmed state/cursor held in existing PSRAM buffers; changed-record appends, no-op suppression, capture coalescing and a five-second failed-write interval measured after completion. Compaction is bounded by both the 1 MiB quota and 64 transactions. Tests exercise continuous edits during failure, bounded replay and recovery. |
| Reconnect/restore data loss — high | A newer failed-write snapshot could be displaced by older disk history on reconnect. A failed pre-attach capture could still be followed by model reset. A late load during a faulted bootstrap could permit saving an incomplete model. | Same-radio unsaved retry snapshot wins over disk. Attachment waits for successful snapshot handoff. Faulted/unfinished bootstrap does not become save-ready. Tests cover capture allocation failure and a late archive after a bootstrap fault. Restore never reconstructs a TX queue. |
| Error recovery — high | A read error could be confused with a corrupt tail; one SD label covered absent media, allocation, validation and I/O errors. | Read failures cannot authorize truncation/replacement. Actual torn tails recover only to validated commits. Chat stage/errno/count/last/max duration are separate from Wi-Fi backup and encryption state. |
| Diagnostics — medium | Total free heap could hide internal heap exhaustion; reset and last activity were absent from remote status. | `/health` reports internal free/largest/minimum block, allocation failure count/size/caps, reset reason, warm boot count, prior activity, UI peak time/stack and mailbox counters. Allocation hook records counters only. RTC breadcrumbs contain no messages, keys or locations and cause no flash writes. |
| LVGL ownership / screenshots | Checked event, timer and HTTP boundaries. Main UI/model mutations are on the LVGL task. HTTP captures already use a bounded single-shot mailbox. | Thread ownership preserved. Capture remains manual and read-only with the existing rate limit/circuit breaker. No remote input or continuous capture added. |
| Native protocol / ACK | Checked serialization, initialization, frame bounds, one in-flight command, ATT/application completion and matched ACK handling. | Existing production sanitizer tests retained. Restored outgoing work is uncertain, never queued; only a correlated direct-message ACK claims delivery. Channels do not claim universal delivery. |
| Build/recovery | A successful host suite alone cannot show SDK compatibility or reserve runtime internal memory. | Full ESP-IDF 5.3.2 build required before merge. Generated config verifies allocator, stack canaries/watchpoint and coexistence settings; ELF still guards static chat memory. Compiler emits stack-usage evidence. v0.7.8 bytes/hash remain immutable. v0.7.10 is withdrawn from the installer. |

## Public implementation guidance and benchmark

The review combines the following primary sources. WADAMESH was used as a
benchmark for the behavior and responsibilities of a field console, alongside
the vendor/SDK guidance; no WADAMESH source or assets are incorporated.

- [Espressif heap allocation, ESP32-S3 / IDF 5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/mem_alloc.html): capability masks and preserving memory requirements across reallocations.
- [Espressif RAM use](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/performance/ram-usage.html): internal/static/heap separation, task stack budgets and low-water measurement.
- [Heap debugging](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/heap_debug.html): allocation failure hooks, largest free block and minimum free heap. The hook must avoid allocation and blocking work.
- [Watchdogs](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/wdts.html): short critical sections and bounded/yielding work. This fix does not disable watchdogs or raise timeouts to conceal stalls.
- [LVGL 8.4 thread ownership](https://lvgl.io/docs/open/8.4/porting/os) and [LVGL custom allocator source](https://github.com/lvgl/lvgl/blob/v8.4.0/src/misc/lv_mem.c): events/timers own UI operations and the custom allocation hook is configured in the actual LVGL component.
- [MeshCore official Companion protocol](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol), plus the [pinned dispatcher already used by this client](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/MyMesh.cpp): negotiate device limits, serialize commands and distinguish radio acceptance from remote ACK.
- [WADAMESH guide](https://wadamesh.com/docs.html): persistent chat/contact data, clear chat state and stable/test release distinction. Reviewed source at commit `26578cdc87896c78d533c00568846ecdd7dcfb2b`, including [UI allocation](https://github.com/ALLFATHER-BV/wadamesh/blob/26578cdc87896c78d533c00568846ecdd7dcfb2b/src/LvglPsramAlloc.cpp), [application responsibility split](https://github.com/ALLFATHER-BV/wadamesh/blob/26578cdc87896c78d533c00568846ecdd7dcfb2b/WADAMESH_SPLIT_PLAN.md), and [buffered file guidance](https://github.com/ALLFATHER-BV/wadamesh/blob/26578cdc87896c78d533c00568846ecdd7dcfb2b/src/helpers/BufferedFileCommit.h). Its integrated-radio/Arduino targets differ from this external-radio ESP-IDF BLE central. New MeshCrossway code uses public ESP-IDF/LVGL APIs; no GPL helper is imported.
- [IDF 5.3.2 SDMMC writes](https://github.com/espressif/esp-idf/blob/v5.3.2/components/sdmmc/sdmmc_cmd.c): the native driver already handles non-DMA-capable source buffers through an appropriate temporary buffer. Arduino/SPI-specific changes are not applied blindly to the native SDMMC port.

## Acceptance on the physical device

Install v0.7.11 with **Erase unchecked**, retaining the SD copy. Check DM and
channel send/receive/ACK, 96-message scrolling, sustained incoming traffic with
Wi-Fi and captures, and at least a 30-minute session. Save numbered messages,
wait for journal-saved, restart with the radio initially off, and compare history.
BLE may remain off during the intentional first 12 seconds of boot; afterwards
it should enable and reconnect only to the saved authenticated peer. Repeat
disconnection/reconnection and SD-absent startup, removing/reinserting the card
only while powered off. No old message may transmit automatically.

If a reset recurs, preserve serial output from before the reset and `/health`
after restart. Record reset_reason, previous_activity, internal_min,
allocation_failures, ui_max_ms, ui_stack_min and history stage/errno/timing.
Those measurements determine the remaining hardware cause; the test suite does
not certify that spontaneous resets are eliminated.
