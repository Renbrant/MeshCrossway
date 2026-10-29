# BLE / LCD investigation

## v0.6.6 — reserved LCD DMA buffers (EXPERIMENTAL — NOT HARDWARE TESTED)

This candidate replaces v0.6.5 before publication. The earlier candidate compiled
successfully in Actions run 40789586924, but its build log reports an unknown
`ESP_WIFI_TASK_CORE_ID` Kconfig assignment. The corrected selection is
`CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y`. A post-build check now reads the
**generated sdkconfig** and rejects missing BLE/coexistence/core selections.

Verified source findings:

- Pinned Waveshare `main.cpp` used two 19,200-pixel PSRAM draw buffers with
  `buff_dma=0`, `buff_spiram=1` and `trans_size=0`.
- ESP-IDF 5.3.2 SPI `setup_priv_desc()` allocates temporary DMA storage when the
  source is not DMA-capable. Allocation failure returns `ESP_ERR_NO_MEM` through
  `spi_device_queue_trans()` to the observed LCD error at line 390.
- Pinned Waveshare ST7796 `draw_bitmap()` discarded `esp_lcd_panel_io_tx_color()`'s
  return value and reported success even on failure. Without a final color
  completion, LVGL waits for flush completion; this explains a possible path
  from a failed transfer to the watchdog. **The owner's trace lacks the return
  code and a full backtrace: DMA exhaustion remains a hypothesis.**

The candidate reserves two 4,800-pixel DMA draw buffers at boot (9,600 bytes each)
and matches SPI max-transfer size in **bytes**. PSRAM remains enabled for the UI,
manual capture and other allocations. Rotation, touch and interface layout remain
unchanged. `trans_size=0` keeps one completion per flush, avoiding reliance on
upstream chunked PSRAM handling discussed in Espressif BSP issue 716.

On a color transfer error, print the exact error, transfer length, DMA free/largest
block and whether the pixel source is DMA-capable, then abort/reset instead of
waiting forever. This is an explicit diagnostic fallback, **not successful BLE
operation**; reboot leaves BLE off so recovery remains possible. Do not signal
fake completion or reuse a buffer that may still have queued DMA work.

The v0.6.5 clock (40 MHz), queue depth (6) and task affinity experiment is retained.
Additional memory logs include largest DMA block and task stack high-water mark,
after controller initialization and on host sync. ELF, map, dependencies.lock and
generated sdkconfig are retained in the `ble-display-debug-symbols` CI artifact.
No complete watchdog backtrace was supplied, so no address decoding is claimed.

The rollback v0.6.2 is copied from the existing approved publication and its full
SHA-256 is pinned; it must not be silently rebuilt with different dependencies.

### Owner hardware test

1. Open the official Web Flasher, select **v0.6.6** deliberately and accept the
   experimental warning. Keep erase disabled. Do not flash the Heltec.
2. Open **Logs**, reset the Waveshare, and confirm `DEV 0.6.6` and
   `[MCX-LCD] reserved DMA draw buffers: 2 x 9600 bytes; trans_size=0`.
3. For 30 seconds, navigate and confirm Wi-Fi, keyboard and one manual screenshot.
4. Go to **More > Radios > ENABLE BLE / TEST**. Keep Logs open and navigate for
   another 30 seconds. Look for `nimble_port_init result=0`, `[MCX-MEM] after nimble
   init` and `[MCX-MEM] host synced`. Neither a watchdog nor a reset is a pass.
5. If touch and Wi-Fi remain responsive, scan, select the Heltec MeshCore
   Companion, and enter the configured six-digit PIN when requested. Test manual
   disconnect and reconnect, then a power cycle of the Heltec. Report each stage
   separately; protocol messages remain unimplemented and are not transmitted.
6. Copy the log starting before activation, including any `[MCX-LCD] tx_color
   failed` or panic/backtrace. If it fails, reinstall **v0.6.2** without erase.

Primary references:
- https://github.com/espressif/esp-idf/blob/v5.3.2/components/esp_driver_spi/src/gpspi/spi_master.c
- https://github.com/espressif/esp-idf/blob/v5.3.2/components/esp_lcd/spi/esp_lcd_panel_io_spi.c
- https://github.com/espressif/esp-idf/blob/v5.3.2/components/esp_wifi/Kconfig
- https://components.espressif.com/components/espressif/esp_lvgl_port/versions/2.9.0/readme
- https://github.com/espressif/esp-bsp/issues/716
- https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.5/ESP-IDF

Current Waveshare documentation specifies IDF 5.5.0+; the pinned example declares
IDF >=5.1 and has compiled on 5.3.2. This candidate deliberately retains 5.3.2 to
isolate the memory-path change. A toolchain/BSP migration requires separate tests.
