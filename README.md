![MeshCrossway — independent Meshtastic and MeshCore radios](docs/images/meshcrossway-modular-hero.jpg)

# MeshCrossway

**Two independent mesh networks. One interface.** Use Meshtastic and MeshCore together for messages, contacts, maps and tracks, with a separate Bluetooth radio for each network. Messages, identities and routing stay on their own network; MeshCrossway does not bridge or relay between them.

[Project website](https://renbrant.github.io/MeshCrossway/) · [Download Android beta APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.2/MeshCrossway-0.2.2.apk) · [Beta release notes](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.2) · [Documentation](docs/TECHNICAL_DOCUMENTATION.md)

> **Android 0.2.3 implementation passed its Android tests, but the APK must NOT be installed or distributed yet.** Permanent release signing is being configured to prevent data loss on future updates. The [signing and migration guide](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-permanent-signing-backup.md) explains the one-time transition and complete ZIP backups; [issue #195](https://github.com/Renbrant/MeshCrossway/issues/195) tracks private signing-key setup. **Do not uninstall your current app until you have saved your history and tracks.**

> **Windows ADB migration:** Download the [Android source ZIP](https://github.com/Renbrant/MeshCrossway/archive/refs/heads/baseline/android-0.1.0.zip), unzip it and double-click `scripts/MeshCrossway-Backup-Windows.cmd` with authorized USB debugging and Google Platform Tools/Python installed. It creates a verified ZIP backup outside the phone; **do not uninstall the current app**. The separate `scripts/MeshCrossway-Update-Windows.cmd` uses non-destructive `adb install -r` for **future updates signed with the permanent key**.

> **Android 0.2.2 is a public beta for testing, not version 1.0.0.** The functional baseline has been tested with both radios, but broader phone, radio and firmware compatibility still needs community testing. Feedback and bug reports are welcome.

## Choose how to use MeshCrossway

| Edition | Current reference | What you need | Get started |
|---|---|---|---|
| **Android app** | **0.2.2 beta** — browser Chats/Channels/Nodes and visual offline map download | ARM64 Android phone (Android 8.0+) and two compatible BLE radios | [Download APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.2/MeshCrossway-0.2.2.apk) · [Install and test](docs/android-apk.md) |
| **Touchscreen controller** | **Firmware v0.10.5** — accepted stable controller release within an experimental project | Supported Waveshare ESP32-S3 touchscreen, GNSS/storage and two compatible BLE radios | [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/) · [Firmware guide](firmware/display-prototype/README.md) |

The Android app connects directly to the radios; it does not require the Waveshare controller. The controller remains an active, separate development track. Both the controller and Android editions now have separate local browser companions; Android Remote Access must be explicitly enabled.

## Try the Android beta

1. [Download MeshCrossway-0.2.2.apk](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.2/MeshCrossway-0.2.2.apk) on your Android phone. It is a public release asset; no GitHub Actions login is required.
2. Open the APK and allow installation from your browser or file manager when Android asks.
3. Close other apps using the same radios, then select one Meshtastic radio and one MeshCore radio in MeshCrossway.
4. In Home, optionally enable Remote Access on private Wi-Fi, choose a 4–8 digit PIN (a 6-digit suggestion is provided), verify the HTTPS certificate fingerprint on the phone and sign in on the computer. Under Messages use **Chats, Channels, or Nodes**; under Offline Maps choose **Draw area** and drag a rectangle instead of entering coordinates. Test connections, messages, maps and reconnection, and [report reproducible issues](https://github.com/Renbrant/MeshCrossway/issues).

This beta uses a development signing certificate. If Android reports a signature conflict, **export JSON history and needed GPX tracks before uninstalling the previous build**; uninstalling removes local app data. Downloading basemaps/offline areas needs internet; native LoRa messages and the browser interface over local Wi-Fi do not. Remote Access is off by default and must not be exposed to the public internet.

[Installation, requirements and known limits](docs/android-apk.md) · [Checksums and release notes](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.2)

## Strategy and version numbers

Android development continues from the preserved **0.1.0** functional baseline; **0.1.1** adds the paired dual-network Home dashboard and **0.2.0** adds local HTTPS Remote Access with messages, offline map regions and track folders. **0.2.1** simplifies browser login with a user-chosen numeric PIN. **0.2.2** adds separate browser Chats/Channels/Nodes and visual rectangle selection for offline map downloads. Earlier `0.2.0-alpha02` through `0.6.0-alpha07` labels were internal development iterations; they do not represent newer public releases in this sequence. The alpha07 behavior is retained in 0.1.0.

| Version | Meaning |
|---|---|
| **0.1.0** | Preserved functional beta recovery baseline |
| **0.1.1** | Preserved paired Home-card recovery beta |
| **0.2.0** | HTTPS Remote Access, independent dual-radio browser messaging, offline regions and track folders; recovery beta |
| **0.2.1** | Numeric 4–8 digit user-chosen Remote Access PIN and cleaner login UI; recovery beta |
| **0.2.2** | Browser Chats/Channels/Nodes and visual rectangle-based offline map downloads |
| **0.2.3, 0.2.4…** | Corrections and small improvements |
| **0.3.0, 0.4.0…** | Future substantial features or workflow changes; patch numbering restarts at zero |
| **1.0.0** | First production release after core functionality and stability acceptance; not released yet |

All Android **0.x builds are development versions**. A functional recovery baseline is not a production guarantee. Android and controller firmware use **independent version sequences**; controller v0.9.32 does not mean the Android beta is nearly at 1.0. Android's internal versionCode keeps increasing even when public labels are reset.

## What works in Android 0.2.2 beta

- Independent simultaneous Meshtastic and MeshCore BLE sessions, messages, contacts and channels.
- Five main tabs: Home, Chats, Contacts, Map and Settings, with network filters.
- Map icons on the right and lower control panels for layers, tracks, POIs and offline areas.
- Persistent node grouping on/off in Layers, plus labels and visibility filters.
- Track recording/pause/resume, GPX export, saved POIs and offline map area management.
- Available received-message hops and signal details; tap a message to inspect reception metadata.
- Persistent local history and JSON backup/import.
- Optional on-phone HTTPS workspace accessible by computer on the same private Wi-Fi, showing both independent networks, **Chats**, **Channels** and **Nodes** as separate lists, and GPX tracks organized in virtual folders.
- Visual browser map to pan/zoom, draw a rectangle and download the selected area to the phone with a zoom slider and tile limit estimate; stored regions can be shown and deleted.

A complete received path is shown only when captured from a live MeshCore channel copy. Private/backlog MeshCore messages and Meshtastic text may lack the full historical route. Names matched to repeater hash prefixes can be unknown or ambiguous. A successful send accepted by the radio is not proof of end-to-end delivery.

The 0.2.2 APK passed build/lint/protocol checks, browser map geometry/navigation tests and Android API 29/35 smoke tests. The 0.2.1 short-PIN login remains unchanged. Real-device acceptance of remote access, Wi-Fi changes and two active BLE radios is still pending. The preceding functional alpha07 build was accepted in device use; other phone/radio/firmware combinations still need validation. [Validation and limitations](docs/android-apk.md). [Remote workspace design and security](docs/android-apk.md#remote-access-from-a-computer).

## Architecture and hardware

MeshCrossway began with a custom all-in-one PCB. Development with separate Bluetooth radios showed a more flexible path: use an Android phone or the dedicated touchscreen as the interface, and choose one compatible radio for each network. Separate modules also allow physical antenna separation.

Heltec V3 radios are used in development; no XIAO model is required. Each radio needs the correct native BLE interface and compatible firmware. The original integrated PCB is archived, while the modular controller/enclosure work continues.

## See it in operation

### v0.9.8 candidate running on the device

These field photos show the MeshCrossway touchscreen hardware running the v0.9.8 candidate, including its home status dashboard, map and message views.

<p align="center">
  <img src="docs/site/media/v0.9.8-field/01-assembled-device.jpg" width="49%" alt="Assembled MeshCrossway touchscreen console with external GNSS antenna">
  <img src="docs/site/media/v0.9.8-field/02-home-status.jpg" width="49%" alt="v0.9.8 home dashboard showing connected Meshtastic and MeshCore radios, GPS and Wi-Fi">
</p>
<p align="center">
  <img src="docs/site/media/v0.9.8-field/03-map-view.jpg" width="49%" alt="MeshCrossway map view with nearby network nodes">
  <img src="docs/site/media/v0.9.8-field/04-messages-view.jpg" width="49%" alt="MeshCrossway message list showing received messages and hop counts">
</p>

### Local browser companion

The controller also serves a companion interface over local Wi-Fi, so you can use messaging and track tools from a browser.

<p align="center">
  <img src="docs/site/media/v0.8.16/05-remote-tracks-map.png" width="49%" alt="Local browser track management">
  <img src="docs/site/media/v0.8.16/06-remote-messages.png" width="49%" alt="Local browser messaging">
</p>

## Hardware concept

The project includes an enclosure design and printable parts. The images below show the proposed physical form.

<p align="center">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20144917.png" width="49%" alt="MeshCrossway enclosure design, front view">
  <img src="hardware/3D/Render/Captura%20de%20tela%202026-11-01%20145119.png" width="49%" alt="MeshCrossway enclosure design, second view">
</p>

[3D models and hardware files](hardware/3D/) · [Hardware design details](docs/TECHNICAL_DOCUMENTATION.md)


## Documentation and project status

- [Android beta guide](docs/android-apk.md) — download, install, test, version policy and reception limits.
- [Technical documentation hub](docs/TECHNICAL_DOCUMENTATION.md) — controller, Android, hardware, research and historical records.
- [Project status](https://renbrant.github.io/MeshCrossway/project-status/) — separate Android and controller release records.
- [Open issues](https://github.com/Renbrant/MeshCrossway/issues) — requested work and bug reports; proposals are not released features.

The photos above retain their original controller-version labels. They do not depict the Android beta. Controller recovery images v0.9.28, v0.9.25 and v0.9.16 remain available through the Web Flasher.

## Contributing and support

Contributions to Android, firmware, hardware, documentation and testing are welcome. [Open an issue](https://github.com/Renbrant/MeshCrossway/issues), read the [contribution guide](CONTRIBUTING.md), or [support the project](https://github.com/sponsors/Renbrant).

MeshCrossway is experimental and is not certified for safety-critical use. Do not rely on it as the only communication system in an emergency. Original project code and documentation use the [MIT License](LICENSE); third-party components retain their own licenses. See [licensing](docs/licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
