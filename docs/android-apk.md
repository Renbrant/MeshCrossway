# MeshCrossway Android — 0.2.2 beta

> **Update safety:** Android 0.2.3 source code is merged and automated tests passed, but its permanent-key APK release is still blocked pending private signing-key setup. Do **not uninstall** your existing 0.2.2 build before exporting history and GPX or taking the optional full ADB snapshot. Once 0.2.3 is permanently signed, all future official updates with the same signing certificate will install over it and retain app-private settings, favorites, tracks and offline maps. [Migration instructions](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-permanent-signing-backup.md) · [Signing setup issue #195](https://github.com/Renbrant/MeshCrossway/issues/195).


> **Use ADB on Windows:** [Download the Android source ZIP](https://github.com/Renbrant/MeshCrossway/archive/refs/heads/baseline/android-0.1.0.zip), unzip it, connect the phone with USB debugging authorized, then run `scripts/MeshCrossway-Backup-Windows.cmd`. It saves a verified full backup ZIP under `%USERPROFILE%\\MeshCrossway-Backups`. Do not uninstall 0.2.2 until a usable backup exists. After the first permanent-key release, `scripts/MeshCrossway-Update-Windows.cmd` can install compatible signed APKs using `adb install -r` without deleting data.

> **Public beta for testing. Not version 1.0.0 and not a production release.**

[Download MeshCrossway-0.2.2.apk](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.2.2/MeshCrossway-0.2.2.apk) · [Release notes and SHA256SUMS.txt](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.2) · [Android download page](https://renbrant.github.io/MeshCrossway/android/)

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

SHA-256 (`MeshCrossway-0.2.2.apk`): `0ce51bd68d0f899753a156c2bd17aeee8f32389b2d919c4dd72833404fffd505` (verified against the exact successful CI artifact; also included in the release's `SHA256SUMS.txt`).
## Included in this beta

**New in 0.2.2:** Messages now has independent **Chats, Channels and Nodes** lists for both radios. Channels appear even without a message history, and selecting a messageable node starts a direct chat. Offline Maps has a **visual map with pan/zoom and Draw area rectangle**, maximum offline zoom, estimated tile limits and saved-region overlays. The 0.2.1 short numeric PIN remains unchanged. The 0.2.0 local HTTPS workspace, independent Meshtastic/MeshCore browser messaging, offline regions and track folders remain. The paired Home cards from 0.1.1 are retained. This does not bridge or relay mesh traffic.

**Recovery:** [Android 0.2.1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1), [Android 0.2.0](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.0), [Android 0.1.1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.1) and [0.1.0](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0) remain available.

Independent dual BLE connections; chats, channels and contacts; a map with right-side icons and nonmodal lower panels; optional node grouping in Layers; tracks, GPX, POIs and offline map areas; persistent message history and JSON import/export. Each network keeps its own messages, identities and routing. There is no bridging.

The app uses the phone's location. Track recording does not automatically broadcast your location to either mesh. Offline map availability depends on the successfully downloaded area, source and zoom levels; test the area without internet before a trip.

## Remote Access from a computer

**Beta: use only on a trusted local Wi-Fi network. Do not expose the server to the public internet.**

1. Connect the Android phone and computer to the same private Wi-Fi network. On the phone, open **Home → Remote Access**, choose a numeric PIN containing 4–8 digits (or use the suggested random 6-digit PIN), then tap **Enable**.
2. Read the HTTPS address and your short access PIN shown prominently on the phone. The long TLS SHA-256 certificate fingerprint is available separately via **View HTTPS certificate fingerprint**.
3. Open the HTTPS address in your computer browser. Because the phone uses a self-signed certificate, compare its SHA-256 fingerprint with the phone's certificate details before accepting the browser exception. Then enter the exact same numeric PIN (leading zeros included). Do not disable browser certificate verification globally.
4. Sign in to the local web workspace. Meshtastic and MeshCore remain separate. Use the browser to read/send messages, manage map regions downloaded on the phone, and organize tracks into virtual folders or export GPX.
5. Use **Change PIN** on the phone to select a new code at any time; existing browser sessions are immediately signed out. Use **Stop server** when finished. The PIN is not saved; a new activation offers a fresh six-digit suggestion.

The web server uses a local TLS identity with its private key encrypted at rest by Android Keystore AES-GCM, HTTP-only/secure session cookies, login throttling, session expiry and same-origin protections. Internet is not required for the local browser session or native LoRa communication; downloading new map tiles still needs connectivity and follows the map provider limits.

**Current limitations:** remote maps are selected by numeric geographic bounds (not by drawing a rectangle); the phone and PC must share a reachable Wi-Fi LAN; no cloud or cellular remote access; browser trust requires fingerprint verification; screen-off/Doze, Wi-Fi handovers and real simultaneous dual BLE have not yet been field-tested with this feature.

[Remote workspace technical design](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-remote-workspace.md).

## What changed in the remote browser (0.2.2)

**Messages:** choose Meshtastic or MeshCore, then the **Chats** tab to open private chats, **Channels** for configured channels and group conversations, or **Nodes** to initiate a private message even without an earlier conversation. Select an entry to view its history, send a reply and mark as read. No cross-network forwarding occurs.

**Offline maps:** click **Offline maps** → **Draw area**. Navigate around the map using drag, mouse wheel and +/- zoom; then draw the desired rectangle. Give the area a name, select the *maximum offline zoom* and click **Download area to phone**. The browser calculates the geographic coordinates automatically, and prevents requests over 2,400 tiles; the phone also enforces its 45-MB region budget. Under **Saved offline regions**, click **Show on map**, pause, resume or delete as needed.

The browser map preview fetches only live visible OpenStreetMap raster tiles; it does not download tiles from that service for offline use. The phone downloads the requested offline area through OpenFreeMap/MapLibre. Previewing the map requires internet on the computer; downloading new offline regions requires internet on the phone. [OSM raster tile policy](https://operations.osmfoundation.org/policies/tiles/) · [OpenFreeMap offline quota](https://openfreemap.org/quick_start/).

## Message hops and path limits

Received messages show available hop count and SNR/RSSI. Tapping one opens its details.

For live MeshCore channel messages, compatible firmware RX-log observations can supply one received copy's ordered repeater hashes. The app verifies/decrypts the observation and matches it to a canonical radio message; raw logs cannot create a new user message. Names are matched against saved contacts, and unknown/ambiguous hashes remain explicit. Another copy can travel a different route.

Private messages, old backlog, direct-routing and unsupported/truncated MeshCore logs may have no complete captured path. Meshtastic text can provide hop count and signal metadata but not a complete historical repeater list. A new traceroute cannot reconstruct the historical path of an earlier text. The app does not invent a path or send automatic traceroutes.

## Validation and current baseline

- Functional alpha07 behavior was accepted in maintainer device use, with both networks working.
- 0.2.0 adds the LAN HTTPS workspace, browser messaging, offline region management and virtual track folders.
- 0.2.1 adds the short numeric PIN. 0.2.2 adds browser Chats/Channels/Nodes and visual offline-area selection, retaining the same security model. 0.2.0 remains available for recovery.
- [0.1.0 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42560950710): APK/lint/JVM checks and API 29/API 35 device smoke suites passed.
- Source: [ae80e8d](https://github.com/Renbrant/MeshCrossway/commit/ae80e8dbcf2909f13c721e2cb71785056df1d0ce); retained [baseline/android-0.1.0](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0).
- The APK is preserved as a public GitHub **prerelease**, separately from controller firmware releases. More device/firmware testing remains necessary.

- [0.1.1 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42632459274): previously accepted beta checks passed.
- [0.2.0 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42648549926): original HTTPS/browser workspace validated.
- [0.2.1 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42655097137): short-PIN login validated.
- [0.2.2 CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42667980890): build/lint, browser map geometry tests and Android API 29/35 emulator tests passed. [PR #193](https://github.com/Renbrant/MeshCrossway/pull/193). See [PR #191](https://github.com/Renbrant/MeshCrossway/pull/191). Physical S25 + dual BLE + PC validation remains pending.

## Version strategy

| Version | Intended scope |
|---|---|
| 0.1.0 | Preserved functional beta recovery baseline |
| 0.1.1 | Paired dual-network dashboard cards; recovery beta |
| 0.2.0 | HTTPS Remote Access, maps and track folders; recovery beta |
| 0.2.1 | Short numeric Remote Access PIN; recovery beta |
| 0.2.2 | Separate web Chats, Channels, Nodes and visual offline map selection |
| 0.2.3, 0.2.4, etc. | Fixes and small improvements |
| 0.3.0, 0.4.0, etc. | Future substantial changes; patch resets to zero |
| 1.0.0 | First production release after core scope and stability acceptance |

All 0.x versions remain in development. The earlier alpha02–alpha07 public-looking numbers were development iteration labels, not releases in the new sequence. Android versionCode remains strictly increasing (0.1.0 uses 8, 0.1.1 uses 9, 0.2.0 uses 10, 0.2.1 uses 11, 0.2.2 uses 12). Controller firmware keeps its independent numbering and recovery images.

## Help test

Check initial connection and reconnection to both radios, sending/receiving, unread state, map grouping, offline areas, tracks and backup/import on your setup. [Report a bug](https://github.com/Renbrant/MeshCrossway/issues) with app version, phone/Android version, each radio model/firmware, steps, expected result and actual result. Redact keys, personal messages and precise locations.

Wardriving/MeshMapper upload is a future possibility and is **not included** in 0.2.2.

## Development references

[Android source/build guide](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0/android) · [Development history and wire references](android-development-history.md) · [Technical hub](TECHNICAL_DOCUMENTATION.md)

The source build guide contains historical alpha references; this page and the public release define the current beta number and download.
