# Web Flasher firmware publication policy

**Accepted by project owner on 2026-10-28.**

1. Publish a successful, compiled firmware candidate through the official [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/), even before it has passed physical testing, **clearly labeled EXPERIMENTAL / NOT HARDWARE TESTED**. Do not publish a failed CI build or an incomplete manifest.
2. The default selected build must always be the **most recent explicitly hardware-tested release**. Never automatically select `latest` if that release remains experimental or has a known issue.
3. Experimental versions must display a prominent warning and require explicit opt-in before installation. Versions with a confirmed critical boot regression must not appear in the installation picker.
4. The active Web Flasher lists **only the most recently owner-approved hardware-tested release and supported newer candidates**. Do not republish older recovery versions or known-broken builds in the Pages artifact. Retain historical source commits, version metadata and failure reports in GitHub. Every selectable binary must keep its own manifest and SHA-256 validation.
5. Once the owner confirms a candidate works on physical hardware, record the test conditions and update its catalog label/status to `hardware-tested`. The tested release then becomes the default without modifying the flasher's selection logic.
6. The Web Flasher targets only the **Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B**. The MeshCore-enabled Heltec V3 runs its **own separate Companion Bluetooth firmware**, not a MeshCrossway display image.

**Current baseline (October 28, 2026): v0.6.8 is the last owner-confirmed hardware-tested build and remains the recommended default/rollback.** `main` currently contains experimental **v0.7.6**, which is not automatically promoted to hardware-tested by CI. **v0.7.7 exists only in open PR #54** and must not be advertised as installed/published before merge, successful CI, artifact publication and physical validation. The Web Flasher reads its catalog dynamically; consult the catalog rather than hardcoding an assumed list of all available experimental versions.

**Historical exclusions:** v0.6.3 and v0.6.4 had confirmed freeze regressions and must never be offered as a safe default. Older versions below the latest approved baseline are removed from the rolling public picker; source history and incident documentation remain in GitHub. Preserve verified binary/manifests and the user's unselected **Erase** setting.

## MeshCore first-device acceptance test

- Heltec V3 uses **MeshCore Companion Bluetooth** firmware, **not** Repeater/USB-only firmware.
- Turn off other clients connected to the Heltec to leave its BLE connection available.
- Open **More > Radios** on the Waveshare; scan and select the Heltec entry.
- Verify the advertised Nordic UART service UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, pair with the PIN shown on the radio, and observe a stable authenticated connection.
- Disconnect/reconnect and confirm physical touch, Wi-Fi connection, SD backup, read-only remote capture and memory remain functional.
- **Historical note:** this test applied to the older BLE-discovery candidate. Newer v0.7.x experimental firmware includes real MeshCore Companion messaging and command handling. Follow [current implementation status](current-status.md) and [v0.7.0 protocol notes](meshcore-v070.md) for those capabilities. Neither Meshtastic application messaging nor concurrent two-radio BLE operation is implemented.

Reference: https://docs.meshcore.io/companion_protocol/
