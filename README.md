<p align="center"><img src="docs/site/assets/meshcrossway-logo.svg" alt="MeshCrossway" width="420"></p>

# MeshCrossway for Android

**Two independent mesh networks. One Android app.** Use Meshtastic and MeshCore side by side, connecting to a separate Bluetooth radio for each network. Messages, identities and routing stay on their native networks; MeshCrossway does not bridge or relay between them.

<p align="center"><a href="https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.4.1/MeshCrossway-0.4.1.apk"><strong>Download MeshCrossway Android 0.4.1 field beta APK</strong></a> · <a href="https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.1">Release notes and checksums</a> · <a href="https://meshcrossway.renbrant.com/android/">Installation guide</a></p>

> **Public beta for testing — not version 1.0.0.** Requires Android 8.0+ on ARM64. Broader phone, radio and firmware compatibility is still being evaluated.

## Install and update

1. Download the [public APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.4.1/MeshCrossway-0.4.1.apk) on your Android phone. No GitHub Actions login, ADB or ZIP file is needed.
2. Open it and allow your browser or file manager to install apps when Android asks.
3. In MeshCrossway, connect one compatible Meshtastic radio and one compatible MeshCore radio over Bluetooth.

**Updating an existing permanent-key installation (0.2.3 or newer):** Back up important local data, install 0.4.1 over the existing app and choose Update. It uses the same permanent release signing certificate, so local messages, settings, favorites, tracks and offline maps remain in place. Do not uninstall to update.

**0.4.1 is an experimental field beta:** remote MeshCore status/neighbor queries require authorized real-repeater testing. Requests are manual, may cause flood relay traffic and share a five-minute per-repeater limit; no automatic queries, retries or cross-network bridging. The signed [0.4.0 beta remains available](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.0-stage1), but Android normally does not allow a direct downgrade from versionCode 35 to 34. See [0.4.1 release notes](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.1).

Builds **0.2.2 and earlier** used temporary debug signing. If you still have local data on one of those builds, follow the [one-time signing migration and backup guide](https://github.com/Renbrant/MeshCrossway/blob/main/docs/android-permanent-signing-backup.md) before changing versions. Uninstalling deletes app data.

## What the beta includes

- Simultaneous, independent Meshtastic and MeshCore Bluetooth sessions.
- Chats, channels, contacts and nodes for both networks.
- Maps, node visibility controls, track recording and GPX export.
- Saved points of interest and offline map area downloads.
- Local history, backup and restore tools, and customizable app appearance.
- Compact Home with separate radio cards and measured Bluetooth RSSI for each phone-to-radio connection.
- Optional HTTPS browser workspace on a private local Wi-Fi network. Remote Access is off by default.

The Home signal bars and dBm value describe the **Bluetooth link between the phone and a radio**, not LoRa reception. Open a node's **Reception history** to see actual reported LoRa RSSI/SNR from saved messages when available; outgoing status now separates radio acceptance from peer acknowledgement and allows only manually confirmed retry drafts. Messages and tracks are stored locally. Optional account access is available from the toolbar. Basemap downloads need internet access, while native LoRa messages and previously downloaded maps can be used without cellular coverage. Do not expose Remote Access to the public internet.

## Version policy

Android uses its own version sequence, independent of the embedded controller firmware. Android 0.x versions are development releases; **1.0.0 has not been released**.

| Version | Purpose |
|---|---|
| **0.1.0–0.1.1** | Initial functional beta baselines |
| **0.2.0–0.2.2** | Local browser workspace, messaging views and offline-map selection |
| **0.2.3–0.2.5** | Permanent signing, backup/restore, appearance and compact Home |
| **0.2.6** | Message actions and official icon |
| **0.2.7–0.2.9** | Account sign-in, cloud conversation sync and GPX track sync |
| **0.3.0** | MeshCore device settings |
| **0.3.1–0.3.5** | Meshtastic settings, protocol corrections and save verification |
| **0.3.6–0.3.7** | Compact Home, 15-minute recent nodes and toolbar account layout |
| **0.3.8** | Meshtastic settings corrections |
| **0.3.9** | MeshCore settings and messaging corrections |
| **0.3.10** | MeshCore contact membership and reconnect verification fixes |
| **0.3.11** | MeshCore announcements, channel management, contact filters and diagnostics; corrects the mistaken 0.4.0 number |
| **0.3.12** | Channel management for both networks, official links and QR camera/image import; previous beta |
| **0.4.0** | Better Bluetooth signal freshness, LoRa reception charts and safer manual recovery; previous tested beta |
| **0.4.1** | Experimental field beta: passive dual-network diagnostics, MeshCore repeater status and paginated neighbors by manual request with a five-minute per-repeater airtime guard, and recent Home outbox summary; current beta pending physical repeater acceptance |
| **1.0.0** | First production release after an explicit acceptance decision |

See [channel management and QR Codes](docs/android-channel-management.md).

The published 0.4.1 beta uses versionCode **35** and the existing permanent Android signing certificate. The old `android-v0.4.0` tag is a **historical mistakenly numbered beta** (versionCode 31), superseded by 0.3.11. The new, significant-update **Android 0.4.0** is identified by tag `android-v0.4.0-stage1` and versionCode **34**. For the previous tested beta, use the Stage 1 release, never the old tag; see the [numbering analysis and release history](docs/android-version-history.md).

A functional recovery baseline is not a production guarantee. Send reproducible problems through [GitHub Issues](https://github.com/Renbrant/MeshCrossway/issues), including your phone, radio models and firmware versions.

The ESP32 touchscreen device remains a secondary development project; its hardware, firmware, status, roadmap and Web Flasher details are collected in the [controller project area](https://meshcrossway.renbrant.com/esp32/).

## Contributing and license

Contributions to the Android app, documentation and testing are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) and open an [issue](https://github.com/Renbrant/MeshCrossway/issues). MeshCrossway is experimental and is not certified for safety-critical use. Original project code and documentation use the [MIT License](LICENSE); see [licensing](docs/licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

## Development branch policy

The website and public documentation are maintained on `main`. Recent signed Android APKs are built from `baseline/android-0.1.0`; despite its historical name, that branch is currently active for Android releases. The immutable release tag identifies the exact source shipped in each APK. The Android source on `main` is behind the release branch and must not be assumed to match the latest APK. See [release history and numbering](docs/android-version-history.md).
