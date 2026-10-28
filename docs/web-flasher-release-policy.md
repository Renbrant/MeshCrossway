# Web Flasher firmware publication policy

**Accepted by project owner on 2026-10-28.**

1. Publish a successful, compiled firmware candidate through the official [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/), even before it has passed physical testing, **clearly labeled EXPERIMENTAL / NOT HARDWARE TESTED**. Do not publish a failed CI build or an incomplete manifest.
2. The default selected build must always be the **most recent explicitly hardware-tested release**. Never automatically select `latest` if that release remains experimental or has a known issue.
3. Experimental versions must display a prominent warning and require explicit opt-in before installation. Versions with a confirmed critical boot regression must not appear in the installation picker.
4. The active Web Flasher lists **only the most recently owner-approved hardware-tested release and supported newer candidates**. Do not republish older recovery versions or known-broken builds in the Pages artifact. Retain historical source commits, version metadata and failure reports in GitHub. Every selectable binary must keep its own manifest and SHA-256 validation.
5. Once the owner confirms a candidate works on physical hardware, record the test conditions and update its catalog label/status to `hardware-tested`. The tested release then becomes the default without modifying the flasher's selection logic.
6. The Web Flasher targets only the **Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B**. The MeshCore-enabled Heltec V3 runs its **own separate Companion Bluetooth firmware**, not a MeshCrossway display image.

**Flasher archive policy narrowed on 2026-10-28:** publish the last stable and later supported builds only. For now show v0.6.2 and v0.6.4, not the known-broken v0.6.3. When a newer release passes owner hardware testing, advance the stable pin and prune older images.

After the owner tested v0.6.3 twice, the latest confirmed hardware-tested build is **v0.6.2** (Wi-Fi, SD backup and manual remote screenshots). v0.6.3 is confirmed broken at boot and its Web Flasher installation is blocked. v0.6.4 is the new experimental safe-boot candidate (BLE initially disabled). Older v0.3.x, v0.4.x and v0.5.x builds are no longer offered through the active Web Flasher; their source history remains in GitHub.

## MeshCore first-device acceptance test

- Heltec V3 uses **MeshCore Companion Bluetooth** firmware, **not** Repeater/USB-only firmware.
- Turn off other clients connected to the Heltec to leave its BLE connection available.
- Open **More > Radios** on the Waveshare; scan and select the Heltec entry.
- Verify the advertised Nordic UART service UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, pair with the PIN shown on the radio, and observe a stable authenticated connection.
- Disconnect/reconnect and confirm physical touch, Wi-Fi connection, SD backup, read-only remote capture and memory remain functional.
- This candidate does **not** send MeshCore messages or commands. Full characteristic discovery, subscription, transport frames and protocol synchronization are subsequent work, not completed acceptance criteria.

Reference: https://docs.meshcore.io/companion_protocol/
