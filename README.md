<p align="center"><img src="docs/site/assets/meshcrossway-logo.svg" alt="MeshCrossway" width="420"></p>

# MeshCrossway for Android

**Two independent mesh networks. One Android app.** Use Meshtastic and MeshCore side by side, connecting to a separate Bluetooth radio for each network. Messages, identities and routing stay on their native networks; MeshCrossway does not bridge or relay between them.

<p align="center"><a href="https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.4/MeshCrossway-0.2.4.apk"><strong>Download MeshCrossway Android 0.2.4 beta APK</strong></a> · <a href="https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.4">Release notes and checksums</a> · <a href="https://meshcrossway.renbrant.com/android/">Installation guide</a></p>

> **Public beta for testing — not version 1.0.0.** Requires Android 8.0+ on ARM64. Broader phone, radio and firmware compatibility is still being evaluated.

## Install and update

1. Download the [public APK](`https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.4/MeshCrossway-0.2.4.apk`) on your Android phone. No GitHub Actions login, ADB or ZIP file is needed.
2. Open it and allow your browser or file manager to install apps when Android asks.
3. In MeshCrossway, connect a compatible Meshtastic radio and a compatible MeshCore radio over Bluetooth.

**Updating from 0.2.3:** Install 0.2.4 over the existing app. It uses the same permanent release signing certificate, so local messages, settings, favorites, tracks and offline maps remain in place. Do not uninstall to update.

Builds **0.2.2 and earlier** used temporary debug signing. If you still have local data on one of those builds, follow the [one-time signing migration and backup guide](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-permanent-signing-backup.md) before changing versions. Uninstalling deletes app data.

## What the beta includes

- Simultaneous, independent Meshtastic and MeshCore Bluetooth sessions.
- Chats, channels, contacts and nodes for both networks.
- Maps, node visibility controls, track recording and GPX export.
- Saved points of interest and offline map area downloads.
- Local history, backup and restore tools, and customizable app appearance.
- Optional HTTPS browser workspace on a private local Wi-Fi network. Remote Access is off by default.

The Android app stores its current data locally on the phone; cloud account synchronization is not part of this release. Basemap downloads need internet access, while native LoRa messages and previously downloaded maps can be used without cellular coverage. Do not expose Remote Access to the public internet.

## Version policy

Android uses its own version sequence, independent of the embedded controller firmware. The Android 0.x versions are development releases; **1.0.0 has not been released**.

| Version | Purpose |
|---|---|
| **0.1.0–0.1.1** | Preserved functional and recovery beta baselines |
| **0.2.0–0.2.2** | Remote browser workspace, messaging views and visual offline-map selection |
| **0.2.3** | First permanent release signing and complete-data backup/restore |
| **0.2.4** | Theme and appearance choices; current public beta |
| **0.2.5, 0.2.6…** | Corrections and smaller improvements |
| **0.3.0, 0.4.0…** | Larger feature or workflow changes |
| **1.0.0** | First production release after core functionality and stability acceptance |

A functional recovery baseline is not a production guarantee. Send reproducible problems through [GitHub Issues](https://github.com/Renbrant/MeshCrossway/issues), including your phone, radio models and firmware versions.

## The ESP32 controller project

The separate ESP32 touchscreen controller is a **secondary development track**; its hardware, firmware, status, roadmap and Web Flasher are collected in the [ESP32 project area](https://meshcrossway.renbrant.com/esp32/).

## Contributing and license

Contributions to the Android app, documentation and testing are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) and open an [issue](https://github.com/Renbrant/MeshCrossway/issues). MeshCrossway is experimental and is not certified for safety-critical use. Original project code and documentation use the [MIT License](LICENSE); see [licensing](docs/licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
