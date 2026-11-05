# v0.9.21: allocation failure attribution

Follow-up to [memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137). The v0.9.20 field capture contained one failed 124-byte request with capability mask 2060 (`0x80c`: INTERNAL | DMA | 8BIT), during a map-render interval. No watchdog or touch overflow was reported in that approximately five-minute capture. Timing alone does not identify the allocating component.

## Diagnostic behavior

The existing ESP-IDF allocation-failure callback now copies uptime, requested bytes/capabilities, CPU core, interrupt context, current task name and the allocation API name into a four-entry fixed internal-memory ring. Record storage is 288 bytes plus its lock and indices. Internal storage is intentional for callback/ISR access; no PSRAM or heap allocation is attempted by the callback. The existing 2,944-byte PSRAM sample ring remains unchanged.

The callback does not format or print text, query/walk the heap, inspect stack high-water marks, access the SD, create tasks or retain pointers to task structures. Task identity is queried only outside interrupt context and only when a current task handle exists. ISR records say `ISR`; pre-task records say `startup`. Task names are limited to 15 characters; API names to 31. Whitespace, commas and non-ASCII characters become underscores. Names are copied before returning, so deleted tasks cannot leave dangling pointers in deferred output. A separate ISR-safe critical section protects the ring on both cores; no output occurs with that lock held.

The existing BLE coordinator drains at most two failure records per scheduled flush, alongside the existing at-most-four memory samples. New lines start with `[MCX-ALLOC] v0.9.21` and include `boot`, `seq`, `uptime_ms`, `bytes`, `caps`, `core`, `isr`, `task`, `function` and `dropped`. Normal `[MCX-MEMLOG]` CSV columns stay unchanged. Failed output keeps the record for retry; failure-ring overflow discards the oldest record and increments `dropped`. Console failure can cause retried/duplicate lines: identify records by boot and sequence. Records are volatile and can be lost at reset or if output cannot drain. The diagnostic remains disabled if the existing PSRAM logger cannot start.

`function` comes from ESP-IDF's `function_name` callback argument, which identifies an allocation API, such as `heap_caps_malloc`. It is **not** a source-level caller, return address or backtrace. Task names can also be shared. This narrows the investigation without claiming an exact application call site. If needed, add targeted component instrumentation after observing the responsible task; do not add an unbounded stack walk in the failure hook.

The v0.9.20 map preview, accepted node layout/manual refresh, native radio bootstrap and stable v0.9.16 recovery image are preserved. This release diagnoses the failure; it does not claim to fix its underlying cause.

## Test procedure

1. Install v0.9.21 and open Logs / Console. Keep the complete boot capture through the end of testing. Do not intentionally reset during capture; note any manual reset or console reconnect separately.
2. Connect both independent BLE radios and Wi-Fi. First leave the device on Home for two minutes to establish a baseline.
3. Open the map, pan and zoom to request tiles, open/close dense node-group previews, and use their zoom action. Note the operation when a visible delay occurs. Test repeated tile loads with both radios active, then repeat using cached tiles. Avoid changing radio or GPS configuration in the same comparison.
4. Navigate Home / Nodes / Channels / Chats and back to the map; keep the log for at least 15–30 minutes. Send the entire log, including `[MCX-ALLOC]`, `[MCX-MEMLOG]`, reset and watchdog lines.
5. Check that failure lines identify a bounded task/API/context, the failure count agrees with the existing counter, and `dropped` remains zero. Watchdogs, resets, allocation failures and touch overflows are separate outcomes; report each. No observed failure in a short run is not proof of a resolved leak or long-term stability.

## Validation

Host sanitizer tests exercise task, ISR and startup attribution, bounded name copying/sanitization, callback exclusion of heap queries/allocations and console I/O, burst overflow, fixed drain budget and failed-output retry. Existing memory CSV/ring/cadence and runtime/health regression checks are retained. Full ESP-IDF compilation and stack/config checks are required before merge; physical validation follows installation.

API references: [ESP-IDF v5.3.2 allocation callback contract](https://github.com/espressif/esp-idf/blob/v5.3.2/components/heap/include/esp_heap_caps.h) and [Xtensa ISR-safe critical sections](https://github.com/espressif/esp-idf/blob/v5.3.2/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos/portmacro.h).
