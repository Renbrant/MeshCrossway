# v0.8.13 remote navigation resets and missing browser folders

The owner's new log contains two stack-canary crashes on CPU1 while using the remote interface. Both have the same application chain. ELF SHA-256 `4088aab935f600152f0aa4edccff1d16cc70d9586c9901f63eba4839eed55b02` from PR #80 run `41214134672`, artifact `13047413408`, matches the device's `4088aab93`.

Decoded chain:

`HTTP parser → httpd_uri → mcxnavhttp::state → std::to_string(double) → __to_xstring → vsnprintf → newlib floating formatter`

The failing line serializes each saved track's distance in `/navigation`. The target reports a 1376-byte state handler frame, with a further dynamically sized libstdc++ conversion buffer and newlib's float conversion beneath it, on a 4096-byte HTTP task. HTTP stack corruption also explains the unreadable task name printed by the panic handler. The endpoint is used to refresh both tracks and folders, including after a folder command. The SD command can finish successfully and appear on the device while its following browser GET crashes before returning the folder catalog.

The previous BLE resume correction is not the crashing path in this log. BLE resume reports 3852 bytes minimum free out of 6144. The navigation worker reports 5932 bytes minimum free out of 8192.

## v0.8.14

- Replace chained navigation JSON strings with a bounded 32 KiB PSRAM writer. The route preview uses the same writer with a 96 KiB cap.
- Convert distances to three decimal places and route coordinates to seven using bounded integer/fraction digit output, avoiding libc floating formatting and libstdc++'s dynamic stack buffer. JSON escaping preserves UTF-8 and escapes controls. Non-finite/out-of-range numeric values become JSON null.
- Return 503 on allocation or response-capacity failure, releasing snapshots and buffers. Saved files are unaffected.
- HTTP stack becomes 8192 bytes. Every route wrapper samples the actual task high-water mark after processing and logs the first or critically low minimum.
- Add target compiler frame checks for the HTTP handlers, with conservative SDK/FATFS/newlib and HTTP parser reserves. This is a budget guard, not a full SDK/ROM call-graph proof.
- After a browser folder command, wait up to 15 seconds for its specific effect in a complete snapshot. An HTTP 202 means queued, not saved. Select a newly confirmed folder; suppress duplicate commands while waiting. A timeout reports lack of confirmation rather than success.
- Navigation refresh is single-flight. Temporary HTTP or malformed responses preserve the last complete list. The Tracks view refreshes regularly to see changes made on the device, and unchanged snapshots do not rebuild editors.

## Verification and acceptance

Host sanitizer tests run 200 production HTTP responses with all 32 tracks and all 16 folders, UTF-8/escaping, decimal edge cases, SD folder reload, allocation/buffer/busy failures and release checks. Production browser functions are tested with deterministic DOM/time/HTTP adapters for delayed saves, temporary errors, partial snapshots, rename/move/delete, duplicate suppression and unconfirmed timeouts. A full browser test is extended with the same scenarios; browser execution is unavailable in this environment because its Chromium download fails.

Full ESP-IDF compilation must pass the BLE, navigation and new HTTP stack guards. On hardware, repeatedly open Tracks, create/rename/move/remove folders, load route previews and download GPX while checking the HTTP minimum-free-stack log. Physical acceptance remains required. v0.8.11 stays the exact recommended stable binary; v0.8.13 is known-issue and v0.8.14 is experimental.
