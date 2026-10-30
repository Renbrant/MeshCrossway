# v0.8.9 — navigation startup stack hardening

The owner reports a repeatable boot loop in v0.8.8. The supplied USB log shows
the SD card mounting successfully, followed by a software panic with
`Backtrace: 0xfffffffe:0x80386d2e |<-CORRUPTED`. The corrupted trace cannot
identify the crashing task or prove a specific source line. Stack exhaustion
in the navigation startup/recovery path is the leading hypothesis, not a
confirmed hardware diagnosis.

## Changes

- Increase the navigation worker's **internal** stack from 4096 to 8192 bytes.
  Bulk track data still lives in PSRAM. ESP-IDF task stack sizes are bytes.
- Put command processing in a separate non-inlined function so command-only
  temporaries do not inflate the startup/load call chain.
- Log navigation worker startup, entry/exit of SD loading and minimum remaining
  stack. Expose `navigation_stack_min` in `/health` (zero before measurement).
- Task-creation failure remains retryable without throwing away allocated
  navigation state or reporting a phantom running worker.
- Audit real target compiler `.su` files during firmware builds. Reserve the
  worker/dispatch frames plus the three largest storage frames and 4096 bytes
  for filesystem/newlib calls. This is a conservative budget guard, **not** a
  complete call-graph proof. Preserve reports alongside ELF debug artifacts.

Track files, preferences and radio behavior do not change.

## Validation and remaining limits

Host LVGL/navigation tests exercise recording, naming, deleting, overlays and
the task-allocation failure/retry path. A separate ESP32-S3 GCC 13.2 compile of
the navigation code with peripheral stubs produces a 6896-byte conservative
reserve, accepted by 8192 bytes and rejected by 4096. This is not a measurement
of the owner's filesystem call stack. The full ESP-IDF build runs the same
guard against its own compiler reports.

Physical acceptance: cold boot and warm reboot with the existing SD and saved
tracks; keep running for several minutes with Wi-Fi and both BLE companions;
open the map, record/pause/finish/rename/export/delete a disposable track, and
check the navigation high-water mark. If a reset recurs, capture the entire
USB output starting before the first panic, including the new `[MCX-NAV]`
lines. Use the v0.8.9 ELF matching that image to decode any usable addresses.
v0.8.9 remains a candidate until this physical test passes; v0.8.2 remains the
immutable rollback image.
