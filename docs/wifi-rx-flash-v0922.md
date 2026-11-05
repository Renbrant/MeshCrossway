# v0.9.22: isolated Wi-Fi RX code-placement experiment

Related: [memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137).

## Hypothesis and scope

The v0.9.21 field run remained responsive and recorded no allocation failure/watchdog/touch overflow, but sampled internal free memory reached 12,215 bytes and its overlapping DMA subset 4,683 bytes. Reducing Wi-Fi RX code retained in internal executable RAM may create more heap headroom without a functional change to the user interface or radio protocols.

The sole functional configuration change relative to v0.9.21 is `CONFIG_ESP_WIFI_RX_IRAM_OPT`: enabled in A, disabled in B. Firmware identity is v0.9.22. The patcher removes inherited assignments and writes a single canonical unset directive; the build checks the **generated** sdkconfig, not only defaults. It guards the unchanged general Wi-Fi IRAM optimization, RX/TX counts, aggregation window, internal reserve, malloc threshold, dual-BLE settings and security/display prerequisites. The existing memory/allocation console logging retains the same budgets and schemas with the new version label.

This trial leaves AP+STA, PSRAM-first network preference, Wi-Fi buffer counts, Bluetooth/UI task stacks, GPS, SD workers, node snapshots and map rendering unchanged. v0.9.16 remains the immutable hardware-tested recovery image. v0.9.21 is an experimental comparison baseline, not newly certified stable.

Espressif documents lower IRAM consumption with RX optimization disabled, at a potential Wi-Fi performance cost. That generic estimate is not a measurement of this application. Internal free and DMA-compatible free memory must be checked on hardware; DMA is a subset and must not be added to internal free. This trial does not establish the original failed allocation's caller or fix every possible low-memory condition.

## Reproducible baseline

A: v0.9.21 at merged commit `71a3a6fca866ac60a8221fd5c8ea1be25c84ea2b` (same source tree as preflight head `ae4089a67433838e3c30127e58f4646d59ce4a16`). Baseline generated config, linker map and size report are retained in [run 42080212701](https://github.com/Renbrant/MeshCrossway/actions/runs/42080212701), [debug artifact 13440068245](https://github.com/Renbrant/MeshCrossway/actions/runs/42080212701/artifacts/13440068245); the compiled baseline installation archive is [artifact 13441475018](https://github.com/Renbrant/MeshCrossway/actions/runs/42080212701/artifacts/13441475018). Download and retain the A archive before its artifact expiry if a repeated comparison is needed. The regular flasher selects the stable recovery and newest candidate, so do not assume A remains in that selector after B is published.

Baseline `idf.py size`: shared DIRAM 171,716 bytes (text 110,735; BSS 35,008; data 25,860, plus alignment); dedicated IRAM 16,383. Static report percentages are not runtime heap utilization. ESP-IDF v5.3.2, pinned Waveshare BSP `d9ff7837148b47b33edadc4b87775c341cf35bbe` and the dependency lock must match when comparing builds.

B build results and the full generated-config comparison are recorded in the issue/PR after CI. Compilation alone does not establish runtime memory gains or Wi-Fi/BLE performance.

## Physical A/B procedure

1. Capture A first (or use the existing A capture for a preliminary comparison). Record firmware, radio firmware, router/hotspot and SD contents. Keep comparable cache contents and locations. Capture complete boots and avoid erasing credentials/bonds between profiles. Note manual resets and console reconnects separately.
2. Connect both BLE radios and the same Wi-Fi. Leave Home idle for two minutes, then use Nodes/manual refresh, Channels and Chats. Compare matched boot/connection/page milestones after initial synchronization settles.
3. Open the map and repeat a fixed pan/zoom route: one pass through cached tiles, one through uncached tiles, then dense-group open/close and the explicit zoom action. Record missing tiles and visible delays; worker render durations are not automatically UI stalls.
4. Repeat with remote HTTP access active. Exercise both router/STA access and direct SoftAP access. Switch from the router to the phone hotspot and require **tile downloads** to resume, not only successful Wi-Fi association. Keep both radios connected.
5. Test SD history, track recording and credential save/reconnect; reboot and confirm restoration. Include weak Wi-Fi and reconnects. Avoid unrelated configuration changes during the comparison.
6. Capture at least 30 minutes per profile and repeat matched boots. Preserve `[MCX-MEMLOG]`, `[MCX-ALLOC]`, touch, BLE, reset and watchdog output. Compare minimum/final internal and DMA free values at equivalent stages, failures/drops, UI/NimBLE stack headroom, touch queue and overflows, BLE disconnections, tile completion and UI response.

Accept B only if matched-stage memory headroom improves and the exercised features retain acceptable response with no allocation failures, watchdogs, unexplained resets or touch overflows. If map downloads or BLE/HTTP reliability regress, reject B and return to the comparison/recovery image; do not compensate by changing another parameter in the same trial. A short failure-free test is evidence only for the exercised conditions.

## References

- [ESP-IDF v5.3.2 RAM/IRAM guidance](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/performance/ram-usage.html#optimizing-iram-usage)
- [Pinned Wi-Fi Kconfig](https://github.com/espressif/esp-idf/blob/v5.3.2/components/esp_wifi/Kconfig)
