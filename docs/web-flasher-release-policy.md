# Web Flasher firmware publication policy

**Accepted by project owner on 2026-10-28.**

1. Publish a successful, compiled firmware candidate through the official [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/), even before it has passed physical testing, **clearly labeled EXPERIMENTAL / NOT HARDWARE TESTED**. Do not publish a failed CI build or an incomplete manifest.
2. The default selected build must always be the **most recent explicitly hardware-tested release**. Never automatically select `latest` if that release remains experimental or has a known issue.
3. Experimental and known-issue versions must display a prominent warning and require an explicit opt-in checkbox before the Install button is enabled.
4. Retain older, pinned compiled versions and recovery images, with intact versions, manifests and SHA-256 verification. No update may silently replace a previously released binary.
5. Once the owner confirms a candidate works on physical hardware, record the test conditions and update its catalog label/status to `hardware-tested`. The tested release then becomes the default without modifying the flasher's selection logic.
6. The Web Flasher targets only the **Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B**. The MeshCore-enabled Heltec V3 runs its **own separate Companion Bluetooth firmware**, not a MeshCrossway display image.

After the owner tested v0.6.3 twice, the latest confirmed hardware-tested build is **v0.6.2** (Wi-Fi, SD backup and manual remote screenshots). v0.6.3 is confirmed broken at boot and its Web Flasher installation is blocked. v0.6.4 is the new experimental safe-boot candidate (BLE initially disabled). The pre-v0.5.0 v0.4.0 and v0.3.0 images remain emergency recovery options.

## MeshCore first-device acceptance test

- Heltec V3 uses **MeshCore Companion Bluetooth** firmware, **not** Repeater/USB-only firmware.
- Turn off other clients connected to the Heltec to leave its BLE connection available.
- Open **More > Radios** on the Waveshare; scan and select the Heltec entry.
- Verify the advertised Nordic UART service UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, pair with the PIN shown on the radio, and observe a stable authenticated connection.
- Disconnect/reconnect and confirm physical touch, Wi-Fi connection, SD backup, read-only remote capture and memory remain functional.
- This candidate does **not** send MeshCore messages or commands. Full characteristic discovery, subscription, transport frames and protocol synchronization are subsequent work, not completed acceptance criteria.

Reference: https://docs.meshcore.io/companion_protocol/
