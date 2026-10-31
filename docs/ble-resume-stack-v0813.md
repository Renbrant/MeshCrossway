# v0.8.12 BLE resume crash: analysis and v0.8.13 fix

The owner's log records eight `Stack canary watchpoint triggered (mcx_ble_resume)` crashes from 08:59:06 to 09:01:07, normally about 17 seconds apart. The task waits 12 seconds after boot and another 3 seconds before trying its saved radio, consistent with the observed timing.

## Exact binary evidence

The retained symbols come from successful PR #79 run `41140087805`, debug artifact `13018182990`. The ELF SHA-256 begins `69fa4f59a`, matching the device log.

The decoded application call chain is:

`auto_resume_task → reconnect_remembered → connect → connect_selected → ble_gap_connect → esp_log_writev → vprintf → _vfprintf_r`

Target compiler `.su` reports for v0.8.12:

| Function | Frame bytes |
| --- | ---: |
| auto_resume_task | 512 |
| reconnect_remembered | 512 |
| connect | 512 |
| connect_selected | 576 |
| scan (alternate retry path) | 1008 |
| request_start (initialization path) | 528 |

The four nested application frames alone retain 2112 bytes of a 4096-byte task stack before NimBLE, logging/newlib and RTOS frames. PR #79 split `connect` into a wrapper plus `connect_selected`, retaining an extra full Snapshot in this nested path. The additional frame exhausted the already-small SDK/logging margin. The crash occurs inside NimBLE logging during `ble_gap_connect`; this is not evidence that GPS, tracks or HTTP caused it.

## Correction

- Phase checks copy only the phase under the existing lock.
- Native and remote connect paths capture one Peer under the lock, preserving the actual address if scan rows change.
- Saved-radio reconnect captures one Peer and calls `connect_selected` directly; no nested full Snapshot copies remain.
- Scan, startup and saved-name lookup also avoid unnecessary full snapshots.
- Resume stack is 6144 bytes, with minimum-free-stack diagnostics after direct and scan attempts. ESP-IDF high-water marks are bytes.
- A target compiler guard conservatively sums application control frames plus 4096 bytes for SDK/logging. It rejects missing/unbounded reports, excessive total usage and individual control frames above 256 bytes. This is a conservative budget, not a full SDK/ROM call-graph proof.

Host tests execute the actual connection helpers for both protocols, repeated retries/failures, identity normalization, peer reorder during cancellation, active-link rejection and manual pause. The old .su failure shape is rejected by the new guard. Full ESP-IDF compilation validates the actual new target frames.

v0.8.11 remains the immutable stable default. v0.8.13 is experimental until repeated power cycles and reconnects to the owner's MC/MT radios confirm adequate runtime minimum stack and no restart. Install without Erase. Networks remain independent and only one BLE session is active.
