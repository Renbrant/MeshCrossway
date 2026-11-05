# v0.9.23: software AES for map HTTPS downloads

Follow-up to [memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137) and the v0.9.22 RX-placement trial.

## Field evidence

The supplied v0.9.22 log contains five failures requesting 1,371 bytes with DMA capability (`0x8`) through `heap_caps_aligned_alloc`, task `mcx_map`, core 0, outside ISR context. Each coincides with `esp-aes: Failed to allocate memory` and TLS read failure -0x0001; four sequences also report -0x7180 (MAC verification error). The first follows successful certificate validation. Uncached map regions remained blank in the reported interaction.

The partial boot capture has 60 memory rows covering uptime 13,109–212,197 ms, sampled internal/DMA minima 17,519/9,987 bytes and final values 20,771/13,239. PSRAM sampled minimum is 4,778,444. Diagnostic drops/output errors are zero. No watchdog, backtrace, reset or touch overflow is reported in this capture; touch queue peak is 14. DMA is an overlapping subset of internal memory. Post-event totals do not establish contiguous/aligned availability at the instant of failure and do not prove fragmentation or the exact transient allocation sequence.

The log now identifies the component, not merely the task: ESP-IDF v5.3.2's hardware AES DMA code (`esp_aes_process_dma_ext_ram`) allocates temporary aligned DMA buffers when the input/output cannot be used directly. Its allocation failure returns -1, matching the recorded chain. The failure hook does not capture a source backtrace or negotiated cipher, so do not infer a particular AES mode.

Existing map retries/backoff continue in the log; repeated TLS failures can keep new areas missing. This is not evidence of an absent retry loop. A temporary file is removed after a failed download; incomplete PNG data is not committed to the tile cache.

## Targeted change

Disable `CONFIG_MBEDTLS_HARDWARE_AES` and use mbedTLS software AES for TLS. Its hardware-only interrupt/auxiliary configuration may disappear as a dependency consequence. This removes the implicated mbedTLS hardware-AES temporary DMA path. It does not remove all DMA needs of the display, Wi-Fi or other crypto hardware, and does not claim that every memory shortage is fixed.

Preserve AES/GCM/CCM algorithm support, TLS 1.2, certificate bundle, hostname/certificate verification, HTTPS-only map configuration, external mbedTLS memory preference and SHA/MPI acceleration. Do not replace HTTPS with HTTP or bypass MAC/certificate checks. Hardware acceleration changes performance/implementation, not the intended cryptographic protection. Measure CPU/latency and task stack headroom on hardware; software AES is not assumed to have zero cost.

Preserve the v0.9.22 RX placement change and its measured 9,472-byte static shared-DIRAM reduction relative to v0.9.21. Keep Wi-Fi buffers, allocation threshold/reserve, AP+STA, Bluetooth stacks, SD workers, accepted node snapshots and map retry/render/cache logic unchanged. No additional task or buffer is introduced by application code. Firmware/version labels become v0.9.23. Stable v0.9.16 remains immutable recovery.

## Validation and publication

Patcher tests seed inherited enabled AES/RX defaults and verify canonical disabled values after repeated application. The generated-sdkconfig guard rejects re-enabled hardware AES and checks TLS/AES/GCM/CCM support, certificate prerequisites, SHA/MPI, networking budgets and stack/coexistence settings. Existing diagnostic ASan/UBSan, runtime/health, catalog/patcher and real-LVGL integration checks remain required.

Compare the full generated sdkconfig and dependency lock against v0.9.22; distinguish direct AES changes from derived aliases/dependencies. Inspect the compiled AES implementation in the linker map. Record CI/static results in issue #137 before publication. Publish successful candidates under the standing authorization; physical results remain pending until a new device log is supplied.

## Physical acceptance

1. Install v0.9.23 and capture the full console from boot, noting any manual reset/reconnect.
2. Keep both BLE radios and Wi-Fi active. Repeat the pan/zoom into the uncached areas that failed on v0.9.22; test at least 20 newly needed tiles at normal interactive request pacing. Do not bulk-prefetch or change tile provider to hide the failure.
3. Verify new tiles actually appear and remain usable from SD after returning to the area. Test cached versus downloaded tiles separately. Open/close dense node groups and change zoom.
4. Repeat with remote HTTP access, router→phone-hotspot switching, and weak-signal/reconnect conditions. Confirm BLE stays responsive and SD history/track recording still works.
5. Capture at least 30 minutes and repeat boots. Require no `esp-aes` memory errors, new DMA allocation failures, persistent blank regions after network recovery, watchdogs or touch overflow in the exercised conditions. Check map-worker/UI/NimBLE stack minima and download/UI latency against v0.9.22. Other allocation failures need separate attribution.

If uncached downloads still fail, retain their allocation/task/TLS evidence and investigate the remaining cause; do not disable TLS validation or assume that a higher total free heap proves recovery. A passing compilation is not physical acceptance.

References: [pinned AES DMA code](https://github.com/espressif/esp-idf/blob/v5.3.2/components/mbedtls/port/aes/dma/esp_aes_dma_core.c), [ESP-IDF v5.3.2 mbedTLS options](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/kconfig.html#config-mbedtls-hardware-aes).
