# MeshCrossway Android controller

Tracking: [#187](https://github.com/Renbrant/MeshCrossway/issues/187). Implementation: [#188](https://github.com/Renbrant/MeshCrossway/pull/188).

## Direction and validated baseline

Android is the priority for a broadly accessible MeshCrossway controller. The ESP32-S3 controller remains a dedicated option. Both use independent Meshtastic and MeshCore radios; no bridging, forwarding or automatic retransmission across networks.

On a Samsung Galaxy S25, the first APK (`0.1.0-alpha01`) was installed and displayed **Protocol verified** for two different Heltec V3 radios simultaneously: Meshtastic PhoneAPI configuration and MeshCore Companion identity. This is user-reported hardware evidence of initial simultaneous connection/protocol identification, initial evidence only. The user subsequently tested alpha02 on the same S25 with both radios Ready, 203 Meshtastic nodes and 350 MeshCore contacts/6 channels. Both radios accepted outgoing text; a MeshCore weather bot replied to an app request. Initial connection required repeated attempts. Long-duration background reliability and other phone models remain unverified.

## Earlier functional reference: 0.3.1-alpha04

Accepted by the project maintainer on November 8, 2026 as the Android functional reference for subsequent changes. Preserve this tested APK as a recovery reference; this acceptance does not establish production stability.

- Tested source: `d7052bbf9dcc1c0f4f24e06da6665ec660da6c56`.
- APK: `MeshCrossway-0.3.1-alpha04.apk`; SHA-256: `c0be5760d51f7d2389cdb2f23d891136436ad9f431178a246eec7c06503c6102`.
- [Validated build and downloadable APK artifact](https://github.com/Renbrant/MeshCrossway/actions/runs/42524563365): protocol fixtures, build/lint and API 29/35 emulator smoke tests passed.
- User report and screenshots on the Samsung Galaxy S25 show both Meshtastic and MeshCore Ready concurrently, synchronized nodes/channels, MeshCore outgoing messages and weather/test bot replies, plus map markers/clusters and phone GPS with displayed accuracy of ±4 m.
- The previously reported MeshCore connection failure no longer occurs in this test after the MTU compatibility fix.
- Still unverified for alpha04: peer-confirmed Meshtastic send/receive, direct-message delivery, offline region downloads in the field, persistence/reconnect after restart, prolonged screen-off/background use, car audio coexistence and other phone models.

CI APKs use a runner-generated debug signing key. A different run may require backup/export before uninstalling an incompatible installed build; keep the exact tested APK above for recovery.

## 0.2.0-alpha02 — messaging increment

Implemented together:

| Feature | Behavior |
| --- | --- |
| Independent live clients | Continuous Meshtastic FromRadio reads with a 30-second PhoneAPI heartbeat and MeshCore waiting-message polling/subscriptions; separate state and queues for each radio. |
| Synchronization | Meshtastic PhoneAPI configuration, NodeDB and channels; MeshCore identity, contacts and all advertised channel slots. No radio-region, key or frequency changes. |
| Channel text | Open Groups, choose a synchronized channel and compose text. Channel history includes a fingerprint of its configuration/key, preventing a changed slot from mixing conversations. |
| Direct text | Open Nodes, select a peer and choose Message. Meshtastic supplies the recipient public key when the radio supports PKI. MeshCore direct chat requires a client contact and an unambiguous six-byte public-key prefix; repeater/room commands are not implemented. |
| Durable history | SQLite in private app storage, keyed by network + radio + message ID; duplicate backlog packets preserve the existing read/delivery state. Writes and queries run on a worker. Outgoing text is committed before the BLE transmission is released. |
| Offline use and drafts | Saved nodes, channels and messages can be browsed offline. Drafts persist per radio/conversation and survive Activity recreation. Offline messages are not automatically queued for later transmission. |
| Delivery state | Saved/queued, submitted over Bluetooth, accepted by radio, recipient acknowledged, rejected, or delivery unknown. Channel acceptance never claims recipient delivery. Late callbacks cannot downgrade a delivered message. |
| Unread and conversations | Unread counts, conversation previews ordered by most recent message, sender and local timestamps. Opening a visible conversation marks its incoming messages read. |
| Search and filters | Nodes, Groups and Chats filter by network and text. Node details include last heard, available battery and radio-reported coordinates. |
| Reconnection | Each desired session retries independently with 2/4/8/16/32-second backoff, then pauses. Bluetooth-on can resume desired connections. Manual disconnect stops retries. A Settings switch disables automatic reconnect. Reopening the app can resume successfully saved radios. |
| Background ownership | A connectedDevice foreground service owns both GATT sessions independently of screen navigation. The notification can disconnect all radios. No boot receiver or automatic process-death service restart. |
| UI | Firmware palette, official logo, six bottom navigation destinations and left context rail. Composer remains intact during incoming updates; system bars and keyboard insets are handled. |

Lists and conversation messages initially render 100 entries, with explicit buttons to show more. Recent history loads up to 2,000 messages per radio initially. **Load older messages** increases the window by 2,000 up to 20,000; the database keeps older records. Unread counts and conversation lists apply to the currently loaded window. Alpha03 adds history export/import; full history deletion and unlimited pagination remain subsequent work.

Transmission guards include protocol readiness, current radio identity, enabled/current channel identity, LoRa region/TX enable for Meshtastic, destination availability, UTF-8 byte limits, queue bounds and negotiated ATT payload. Text limits are conservative (220 bytes for Meshtastic; MeshCore derives its limit from the name prefix and frame size, at most 150). The UI displays the UTF-8 byte count and keeps the draft if validation fails.

A connection loss or app restart marks pending outbound messages **delivery unknown**. The app does not resend them automatically: they might already have reached the radio or recipient. Duplicate suppression uses the Meshtastic sender/packet ID, or MeshCore conversation/timestamp/sender/text hash; MeshCore replay detection ignores changing SNR/path metadata.

## 0.3.0-alpha03 — dual-network field tools

Tracking: [#189](https://github.com/Renbrant/MeshCrossway/issues/189). Functional research and independent adaptations: [reference matrix](android-feature-references.md).

- Unified map: Meshtastic orange, MeshCore cyan, clustered markers, network/favorite/recent filters, pinch zoom and node-to-map shortcuts. Node identity always includes its network and radio.
- Optional foreground phone GPS for local position and distance sorting. Local POIs support add, inspect and delete. GPS/POIs are never automatically transmitted.
- Named offline map areas use the visible viewport and selected maximum zoom. Inspect bounds, pause/resume or delete saved areas. OpenFreeMap limits apply: this app conservatively estimates at most 2,400 tiles, stops near 2,800 tiles or 45 MB, uses one active region and at most four HTTP requests. Downloads pause when the app leaves the foreground. Oversized areas must be reduced; an incomplete area does not provide full offline coverage.
- Combined inbox with network/unread/pinned filters; node favorites, pinned/muted conversations, quick-reply drafts and per-network message alerts. Message previews are opt-in. Backlog imports do not generate live-message notifications.
- Connection assistant shows protocol hints, signal and pairing state, serializes initial GATT setup, then keeps both networks active. Saved-radio reconnect, explicit Android pairing and phase diagnostics are available independently.
- User-initiated JSON export/import via Android's document picker. Import merges without erasing existing rows; network/radio keys remain separate and pending outgoing messages become delivery-unknown rather than being resent. Treat exports as private: they include messages, node public keys, drafts and location data.

### Installation and signing

Alpha03 uses package **com.meshcrossway.mobile**, label **MeshCrossway**, version code **3**. It installs alongside alpha02 (**com.meshcrossway.android**, **MeshCrossway Alpha**) so existing history can remain intact. There is no automatic access to alpha02 private storage; alpha02 has no export feature. Disconnect radios in the old app before connecting them in the new one.

The distributed preview APK is signed with a persistent private preview key. The CI artifact uses the runner's debug key and is not an interchangeable upgrade source. Never commit the private signing key or password. Keep alpha02 installed until its history is no longer needed; uninstalling either app deletes that app's private data.

### Still pending

Track recording, radio/channel editing, repeater/room administration, attachments, localization and process-death/background recovery acceptance remain future work. Alpha03 map, GPS, offline downloads and improved pairing require physical-phone acceptance.

## Design and source references

Native Android Java, minimum Android 8/API 26; compile/target API 35. MapLibre Native Android 11.13.5 renders maps with OpenFreeMap vector data; the map attribution and SDK license are included. `RadioService` owns GATT sessions; `ProtocolClient` implements the two independent wire clients; `HistoryStore` owns private SQLite data; `MainActivity` renders state and conversations. The two networks have no forwarding path.

Palette from `firmware/display-prototype/meshcrossway_ui.h`: background `#090C10`, panels `#181D24` / `#242B34`, accent `#FF8A24`, text `#F2F4F7`, muted `#9AA6B4`. The official logo retains its own `#F07120` / `#F2F2F2` colors and proportions; the bitmap was rasterized from the unchanged SVG. The generic radio launcher icon is not a replacement brand mark.

Protocol behavior is referenced against `meshcrossway_ble_link.inc`, `meshcrossway_meshtastic_protocol.h`, and `meshcrossway_meshcore_protocol.h` in `firmware/display-prototype/`, starting from main commit `3aaaf32f371116bb06559a3958b69631370057b4`. Public keys may be stored with node data. Channel secrets/private identities are not stored or logged; only a one-way fingerprint identifies channel history. App database and preferences are excluded from backup/transfer rules. Uninstalling the app still deletes private data.

Android guidance: [BLE background communication](https://developer.android.com/develop/connectivity/bluetooth/ble/background), [GATT API](https://developer.android.com/reference/android/bluetooth/BluetoothGatt), [foreground service types](https://developer.android.com/develop/background-work/services/fgs/service-types), [Bluetooth permissions](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions).

## Build, tests and distribution

Requirements: JDK 17, Android SDK platform/build tools 35. Open `android/` in Android Studio or set `ANDROID_HOME`:

```sh
bash scripts/test_android_probe.sh
cd android
./gradlew assembleDebug lintDebug
# Android emulator/device attached:
./gradlew connectedDebugAndroidTest
```

The dependency-free JVM fixtures test bootstrapping, channel/DM encoding, Meshtastic PKI, ACK ordering, malformed frames, byte limits, replay IDs, network isolation and unread aggregation. Device instrumentation exercises the real SQLite schema (duplicates, network/radio separation, monotonic delivery state, read markers, reopening and draft persistence), navigation and draft preservation during Activity recreation. Neither replaces physical-radio acceptance.

The **Android alpha APK** GitHub Actions workflow builds/lints, runs JVM fixtures, uploads a named APK plus SHA-256 checksum, and runs the device/storage smoke test on Android API 29 and 35 emulators. Download the latest successful run's `MeshCrossway-Android-0.5.0-alpha06` artifact under **Actions → Artifacts**, extract its ZIP and install the APK inside.

Outputs: `android/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk` for phones and `app-x86_64-debug.apk` for emulators. CI runs API 29/35 SQLite, backup migration/import, navigation, native dual-map fixture and offline-region lifecycle smoke tests. Fixture maps do not require a public tile service. Successful automated checks do not replace physical-radio testing.

## Physical acceptance on the S25

1. Close other mesh apps, keep radios awake and select the Meshtastic and MeshCore radios separately. The latter needs **Companion BLE firmware**, not repeater firmware. Grant Nearby devices and respond to system PIN prompts. Android 8–11 additionally needs Location enabled for BLE discovery.
2. Wait for **Ready** on both networks. Verify Nodes and Groups against the official clients (used sequentially, not competing for the same BLE connections).
3. Open Groups, send one short text per network and confirm it on a separate peer radio. Send replies back. Repeat as a direct chat using a synchronized client contact.
4. Distinguish Bluetooth submission, radio acceptance and recipient ACK. A broadcast/channel message does not prove delivery to every node.
5. Confirm network/radio isolation, unread badges, search, drafts, rotation and retained history after closing/reopening the app. Replayed radio backlog should not duplicate messages or reset read state.
6. Disconnect only one radio, then power-cycle it; observe independent backoff/reconnect while the other session remains usable. A manual disconnect must stay disconnected.
7. Leave the screen off for 30+ minutes; repeat with Wi-Fi and car Bluetooth audio. Toggle Bluetooth and verify desired-session recovery. Repeat on another Android vendor before claiming broad compatibility.

No successful build or emulator smoke result establishes physical send/receive, coexistence with car audio, or long-duration radio reliability.


### Alpha04 MeshCore MTU compatibility fix

The alpha03 transport incorrectly required an ATT MTU of 179 for MeshCore.
ESP32 MeshCore configures its MTU to `MAX_FRAME_SIZE` (176), so a valid
176-byte negotiation was rejected before notifications or protocol synchronization.
Alpha04 accepts 176, reserves the three-byte ATT header (173-byte write budget),
and checks every command before writing; it does not split protocol frames.
Diagnostics now retain the negotiated MTU and report callback status/value on failure.
A displayed 23 in alpha03 is not proof that the peer negotiated 23: the old code
updated that field only after passing the incorrect threshold.

Reference: https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/esp32/SerialBLEInterface.cpp

Regression fixtures cover 23, 175, 176 and 185. Initial physical dual-radio acceptance
was reported on November 8, 2026; see the current functional reference above for its scope and remaining checks.

## 0.4.0-alpha05 — tracks and field usability

Built on the hardware-accepted alpha04 without changing either BLE transport.

- Local GPS tracks: start, pause, resume, finish/save, rename, color, show/hide,
  delete saved tracks and export GPX using Android's file picker. Access from
  **Home → Tracks**, **Map → Tracks** or **Settings → Tracks and GPX export**.
- Recording uses a dedicated location foreground service and its own SQLite
  worker/database, independent of both radio queues. Precise GPS permission is
  requested only when recording is started. A notification offers Pause.
- Fresh GPS samples (no older than 30 s), reported accuracy ≤50 m, minimum
  2-second sampling and ≥3 m movement reduce cached fixes and stationary noise.
  Implausible jumps over 150 m/s are rejected. Gaps over 2 minutes and manual
  resumes create separate segments and do not add straight-line gap distance.
- Up to 100 tracks / 100,000 points per track. On-map preview uses at most
  approximately 5,000 sampled points per visible track; GPX contains all saved
  points. Accuracy filtering is not a navigation guarantee.
- Phone GPS card: coordinates, accuracy, fix age, available altitude and speed.
  Track recording is distinct from the optional foreground map-position switch.
- Node types: clients/repeaters/rooms for MeshCore, with independent network and
  favorite filters; distance ordering updates the displayed selected order.
- Node/POI distance and true-north bearing use the latest phone position and the
  saved target coordinates. They are straight-line measurements, not turn-by-turn
  routes or device-heading compass guidance.
- Groups sort by latest loaded message and show unread counters using exact
  channel topic identities. Chat decimal coordinates open on the local map.
- Up to 8 editable quick replies (120 characters each), inserted into the current
  network's draft without sending or replacing existing text.

Tracks are local and never broadcast automatically. **JSON history backup does
not include tracks**; export each required track as GPX before uninstalling or
switching to an APK with an incompatible signing key. GPX import is not included
in this increment. Force-stop/restart preserves committed points and reopens an
interrupted track as paused; it does not restart recording automatically.

Validation for this increment includes pure GPS quality/geometry/GPX fixtures
and Android SQLite lifecycle/export tests in addition to existing dual-protocol,
history migration, map and navigation checks. Hardware acceptance must confirm
real GPS with screen off and simultaneous BLE sessions before promotion above
the alpha04 recovery reference.


### Alpha05 automated validation and download

- Tested source: `a8de13c35cb5583969568b1331670ffb08fb459e`.
- [Build, lint, protocol fixtures and API 29/35 tests](https://github.com/Renbrant/MeshCrossway/actions/runs/42527357099) all passed.
- [APK ZIP artifact](https://github.com/Renbrant/MeshCrossway/actions/runs/42527357099/artifacts/13635622269): `MeshCrossway-0.4.0-alpha05.apk` (version code 5, package `com.meshcrossway.mobile`).
- APK SHA-256: `abc17a79408e0102e365c62dad3a2ee48a7aab560ff6ad1772f0dfb7b4ce82cb`.
- Device tests cover SQLite persistence, interrupted-track recovery, GPX export,
  the real location foreground-service lifecycle and notification, simulated GPS
  points, activity backgrounding, pause/resume/finish and existing dual-map/history
  navigation. Emulator default fixes are suspended during synthetic-point
  injection so GPS jump rejection is tested independently of that fixture.
- The APK signing certificate differs from the distributed alpha04 CI APK.
  Export history first; if Android refuses an in-place update, uninstall the old
  app only after exporting, then install alpha05 and import the JSON backup.
  Uninstalling deletes private app data. Alpha04 remains the hardware-tested recovery reference.

Real phone GPS endurance with screen off and two physical BLE radios still needs
maintainer acceptance; passing emulator tests does not promote alpha05 to stable.

## 0.5.0-alpha06 — navigation and settings redesign

The maintainer reported alpha05 working before requesting this UI-only increment.
Preserve the alpha05 APK as the immediate functional recovery reference; alpha04
remains the earlier explicitly documented dual-radio reference. No changes in
alpha06 are made to the radio protocols, GATT transport, history schema, map
provider/download limits or track-recording service.

- Five primary destinations: Home, Chats, Contacts, Map and Settings. Chats has
  Conversations / Channels tabs; Home also has direct entry points to both.
- Compact dashboard: separate Meshtastic/MeshCore connection rows, combined
  messaging entry points and field tools. Orange/cyan network identification
  remains visible in the status strip, lists and radio settings.
- Settings index: individual radio pages, Connections, Notifications & messages,
  Map & location, Data & backup and About. Existing actions are grouped as icon,
  title and subtitle rows, with switches beside the settings they control.
- Each radio has its own identity/status, device picker and connection actions.
  Diagnostics and system pairing are under Advanced on that radio's page.
- Consistent toolbar Back and Android Back restore the prior screen, including a
  chat draft when returning from settings. The nested route stack survives
  Activity recreation. Contact search and scroll position survive tab changes.
- Chat actions move to the toolbar overflow; contact sorting and type selection
  use labeled choices rather than buttons that cycle through hidden states.
- Full-width map with a compact Layers / GPS / Fit / Tools toolbar. Offline maps,
  tracks, POIs, zoom and north-up remain available from Tools. Settings shortcuts
  open those tools on the map. Native pinch/zoom remains available.
- Original scalable line icons replace font-dependent navigation symbols.

Functional UX research: the public MeshCore Open dev settings screen, app
settings screen, contacts screen and quick-switch widget were inspected. The
useful patterns were grouped compact settings rows, explicit subpages, Back,
contextual overflow actions and a small primary navigation surface. This is an
independent Java UI implementation; no Flutter code, icons or assets were copied.

Reference: https://github.com/zjs81/meshcore-open/tree/dev/lib/screens
and https://github.com/zjs81/meshcore-open/blob/dev/lib/widgets/quick_switch_bar.dart

Acceptance: Android API 29/35 navigation, back-stack/draft recreation, menu entry
points and map rendering are checked in instrumentation. Real-phone usability
and simultaneous physical BLE sessions still need maintainer acceptance of this
new interface. The JSON backup and separate GPX export remain under Settings →
Data & backup; export before uninstalling an incompatible signed CI build.


### Alpha06 validation and APK

- Tested source: `94aa838136cea562824c33f50c006601119dc710`.
- [Final Android validation](https://github.com/Renbrant/MeshCrossway/actions/runs/42553954547): protocol/field fixtures, build/lint and API 29/35 instrumentation all passed.
- Menu tests cover each settings category, return to the index, retained contact search across tabs, and return from nested settings to a chat draft after Activity recreation.
- Existing history/backup, track/service, offline-area and rendered dual-network marker tests also passed. Emulator screenshots of Home, radio settings, app settings, chat and map were visually inspected.
- [Download APK ZIP](https://github.com/Renbrant/MeshCrossway/actions/runs/42553954547/artifacts/13645719395): `MeshCrossway-0.5.0-alpha06.apk`, version code 6, `com.meshcrossway.mobile`.
- APK SHA-256: `60157f96be8cf28de0fd2ea79aeec12e426e298a383aebc65c37a29d83e30b0e`.

This is a new CI debug signature. Export JSON history and required GPX tracks before replacing an incompatible installation. Alpha05 stays the immediate maintainer-reported functional recovery reference. Physical acceptance of the new menu layout on the S25 is still pending.

## 0.6.0-alpha07 — map side controls, live panels and message reception details

Alpha06 was accepted by the maintainer as a substantial usability improvement;
it remains the recovery reference for this increment.

- The map has original right-hand icon controls: basemap, Layers, GPS, fit nodes,
  tracks, offline maps, POIs, zoom and north-up. A scrollable rail remains usable
  while the lower panel is open.
- Layers, basemap controls, GPS information, tracks/track editing, POIs and offline
  region management use a nonmodal lower panel. The exposed map can still be
  panned/zoomed. Collapse/close controls and Android Back leave the map visible.
- Layers → Group nearby nodes is persistent and changes only the local GeoJSON
  source; it does not reload the basemap or reset the camera. Individual markers
  remain available at low zoom when grouping is off.
- Offline download selection uses the exposed map above the panel. Pan/zoom updates the selection automatically when the camera stops; the outline
  previews the selected bounds. Existing size/tile/request limits remain unchanged.
- Received messages show hops and available SNR/RSSI; tap the message or Details
  to inspect reception metadata and the available path. No new traceroute or
  mesh traffic is sent automatically.

### What a received path means

For MeshCore, the Companion path-length byte packs hash width and hop count.
The lower 6 bits are a hop count; the high bits select 1/2/3-byte hash prefixes.
0xFF means direct routing, not a proven zero-hop path. V3 adds receive SNR but
ordinary text notifications contain no complete path list.

For live MeshCore channel copies, the app can observe firmware RX-log frames.
It verifies the channel hash and firmware HMAC-SHA256 tag, then decrypts the
version-1 group payload using the already-synchronized channel key. That key is
kept only in the current client's memory, never written to history or logs.
The observed ordered path is attached only to a canonical radio text notification
matching channel identity, sender timestamp, text type and complete body. A raw
log cannot create a new user message. Correlation caches are bounded; observations
expire after two minutes. A late observation enriches an existing message without
resetting read/delivery state or generating another notification.

The details view labels this as **one observed copy**, resolves hash prefixes
against saved contacts, and explicitly marks unknown/ambiguous matches. Prefixes
are not unique proof of a repeater's identity. Later copies can take other routes.
Private messages, old backlog, truncated/unsupported logs and direct-routing
packets may have no complete captured path. The app states that limitation.

For Meshtastic text, hop_start minus remaining hop_limit gives the count when
hop_start is populated and consistent. A missing/zero hop_start stays unknown
rather than falsely direct. Available SNR, RSSI, MQTT flag and relay-node suffix
are shown; that suffix is not a complete route. A new traceroute cannot reconstruct
the historical path of an already received text and is not presented as one.

History schema 3 adds reception metadata without deleting messages. Old JSON
backups import with unknown metadata; new exports preserve metadata and observed
hashes. Existing deduplication identities and network/radio scoping are unchanged.

Primary wire references:
- https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp
- https://github.com/meshcore-dev/MeshCore/blob/main/src/Packet.cpp
- https://github.com/meshcore-dev/MeshCore/blob/main/src/Utils.cpp
- https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto

The supplied MeshMapper screenshot informed the right-side controls and live
lower-panel workflow; no source, icons or assets were copied. Physical acceptance
must confirm live RX logs on the radio firmware and the new touch workflow.

### Alpha07 validation and APK

Built source: `76e30a019e6cdeadf8915e4d5f9c62d4188b6e7d`.
[CI run 42558188171](https://github.com/Renbrant/MeshCrossway/actions/runs/42558188171)
passed the APK/lint/JVM job and device smoke tests on API 29 and API 35.
Smoke coverage includes schema migration/backup compatibility, message metadata,
late route enrichment, ungrouped native markers, live map panels, offline selection,
track recovery and existing settings/chat flows. Emulator captures were visually inspected.

[Download the alpha07 APK artifact](https://github.com/Renbrant/MeshCrossway/actions/runs/42558188171/artifacts/13648073860)
(the GitHub artifact is a ZIP containing the APK and checksums).

APK SHA-256: `c2ad8690ad1d3b8e8db20b1df9efd299eed0d2ce2c8a2abe31053f965168a848`.

This is a new CI debug signature. Export JSON history and required GPX tracks
before uninstalling an incompatible existing installation. Alpha06 remains the
maintainer-accepted recovery reference. Alpha07 physical dual-radio acceptance
and live route capture on the maintainer's firmware remain pending.

## Android 0.x version policy and 0.1.0 baseline

The maintainer reported normal operation of alpha07 and approved it as the functional Android 0.1.0 baseline. This resets the public Android version sequence; earlier 0.2.0–0.6.0 alpha labels were development builds, not releases in this sequence. Controller firmware versions are independent.

- 0.1.0: accepted functional baseline, with the same app behavior as 0.6.0-alpha07.
- 0.1.1, 0.1.2, 0.1.3: fixes and small improvements.
- 0.2.0 and subsequent minor versions: substantial features or workflow changes.
- 1.0.0: first production release after the agreed core scope and stability are validated.

Use three numeric components consistently. Reset the patch number to zero on a minor increment. A 0.x release remains in development even when accepted as a recovery baseline. Keep Android versionCode strictly increasing across all builds, including this public version reset (0.1.0 uses versionCode 8). Never reset the package ID, history schema or stored data merely to change the display version.

The originally tested APK is preserved as MeshCrossway-0.6.0-alpha07.apk, SHA-256 c2ad8690ad1d3b8e8db20b1df9efd299eed0d2ce2c8a2abe31053f965168a848, built from 76e30a019e6cdeadf8915e4d5f9c62d4188b6e7d. Renumbering the source does not rename that APK's embedded version. Its automated validation and download are recorded above. A newly built 0.1.0 APK changes only version metadata and artifact naming; no wardriving integration is included.
