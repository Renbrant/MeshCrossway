# MeshCrossway Android — 0.4.0 beta

> **Version 0.4.0 is published with the same permanent release signing certificate as 0.2.3/0.2.4.** Starting from a clean installation? Install the APK directly; **no ADB, ZIP or migration is necessary.** Future official APKs with the same certificate can install over this version, retaining locally stored messages, settings, favorites, GPS tracks, and offline maps. **Do not uninstall or clear app data to update.**

> Only people who still have data on older debug-signed 0.2.2-or-earlier builds need an optional one-time migration. See the [migration guide](https://github.com/Renbrant/MeshCrossway/blob/baseline/android-0.1.0/docs/android-permanent-signing-backup.md). This release is still a beta.

> **Public beta for testing. Not version 1.0.0 and not a production release.**

[Download MeshCrossway-0.4.0.apk](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.4.0/MeshCrossway-0.4.0.apk) · [Release notes and SHA256SUMS.txt](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.0) · [Android download page](https://renbrant.github.io/MeshCrossway/android/)

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

**Permanent signing started with 0.2.3 and is retained in 0.4.0.** Its signature SHA-256 is pinned and checked in the GitHub release workflow. All future releases must reuse that certificate. Old debug-signed builds cannot be replaced in place with 0.2.3; if you still have data in an old build, back it up before the one-time migration. Never uninstall to force a future update.

SHA-256: verify the downloaded `MeshCrossway-0.4.0.apk` against the release's `SHA256SUMS.txt` asset. The `SIGNING-CERT-SHA256.txt` asset records the permanent APK signing certificate fingerprint.
## MeshCore everyday tools in 0.4.0

Announce your presence nearby or across the mesh; create private, hashtag or Public channels; edit/remove channels with verified readback; share and import official MeshCore channel links; filter discovered, approved and historical contacts; and save/share a bounded support diagnostic that excludes private content. See the [user guide and validation details](android-meshcore-everyday-tools.md). The package includes 96 MeshCore regression checks, 106 new feature checks and the existing 206 Meshtastic conformance checks. Physical-radio acceptance remains necessary.

## MeshCore contact and reconnect corrections in 0.3.10

Resolves the three residual cases from the 0.3.9 re-audit: actual radio membership is checked after new adverts, a contact can recover after NOT_FOUND without reconnecting, and accepted settings writes retain GET-only verification even when Bluetooth disconnects before its write callback. The suite now has 96 MeshCore checks plus 206 Meshtastic conformance checks. See the [correction and validation matrix](audits/MESHCORE_REMEDIATION_0.3.10.md). Physical-radio acceptance remains necessary; advanced official-app feature gaps are separate work.

## MeshCore integration corrections in 0.3.9

Improves the ten groups from the 0.3.8 audit. The [0.3.9 re-audit](audits/MESHCORE_INTEGRATION_AUDIT_0.3.9.md) confirms eight groups within the reviewed boundaries and identifies remaining contact synchronization and pre-write-callback recovery cases. See the [correction and validation matrix](audits/MESHCORE_REMEDIATION_0.3.9.md). Physical-radio testing remains necessary; advanced official-app feature gaps remain separate.

## Meshtastic settings corrections in 0.3.8

Corrects all eight finding groups from the 0.3.7 settings audit: preset coding rates, full effective LoRa readback, firmware gates, regional slots/power, false-default flags, modem capabilities, licensed identity and independent module availability. Existing regression checks plus 206 new conformance checks pass. See the [correction and validation matrix](audits/MESHTASTIC_REMEDIATION_0.3.8.md). Hardware/RF testing remains necessary; this does not claim every official-app editor is implemented.

## Included in this beta

**Retained from 0.2.5:** Home opens with side-by-side Meshtastic and MeshCore radio cards, Bluetooth RSSI bars/dBm for each and a tighter Nodes/Chats/Channels grid without duplicate headings. **From 0.2.4:** Settings → Appearance & colors offers System, Color, Gradient (Linear/Mesh), and Photo with Dim/Blur. Selections are saved on the phone across normal updates. **From 0.2.3:** permanent release APK signing, complete history/preferences/tracks ZIP backup and restore, and legacy GPX import. **From 0.2.2:** Messages now has independent **Chats, Channels and Nodes** lists for both radios. Channels appear even without a message history, and selecting a messageable node starts a direct chat. Offline Maps has a **visual map with pan/zoom and Draw area rectangle**, maximum offline zoom, estimated tile limits and saved-region overlays. The 0.2.1 short numeric PIN remains unchanged. The 0.2.0 local HTTPS workspace, independent Meshtastic/MeshCore browser messaging, offline regions and track folders remain. The paired Home cards from 0.1.1 are retained. This does not bridge or relay mesh traffic.

**Previous debug-signed beta:** [Android 0.2.2](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.2). **Other older betas:** [Android 0.2.1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1), [Android 0.2.0](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.0), [Android 0.1.1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.1) and [0.1.0](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0) remain available.

Independent dual BLE connections; chats, channels and contacts; a map with right-side icons and nonmodal lower panels; optional node grouping in Layers; tracks, GPX, POIs and offline map areas; persistent message history and JSON import/export. Each network keeps its own messages, identities and routing. There is no bridging.

The app uses the phone's location. Track recording does not automatically broadcast your location to either mesh. Offline map availability depends on the successfully downloaded area, source and zoom levels; test the area without internet before a trip.

## Compact Home and radio BLE signal (0.2.5)

Home now begins with **two radio cards**, one for Meshtastic and one for MeshCore, in their permanent orange/cyan identities. The redundant `MT ready · MC ready` strip, global `Dual mesh dashboard` summary and repeated section headings were removed. Nodes, Chats and Channels remain side-by-side with tighter spacing. **Remote Access** remains available further down, along with GPS and Tracks.

Each radio card measures **Bluetooth RSSI** from the phone's actual connected device (around every 20 seconds during idle GATT periods). Bars and dBm describe only the Bluetooth connection between the phone and its radio, **not LoRa signal strength or network range**. When unavailable, the card says BLE signal unavailable. After disconnection, it may show a muted last-known reading associated with that same radio; old measurements are not treated as live.

**Install over 0.2.4** with the same signing key to retain all chats, saved radios, favorites, tracks, themes and maps. Do **not** uninstall to update.

## Appearance & colors (0.2.4)

Under **Settings → Appearance & colors**, choose one of four themes inspired by familiar mobile-mesh applications:

- **System** follows Android's current light or dark mode.
- **Color** applies a solid palette; move the hue slider or tap one of six color swatches.
- **Gradient** supports **Linear** or **Mesh** gradients, with adjustable hue.
- **Photo** lets you pick a picture from your phone and independently adjust **Dim (30–90%)** and **Blur (0–24)**.

Theme preferences live in existing private Android storage and **survive installing the next version over 0.2.4**. Photo mode stores a private re-encoded image copy (without original EXIF metadata) and does not upload it. Meshtastic remains orange and MeshCore remains blue wherever these colors denote network identity. The computer browser workspace currently retains its own styling. A complete ZIP backup includes theme preferences, but not the photo image itself; after uninstalling, reselect that photo.

**Update in place:** If you already have 0.2.3 or newer, download the 0.4.0 APK and tap **Update**, not Uninstall. Do not clear app data. The verified permanent certificate and package ID are unchanged.

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
| 0.2.2 | Separate web Chats, Channels, Nodes and visual offline map selection; debug-signed |
| 0.2.3 | First permanent-signed beta; full ZIP backup/restore and GPX import |
| 0.2.4 | System, Color, Gradient and Photo appearance personalization |
| 0.2.5 | Compact Home, real per-radio BLE RSSI indicators and dense card layout |
| 0.2.6, 0.2.7, etc. | Fixes and small improvements |
| 0.3.0, 0.4.0, etc. | Future substantial changes; patch resets to zero |
| 1.0.0 | First production release after core scope and stability acceptance |

Android 0.4.0 uses versionCode 31. All 0.x versions remain in development. The earlier alpha02–alpha07 public-looking numbers were development iteration labels, not releases in the new sequence. Android versionCode remains strictly increasing (0.1.0 uses 8, 0.1.1 uses 9, 0.2.0 uses 10, 0.2.1 uses 11, 0.2.2 uses 12, 0.2.3 uses 13, 0.2.4 uses 14, 0.2.5 uses 15). Controller firmware keeps its independent numbering and recovery images.

## Help test

Check initial connection and reconnection to both radios, sending/receiving, unread state, map grouping, offline areas, tracks and backup/import on your setup. [Report a bug](https://github.com/Renbrant/MeshCrossway/issues) with app version, phone/Android version, each radio model/firmware, steps, expected result and actual result. Redact keys, personal messages and precise locations.

Wardriving/MeshMapper upload is a future possibility and is **not included** in 0.2.5.

## Development references

[Android source/build guide](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0/android) · [Development history and wire references](android-development-history.md) · [Technical hub](TECHNICAL_DOCUMENTATION.md)

The source build guide contains historical alpha references; this page and the public release define the current beta number and download.
