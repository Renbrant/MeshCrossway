# Android controller — APK development

Tracking: [#187](https://github.com/Renbrant/MeshCrossway/issues/187).

## Strategic direction

Android becomes the priority for a broadly accessible MeshCrossway controller. The existing dedicated ESP32-S3 controller remains available. Both use the same modular architecture: one user interface, two independent radios, no forwarding or bridge between Meshtastic and MeshCore.

A Samsung Galaxy S25 maintained connections from MeshMapper and Meshtastic to two separate Heltec V3 radios. This observation supports the decision; it does **not** establish that this APK has passed simultaneous-radio, screen-off, or multi-vendor tests.

## First deliverable: 0.1.0-alpha01

This is an installable **connection and protocol probe**, not feature parity with the display firmware.

Implemented:

- Native Android application (Android 8/API 26 minimum; compile/target API 35).
- Current firmware palette: background `#090C10`, panels `#181D24` / `#242B34`, UI accent `#FF8A24`, text `#F2F4F7`, muted `#9AA6B4`.
- Official horizontal logo rasterized uniformly from the unchanged SVG; logo colors remain `#F07120` / `#F2F2F2`. The generic radio launcher icon is not a replacement brand mark.
- Portrait bottom navigation: Home, Nodes, Groups, Chats, Map, Settings. Map context rail on the **left**. System insets and scrollable content.
- Runtime Nearby devices permissions; Android 8–11 location permission for scanning. Notification permission on Android 13+ is requested; denial does not block Bluetooth.
- User-initiated BLE discovery, separate Meshtastic / MeshCore radio selection, independent GATT connections, explicit reconnect and disconnect.
- One serialized operation queue, timeout, subscription and protocol probe per radio. Late callbacks from closed sessions are ignored. One radio cannot occupy both active slots.
- A `connectedDevice` foreground service owns the sessions independently of Activity navigation. A notification provides a disconnect-all action. Bluetooth-off closes sessions; reconnect is explicit in this alpha. No boot auto-start or automatic process-death recovery.
- Meshtastic: expected service/characteristics, MTU negotiation, FromNum subscription, nonce-correlated PhoneAPI configuration request, FromRadio draining and bounded protobuf parsing. Reports verification only after the matching config-complete response plus my-info, metadata and LoRa config.
- MeshCore: expected Nordic UART service/characteristics, subscription, device query and app-start identity probe; Companion protocol v3+ required. No mesh packet transmission.
- Successfully verified radio addresses persist in private app preferences. No message bodies, keys, addresses or location are logged. Incoming frames are counted, not retained as message history.

Still pending (screens explicitly identify these as future work):

- Text send/receive UI, channel/contact synchronization and persistent message history.
- Nodes, groups/channels, unread counts, delivery acknowledgements and filters.
- Phone location, actual map rendering, pinch zoom, tracks, POIs and offline-area downloads/deletion.
- Per-radio automatic reconnection, richer pairing recovery, robust background and battery validation.
- Localization/accessibility refinement and production release signing/update distribution.

The compatibility probe reads radio configuration/identity only. It does not change region, frequency, channels, clocks, keys or transmit messages over LoRa. After verification the Meshtastic probe stops draining FromRadio. FromRadio is a consuming read; packets interleaved with the configuration stream are not retained by this alpha. Do not use alpha01 as a message-history recorder.

## Architecture and implementation references

Source: `android/`. Native Java uses Android platform UI/Bluetooth APIs without third-party runtime dependencies. Java was selected for this small transport prototype; the service/UI boundary permits a later UI migration without changing the radio architecture.

`RadioService` owns independent sessions and queues; `RadioProfile` contains the existing BLE UUIDs; `ProtocolProbe` implements bounded read-only wire probes; `MainActivity` renders state without holding a GATT connection. No shared transmit path connects one network to the other.

Behavior was referenced against these files at main commit `3aaaf32f371116bb06559a3958b69631370057b4` (firmware v0.10.5):

- `firmware/display-prototype/meshcrossway_ui.h` (palette / navigation)
- `firmware/display-prototype/meshcrossway_ble_link.inc` (GATT UUIDs / subscription / draining)
- `firmware/display-prototype/meshcrossway_meshtastic_protocol.h` (PhoneAPI wire format)
- `firmware/display-prototype/meshcrossway_meshcore_protocol.h` (Companion wire format)
- `docs/branding.md` and `docs/site/assets/meshcrossway-logo.svg`

Android guidance: [BLE background communication](https://developer.android.com/develop/connectivity/bluetooth/ble/background), [foreground service types](https://developer.android.com/develop/background-work/services/fgs/service-types), [Bluetooth permissions](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions).

## Build and install

Requirements: JDK 17, Android SDK platform 35, build tools 35.0.0. Open `android/` in Android Studio, or set `ANDROID_HOME` and run:

```sh
bash scripts/test_android_probe.sh
cd android
./gradlew assembleDebug lintDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`. The GitHub **Android alpha APK** workflow builds the APK and uploads an artifact containing the named APK and SHA-256 checksum. Install the APK extracted from that artifact, not the artifact ZIP.

The APK is debug-signed and named **MeshCrossway Alpha**. Different build machines can use different debug certificates: uninstalling the earlier APK may be needed before installing a different build, which deletes app data. Production updates require a stable, securely managed release signing key. Do not commit keystores or passwords.

On the S25:

1. Close MeshMapper, Meshtastic and other apps connected to these radios; keep both Heltec radios awake.
2. Install the alpha APK and allow installation from the browser/file manager if Android asks.
3. Open Settings → choose Meshtastic → scan → select the first radio. Grant Nearby devices and respond to system pairing/PIN prompts. Android 8–11 also needs phone Location enabled for discovery.
4. Choose MeshCore → scan → select the other radio with **Companion BLE firmware** (not repeater firmware).
5. Return Home. Record each status, protocol verification, frame count, Android version and radio firmware version. Disconnecting one should leave the other session active.
6. Test navigating all tabs, rotation, screen off, Wi-Fi, car audio and toggling Bluetooth. Return to the app after screen-off and inspect both connections. Reconnect explicitly after radio loss/Bluetooth-off.

## Acceptance and next increments

Local validation: `assembleDebug` and `lintDebug` passed (no lint issues); the dependency-free protocol wire fixtures passed; `apksigner verify --verbose` confirmed the APK signature, and `aapt dump badging` confirmed package/version/minimum API. Build/lint/protocol-fixture checks are software validation. No physical-radio or Android UI/emulator validation should be inferred from a successful build.

Before calling dual connectivity validated: test one S25 app with both Heltec V3 radios, independent disconnect/reconnect, pairing/PIN, 30+ minutes screen off, Wi-Fi plus car audio, and process recreation. Repeat on another vendor before making broad Android compatibility claims.

Next increment: implement separate full protocol clients and private transactional message storage keyed by network + radio + conversation. Then validate real send/receive on both networks, followed by nodes/channels and location/maps/offline management. Map data sources must support the intended offline use; do not bulk-download standard OSM tile servers.
