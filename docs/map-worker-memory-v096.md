# v0.9.6 experimental: map worker memory

## Diagnosis

The v0.9.5 field capture showed the map blank while Wi-Fi and GNSS remained active. The map renderer creates a pinned FreeRTOS task with a 6144-byte stack using ordinary `xTaskCreatePinnedToCore`. ESP-IDF allocates ordinary task stacks from internal memory. The captured internal heap was about 7.4 KB free, with its largest contiguous block around 3.8 KB after BLE startup, so the map task could fail before rendering. The old UI then hid the memory error because no request remained in flight.

## ESP-IDF 5.3.2 approach

The ESP-IDF 5.3.2 FreeRTOS additions provide `xTaskCreatePinnedToCoreWithCaps`. v0.9.6 requests `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` for only the map worker stack. ESP-IDF keeps that task's TCB in internal memory. The patcher enables `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY`.

ESP-IDF documents that external RAM is unavailable while the flash cache is disabled. Accordingly, the map worker performs rendering, network and SD-cache work only. Map configuration NVS commits now run from the regular LVGL/UI task with its internal stack. The global task-stack placement policy is left untouched, so Bluetooth and other latency-sensitive tasks retain their existing internal stacks.

If task creation still fails, the map view displays a clear worker allocation message and the serial log prints internal and PSRAM free/largest blocks. Successful creation also prints both heap totals.

## Release status

This is an experimental firmware candidate. It has not been verified on the user's physical unit yet. v0.8.26 remains the recommended stable firmware until the user confirms the map works on-device.
