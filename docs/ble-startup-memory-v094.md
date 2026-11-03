# v0.9.4 — BLE startup memory fix

The v0.9.3 log showed automatic Bluetooth startup roughly 12 seconds after boot. Wi-Fi startup had fragmented internal RAM: the largest free block was 25–26 KB, while ESP32-S3 BLE controller initialization requested 30–34 KB. The controller emitted `Malloc failed` and asserted, causing a reboot loop.

The firmware now prepares the BLE controller before starting Wi-Fi, while a sufficiently large contiguous internal block is still available. If that memory check or controller preparation fails, BLE startup is disabled with a diagnostic instead of continuing into a controller assertion. NimBLE host initialization remains in the delayed worker.

This release is experimental pending validation on the physical device. v0.8.26 remains the recommended stable release.
