# v0.7.10 — Bluetooth startup memory regression

The owner reproduced a v0.7.9-only startup failure: `NimBLE init failed: 257`, then `BLE host not ready`. Reinstalling v0.7.8 restored scanning, and reinstalling v0.7.9 reproduced it. Decimal 257 is ESP-IDF `ESP_ERR_NO_MEM` (0x101), not a PIN, bonding or RF discovery error.

The phase-1 implementation accidentally placed 33,188 bytes of initialized chat preferences plus 12,416 bytes of conversation/list/sort buffers in static internal RAM. The SD image buffers already used PSRAM, but those extra 45,604 bytes did not. This reduces the internal heap available to the Bluetooth controller/host, task stacks and DMA even when the displayed total heap still includes megabytes of free PSRAM. The error and A/B test identify an allocation failure; attributing it specifically to these buffers is the code-based diagnosis pending device retest.

The fix uses one 45,608-byte workspace explicitly allocated with `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` (including alignment). Only small pointers/control objects stay static. Allocation failure is reported without falling back into internal RAM. Initialization is idempotent and publication is atomic so a concurrent BLE/UI startup cannot replace saved preferences. Journal schema 2, stored history, Wi-Fi/BLE credentials, radio commands and pairing policy are unchanged. The SD worker also waits for the BLE controller/host startup to finish before claiming its internal task stack. The BLE error now includes its ESP-IDF name as well as its number; pressing scan after a failed initialization preserves that original cause instead of replacing it with a generic host-not-ready message.

Host fixtures verify the exact capability request, allocation failure, repeated initialization and preference preservation. A new ELF symbol audit rejects a large static object in the chat/history namespaces and enforces a 2 KiB aggregate budget, including the real ESP32 ELF after compilation. Existing protocol, journal and actual LVGL tests continue to run. CI cache inputs include the audit and fixtures.

v0.7.9 is withdrawn from the installable catalog. v0.7.10 remains experimental pending a real device test; v0.7.8 remains the recommended recovery with its original published bytes.

## Device retest

1. Install v0.7.10 with Erase unchecked; power-cycle the Core.
2. Enable BLE / scan with Wi-Fi on. Check that the known MeshCore radio appears and no 257 error is shown.
3. Connect using the existing bond, then receive/send a test message and open Nodes/Groups/Chats.
4. Wait for the SD saved indicator and reboot; confirm history restoration and BLE reconnection together.
5. If startup fails, retain `[MCX-MEM]` and `[MCX-BLE]` serial lines showing free internal RAM, largest internal/DMA block and the named error. Do not erase NVS to diagnose this allocation failure.

References: [ESP-IDF 5.3.2 error codes](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/error-codes.html), [external RAM allocation and restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/external-ram.html).
