# MeshCrossway Android — 0.1.0 beta

> **Public beta for testing. Not version 1.0.0 and not a production release.**

[Download MeshCrossway-0.1.0.apk](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.1.0/MeshCrossway-0.1.0.apk) · [Release notes and SHA256SUMS.txt](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0) · [Android download page](https://renbrant.github.io/MeshCrossway/android/)

## Requirements

- ARM64 Android phone, Android 8.0 (API 26) or newer.
- One compatible Meshtastic BLE radio and one compatible MeshCore Companion BLE radio to use both networks. Each can also be used independently.
- Bluetooth/nearby-device permissions; location permission for GPS/map/tracks and where Android requires it for scanning.
- Internet for APK and map downloads. Native LoRa messaging works without internet.
- The Android app connects directly to the radios. A Waveshare controller is not required.

Heltec V3 and a Samsung S25 are used in development. Other phone, board and firmware combinations require validation. Close other apps using the same radio before connecting; the beta does not depend on sharing a radio connection with MeshMapper or native apps.

## Install or update

1. Download the APK from the public release link above.
2. Open it and allow APK installation from the browser/file manager when Android prompts.
3. Grant the requested Bluetooth and location permissions for the functions you use.
4. In Settings, choose each radio and let synchronization complete. Confirm both network status indicators independently.
5. If connection fails, include the Connection details output in a bug report after removing private information.

**Development signatures:** this APK is signed with a CI debug certificate. Some earlier builds have a different certificate. If Android refuses the update due to a signature conflict, export JSON history and required GPX tracks first, then uninstall and install. Uninstalling deletes local app data. The public version reset does not itself erase history.

SHA-256:
```text
7bce895a57265a941ad8c72ef1581a8d786063b5552f5a35a4a2b274eded3851  MeshCrossway-0.1.0.apk
```

## Included in this beta

Independent dual BLE connections; chats, channels and contacts; a map with right-side icons and nonmodal lower panels; optional node grouping in Layers; tracks, GPX, POIs and offline map areas; persistent message history and JSON import/export. Each network keeps its own messages, identities and routing. There is no bridging.

The app uses the phone's location. Track recording does not automatically broadcast your location to either mesh. Offline map availability depends on the successfully downloaded area, source and zoom levels; test the area without internet before a trip.

## Message hops and path limits

Received messages show available hop count and SNR/RSSI. Tapping one opens its details.

For live MeshCore channel messages, compatible firmware RX-log observations can supply one received copy's ordered repeater hashes. The app verifies/decrypts the observation and matches it to a canonical radio message; raw logs cannot create a new user message. Names are matched against saved contacts, and unknown/ambiguous hashes remain explicit. Another copy can travel a different route.

Private messages, old backlog, direct-routing and unsupported/truncated MeshCore logs may have no complete captured path. Meshtastic text can provide hop count and signal metadata but not a complete historical repeater list. A new traceroute cannot reconstruct the historical path of an earlier text. The app does not invent a path or send automatic traceroutes.

## Validation and current baseline

- Functional alpha07 behavior was accepted in maintainer device use, with both networks working.
- 0.1.0 changes version metadata/artifact naming without new features.
- [0.1.0 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42560950710): APK/lint/JVM checks and API 29/API 35 device smoke suites passed.
- Source: [ae80e8d](https://github.com/Renbrant/MeshCrossway/commit/ae80e8dbcf2909f13c721e2cb71785056df1d0ce); retained [baseline/android-0.1.0](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0).
- The APK is preserved as a public GitHub **prerelease**, separately from controller firmware releases. More device/firmware testing remains necessary.

## Version strategy

| Version | Intended scope |
|---|---|
| 0.1.0 | Accepted functional beta baseline |
| 0.1.1, 0.1.2, etc. | Fixes and small improvements |
| 0.2.0, 0.3.0, etc. | Substantial features/workflow changes; patch resets to zero |
| 1.0.0 | First production release after core scope and stability acceptance |

All 0.x versions remain in development. The earlier alpha02–alpha07 public-looking numbers were development iteration labels, not releases in the new sequence. Android versionCode remains strictly increasing (0.1.0 uses 8). Controller firmware keeps its independent numbering and recovery images.

## Help test

Check initial connection and reconnection to both radios, sending/receiving, unread state, map grouping, offline areas, tracks and backup/import on your setup. [Report a bug](https://github.com/Renbrant/MeshCrossway/issues) with app version, phone/Android version, each radio model/firmware, steps, expected result and actual result. Redact keys, personal messages and precise locations.

Wardriving/MeshMapper upload is a future possibility and is **not included** in 0.1.0.

## Development references

[Android source/build guide](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0/android) · [Development history and wire references](android-development-history.md) · [Technical hub](TECHNICAL_DOCUMENTATION.md)

The source build guide contains historical alpha references; this page and the public release define the current beta number and download.
