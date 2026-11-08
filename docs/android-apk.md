# MeshCrossway Android controller

Tracking: [#187](https://github.com/Renbrant/MeshCrossway/issues/187). Implementation: [#188](https://github.com/Renbrant/MeshCrossway/pull/188).

## Direction and validated baseline

Android is the priority for a broadly accessible MeshCrossway controller. The ESP32-S3 controller remains a dedicated option. Both use independent Meshtastic and MeshCore radios; no bridging, forwarding or automatic retransmission across networks.

On a Samsung Galaxy S25, the first APK (`0.1.0-alpha01`) was installed and displayed **Protocol verified** for two different Heltec V3 radios simultaneously: Meshtastic PhoneAPI configuration and MeshCore Companion identity. This is user-reported hardware evidence of initial simultaneous connection/protocol identification, not validation of message delivery, long-duration background operation, all Android phones, or the new alpha02 functionality.

## 0.2.0-alpha02 — messaging increment

Implemented together:

| Feature | Behavior |
| --- | --- |
| Independent live clients | Continuous Meshtastic FromRadio reads and MeshCore waiting-message polling/subscriptions; separate state and queues for each radio. |
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

Recent history loads up to 2,000 messages per radio initially. **Load older messages** increases the window by 2,000 up to 20,000; the database keeps older records. Unread counts and conversation lists apply to the currently loaded window. History export, deletion controls and unlimited pagination are subsequent work.

Transmission guards include protocol readiness, current radio identity, enabled/current channel identity, LoRa region/TX enable for Meshtastic, destination availability, UTF-8 byte limits, queue bounds and negotiated ATT payload. Text limits are conservative (220 bytes for Meshtastic; MeshCore derives its limit from the name prefix and frame size, at most 150). The UI displays the UTF-8 byte count and keeps the draft if validation fails.

A connection loss or app restart marks pending outbound messages **delivery unknown**. The app does not resend them automatically: they might already have reached the radio or recipient. Duplicate suppression uses the Meshtastic sender/packet ID, or MeshCore conversation/timestamp/sender/text hash; MeshCore replay detection ignores changing SNR/path metadata.

### Still pending

- Actual map rendering, phone GNSS/location, pinch zoom, tracks, POIs and offline-area downloads/deletion. Map tools remain explicitly marked as future work.
- Creating/editing channels and radio configuration, MeshCore repeater administration/rooms, attachments, notifications for individual messages and localization.
- Production signing/distribution, extended history management and robust process-death/background acceptance on physical phones.
- Hardware validation of alpha02 send/receive, ACKs, reconnection and background behavior on the S25 + both Heltec V3 radios.

## Design and source references

Native Android Java, minimum Android 8/API 26; compile/target API 35. No third-party runtime dependencies. `RadioService` owns GATT sessions; `ProtocolClient` implements the two independent wire clients; `HistoryStore` owns private SQLite data; `MainActivity` renders state and conversations. The two networks have no forwarding path.

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

The **Android alpha APK** GitHub Actions workflow builds/lints, runs JVM fixtures, uploads a named APK plus SHA-256 checksum, and runs the device/storage smoke test on an Android emulator. Download the latest successful run's `MeshCrossway-Android-0.2.0-alpha02` artifact under **Actions → Artifacts**, extract its ZIP and install the APK inside.

Output: `android/app/build/outputs/apk/debug/app-debug.apk`. Version code 2 / version name `0.2.0-alpha02`. Package `com.meshcrossway.android`; label **MeshCrossway Alpha**.

**Signing limitation:** this is a debug-signed test APK. Different CI machines may use different debug keys. If Android rejects an update due to a signature mismatch, uninstalling alpha01 may be necessary; alpha01 has no message history, but its saved radio selection will be removed. Once alpha02 contains history, keep it installed until a compatible update or export path is available. A durable project-managed signing key is required before routine upgrade distribution; never commit signing keys/passwords.

## Physical acceptance on the S25

1. Close other mesh apps, keep radios awake and select the Meshtastic and MeshCore radios separately. The latter needs **Companion BLE firmware**, not repeater firmware. Grant Nearby devices and respond to system PIN prompts. Android 8–11 additionally needs Location enabled for BLE discovery.
2. Wait for **Ready** on both networks. Verify Nodes and Groups against the official clients (used sequentially, not competing for the same BLE connections).
3. Open Groups, send one short text per network and confirm it on a separate peer radio. Send replies back. Repeat as a direct chat using a synchronized client contact.
4. Distinguish Bluetooth submission, radio acceptance and recipient ACK. A broadcast/channel message does not prove delivery to every node.
5. Confirm network/radio isolation, unread badges, search, drafts, rotation and retained history after closing/reopening the app. Replayed radio backlog should not duplicate messages or reset read state.
6. Disconnect only one radio, then power-cycle it; observe independent backoff/reconnect while the other session remains usable. A manual disconnect must stay disconnected.
7. Leave the screen off for 30+ minutes; repeat with Wi-Fi and car Bluetooth audio. Toggle Bluetooth and verify desired-session recovery. Repeat on another Android vendor before claiming broad compatibility.

No successful build or emulator smoke result establishes physical send/receive, coexistence with car audio, or long-duration radio reliability.
