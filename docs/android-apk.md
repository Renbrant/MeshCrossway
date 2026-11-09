# MeshCrossway Android — 0.2.0 beta

> **Public beta for testing. Not version 1.0.0 and not a production release.**

[Download MeshCrossway-0.2.0.apk](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.0/MeshCrossway-0.2.0.apk) · [Release notes and SHA256SUMS.txt](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.0) · [Android download page](https://renbrant.github.io/MeshCrossway/android/)

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

SHA-256: download `SHA256SUMS.txt` from the 0.2.0 release page and verify the exact APK before installation.
## Included in this beta

**New in 0.2.0:** secure opt-in local HTTPS remote workspace, independent browser messaging for Meshtastic and MeshCore, offline region management and track folders. The paired Home cards from 0.1.1 are retained. This does not bridge or relay mesh traffic.

**Recovery:** [Android 0.1.1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.1) and [0.1.0](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0) remain available.

Independent dual BLE connections; chats, channels and contacts; a map with right-side icons and nonmodal lower panels; optional node grouping in Layers; tracks, GPX, POIs and offline map areas; persistent message history and JSON import/export. Each network keeps its own messages, identities and routing. There is no bridging.

The app uses the phone's location. Track recording does not automatically broadcast your location to either mesh. Offline map availability depends on the successfully downloaded area, source and zoom levels; test the area without internet before a trip.

## Remote Access from a computer

**Beta: use only on a trusted local Wi-Fi network. Do not expose the server to the public internet.**

1. Connect the Android phone and the computer to the same private Wi-Fi network. On the phone, open Home → **Remote Access** and enable it.
2. Read the HTTPS address, activation session code and TLS SHA-256 certificate fingerprint displayed on the phone.
3. Open the shown HTTPS address in your computer browser. Because the phone uses a self-signed certificate, first compare its SHA-256 fingerprint with the fingerprint on the phone before accepting the browser exception or entering the session code. Do not disable browser certificate verification globally.
4. Sign in to the local web workspace. Meshtastic and MeshCore remain separate. Use the browser to read/send messages, manage map regions downloaded on the phone, and organize tracks into virtual folders or export GPX.
5. Return to Home → Remote Access → **Stop server** when finished. Stopping invalidates web sessions and rotates the session code on next activation.

The web server uses a local TLS identity with its private key encrypted at rest by Android Keystore AES-GCM, HTTP-only/secure session cookies, login throttling, session expiry and same-origin protections. Internet is not required for the local browser session or native LoRa communication; downloading new map tiles still needs connectivity and follows the map provider limits.

**Current limitations:** remote maps are selected by numeric geographic bounds (not by drawing a rectangle); the phone and PC must share a reachable Wi-Fi LAN; no cloud or cellular remote access; browser trust requires fingerprint verification; screen-off/Doze, Wi-Fi handovers and real simultaneous dual BLE have not yet been field-tested with this feature.

[Remote workspace technical design](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-remote-workspace.md).

## Message hops and path limits

Received messages show available hop count and SNR/RSSI. Tapping one opens its details.

For live MeshCore channel messages, compatible firmware RX-log observations can supply one received copy's ordered repeater hashes. The app verifies/decrypts the observation and matches it to a canonical radio message; raw logs cannot create a new user message. Names are matched against saved contacts, and unknown/ambiguous hashes remain explicit. Another copy can travel a different route.

Private messages, old backlog, direct-routing and unsupported/truncated MeshCore logs may have no complete captured path. Meshtastic text can provide hop count and signal metadata but not a complete historical repeater list. A new traceroute cannot reconstruct the historical path of an earlier text. The app does not invent a path or send automatic traceroutes.

## Validation and current baseline

- Functional alpha07 behavior was accepted in maintainer device use, with both networks working.
- 0.2.0 adds the LAN HTTPS remote workspace, browser messaging, offline region management and virtual track folders. 0.1.1 remains a recovery beta.
- [0.1.0 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42560950710): APK/lint/JVM checks and API 29/API 35 device smoke suites passed.
- Source: [ae80e8d](https://github.com/Renbrant/MeshCrossway/commit/ae80e8dbcf2909f13c721e2cb71785056df1d0ce); retained [baseline/android-0.1.0](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0).
- The APK is preserved as a public GitHub **prerelease**, separately from controller firmware releases. More device/firmware testing remains necessary.

- [0.1.1 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42632459274): previously accepted beta checks passed.
- [0.2.0 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42648549926): APK build/lint, protocol checks and Android API 29/35 emulator tests passed, including HTTPS handshake, login, session/CSRF checks and both network statuses. See [PR #191](https://github.com/Renbrant/MeshCrossway/pull/191). Physical S25 + dual BLE + PC validation remains pending.

## Version strategy

| Version | Intended scope |
|---|---|
| 0.1.0 | Preserved functional beta recovery baseline |
| 0.1.1 | Paired dual-network dashboard cards; recovery beta |
| 0.2.0 | Local HTTPS Remote Access, maps and track folders |
| 0.2.1, 0.2.2, etc. | Fixes and small improvements |
| 0.3.0, 0.4.0, etc. | Future substantial changes; patch resets to zero |
| 1.0.0 | First production release after core scope and stability acceptance |

All 0.x versions remain in development. The earlier alpha02–alpha07 public-looking numbers were development iteration labels, not releases in the new sequence. Android versionCode remains strictly increasing (0.1.0 uses 8, 0.1.1 uses 9, 0.2.0 uses 10). Controller firmware keeps its independent numbering and recovery images.

## Help test

Check initial connection and reconnection to both radios, sending/receiving, unread state, map grouping, offline areas, tracks and backup/import on your setup. [Report a bug](https://github.com/Renbrant/MeshCrossway/issues) with app version, phone/Android version, each radio model/firmware, steps, expected result and actual result. Redact keys, personal messages and precise locations.

Wardriving/MeshMapper upload is a future possibility and is **not included** in 0.2.0.

## Development references

[Android source/build guide](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0/android) · [Development history and wire references](android-development-history.md) · [Technical hub](TECHNICAL_DOCUMENTATION.md)

The source build guide contains historical alpha references; this page and the public release define the current beta number and download.
