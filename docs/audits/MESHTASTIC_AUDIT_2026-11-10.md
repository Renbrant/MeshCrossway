# Meshtastic integration audit — 2026-11-10

## Verdict and scope

**The Android 0.3.1 Meshtastic integration is not ready to be described as fully verified or at feature parity with the official app.** Basic transport and messaging exist, but configuration contains confirmed source-level defects, including incorrect modem preset values. Passing the release build did not cover these administrative flows.

This is a static source/protocol review, supplemented with the available automated checks. No phone, BLE radio, RF transmission, firmware reboot, or end-to-end settings persistence was exercised during this audit. Effects depending on firmware are identified as risks rather than field observations. This review changes documentation only.

### Compared revisions

- MeshCrossway Android 0.3.1: `00ecd4bb1090150103cf189493dad52986ec6430` ([release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.3.1)). The local ProtocolClient, MainActivity, RadioService and Wire Git blob hashes match the published source tree exactly.
- Official Android app: `4b557c67a731ecc0908de4db2619bae3a696e2f4`.
- Official protobufs: `4c80682b3b441a4cebab9f3e5c3097857cb5c81d`.
- Upstream main/master are moving development branches; newer features are a comparison inventory, not proof they are supported by every released firmware. Capability/version gates are required.

## Confirmed defects and remediation order

P1 = fix before relying on radio configuration; P2 = functional correctness or compatibility gap. Locations below refer to `android/app/src/main/java/com/meshcrossway/android/` in the audited release.

### A01 — P1: modem preset labels send incorrect enum values

`MainActivity.java:862`, `mtLoraSettings`: the UI uses array positions as wire enum values. Official values are MEDIUM_SLOW=3, MEDIUM_FAST=4, SHORT_SLOW=5, SHORT_FAST=6; the UI labels those indices in the opposite order.

| User chooses | Sent value | Actual official preset |
|---|---:|---|
| Medium Range - Fast | 3 | Medium Slow |
| Medium Range - Slow | 4 | Medium Fast |
| Short Range - Fast | 5 | Short Slow |
| Short Range - Slow | 6 | Short Fast |

Preset values >=7 are also displayed as the final array label through clamping, even though the radio may use another preset. This can cause a radio to stop communicating with the intended mesh after a save. Fix with explicit label/enum pairs, preserve unknown values, and filter by region/firmware. Add fixtures validating every offered label against official numeric values. Reference: [config.proto](https://github.com/meshtastic/protobufs/blob/4c80682b3b441a4cebab9f3e5c3097857cb5c81d/meshtastic/config.proto).

### A02 — P1: administrative writes have no result tracking

`ProtocolClient.java:73–97`: writes are queued with response `-1` and an empty UID; `written()` completes them after the BLE write. Setters immediately update the local cache. Routing errors are handled only through the normal text-message `sent` map, which does not contain these admin writes. There is no readback confirming that settings persisted, nor an explicit failed/unknown settings state.

A successful Bluetooth write therefore cannot establish that the radio accepted or saved the configuration. Track administrative request IDs and destination, handle routing errors/timeouts and expected reboot, and reconcile with a fresh read. Reference: official [CommandSenderImpl](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/core/data/src/commonMain/kotlin/org/meshtastic/core/data/manager/CommandSenderImpl.kt) and [RadioConfigViewModel](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/feature/settings/src/commonMain/kotlin/org/meshtastic/feature/settings/radio/RadioConfigViewModel.kt).

### A03 — P1: administrative session does not renew on expiry

`ProtocolClient.java:21,73–86`; `MainActivity.java:846`: the initial request is guarded by `mtAdminRequested`, and the key is subsequently checked only for nonzero length. There is no receipt timestamp, expiry check or refresh/retry state. The official protocol specifies a 300-second lifetime. Other GET responses may incidentally refresh the cached key, but do not provide a reliable renewal policy.

Refresh before expiry and defer writes until a current response arrives. Correlate responses to the local destination and pending request; `mtPacket()` currently accepts any ADMIN_APP payload without checking its sender/destination/request ID before replacing local admin state. Reference: [admin.proto](https://github.com/meshtastic/protobufs/blob/4c80682b3b441a4cebab9f3e5c3097857cb5c81d/meshtastic/admin.proto), official [SessionManagerImpl](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/core/data/src/commonMain/kotlin/org/meshtastic/core/data/manager/SessionManagerImpl.kt).

### A04 — P1: Ham onboarding uses the wrong administrative operation

`MainActivity.java:859` changes `User.is_licensed` through `set_owner`. The official app routes the local OFF→ON transition through `set_ham_mode` with HamParameters, callsign handling, and preserved power/frequency. The UI statement that enabling this switch performs the official encryption transition is therefore not supported by the implemented flow.

Implement the dedicated onboarding flow and firmware compatibility rules, or disable the switch until supported. Reference: official `saveUserConfig` in [RadioConfigViewModel](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/feature/settings/src/commonMain/kotlin/org/meshtastic/feature/settings/radio/RadioConfigViewModel.kt) and `setHamMode` in [AdminControllerImpl](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/core/service/src/commonMain/kotlin/org/meshtastic/core/service/AdminControllerImpl.kt).

### A05 — P1: module GET responses are ignored

`ProtocolClient.java:73`: `mtAdmin()` reads owner response field 4 and config response field 6, but not module response field 8. MQTT and status requests use `get_module_config_request`, whose reply is field 8. Initial FromRadio module streams are decoded, so this can be masked when a value was already streamed. Requested values cannot reliably populate or refresh the module cache.

Decode field 8 and update the correct module entry; add empty and nonempty response fixtures. Reference: `get_module_config_response` in official admin.proto.

### A06 — P1: settings UI does not observe Meshtastic state changes

`MainActivity.java:170`: `settingsFingerprint()` includes the new Meshtastic caches inside a condition requiring `session.profile == MESHCORE`. Changes to MT owner, configuration, modules and session key do not contribute to the refresh fingerprint. A newly received owner/config response can leave fields blank or stale until navigating again. Fix profile-specific observation and test an asynchronous GET arriving while a settings page is visible.

### A07 — P1: empty protobuf messages are treated as missing configuration

`ProtocolClient.java:79,81,85,87`; `MainActivity.java:859,866,868,869`: cache loading is inferred from `byte[].length`, rather than presence/loading state. In proto3, a valid section with all default values can encode to zero bytes. Its setters reject it as not synchronized, and rendering can repeatedly request it. Conversely, `setMtOwnerField`/`setMtOwnerName` can construct a partial User before the owner GET completes, and `setMtStatus` can start from an empty unsynchronized module.

Represent unloaded, loaded-default and failed separately. Disable writes until the relevant object has loaded; preserve the complete current object.

### A08 — P1: channel rename can be a no-op and leaves channel models inconsistent

`ProtocolClient.java:97`: name field 3 is replaced only if already present. Default channels can omit that string; then a rename queues a message without inserting the new name. The method also updates `mtChannels` but not the `channels` map or `listener.channel()`, leaving Groups/chat targets stale. A later initial synchronization hashes all ChannelSettings bytes, so changing a name or unrelated channel setting changes the conversation identity and can split history/favorites.

Use an insert-or-replace helper; validate the official channel-name byte limit (the current writer allows 32 bytes despite the upstream short-name constraint); update/reconcile all channel state only after confirmed application. When frequency slot is automatic, a primary-channel name affects frequency selection and must be presented accordingly. Reference: [channel.proto](https://github.com/meshtastic/protobufs/blob/4c80682b3b441a4cebab9f3e5c3097857cb5c81d/meshtastic/channel.proto).

### A09 — P2: firmware and hardware compatibility gates are missing

`ProtocolClient.java:51` reads only `hasPKC` from DeviceMetadata. Firmware version, hardware model, excluded modules and `has_xeddsa` are not retained for MT. `MainActivity.java:859,862,866,868` offers status message, unmessageable, packet signature policy, region and deprecated roles without the corresponding version/capability filtering. Hardware is shown as a placeholder rather than the reported model. Short and long name limits are not selected by firmware; status is capped at 80 Java characters instead of 80 UTF-8 bytes.

Store metadata and gate fields using tested version/capability predicates. Do not assume unsupported radios implement an offered control. Reference: official mesh.proto DeviceMetadata and settings-radio-user guide.

### A10 — P2: manual LoRa mode and validation are incomplete

`MainActivity.java:862–863`: Use Preset can be turned off, but manual bandwidth and spreading-factor inputs are absent; coding-rate selection still offers the preset-default zero value. Region/preset compatibility, supported frequency slots, finite/range validation for the frequency override, coding-rate override rules and hardware SF restrictions are incomplete. The displayed default for OK to MQTT is true when omitted, while proto3/default guidance is false. Region codes above 26 are absent.

Use firmware-aware enums and a validated complete draft for the LoRa section; do not display guessed defaults. Reference: official config.proto and settings-radio-user guide.

### A11 — P2: configuration reads can leave send guards stale

`ProtocolClient.java:48` updates `region`, `hopLimit`, and `txEnabled` for streamed FromRadio Config. `mtAdmin()` updates only the raw config cache for an administrative GET response. Sending uses the separate scalar values at lines 164–171. A refreshed LoRa config can therefore disagree with the guards/hop value used for messages. Decode both paths through one configuration reducer.

### A12 — P2: replies and reactions are not interoperable protocol features

`ProtocolClient.java:58–71,168–171` reads text payloads but does not preserve Data.reply_id or Data.emoji. Message actions compose ordinary text mentions/quotes. The local Message model has no structured reply/reaction association. This is a feature gap relative to the official app, not a failure of ordinary text messages. Add Data field support, persistent associations and cross-app fixtures. Reference: official mesh.proto Data and messages-and-channels guide.

### A13 — P2: node and telemetry support is a small subset

`ProtocolClient.java:38–43,66`: node name, public key, coordinates, last-heard and battery are retained. User short name, role/hardware detail, status, and most telemetry (environment, power, air quality, historical metrics) are not modeled. Traceroute, neighbor info and remote admin/shell are absent. Distinguish unsupported packet types from corrupt packets and expose only supported actions.

### A14 — P2: no expected-reboot save lifecycle

Configuration setters offer immediate changes without a save transaction/result state, restart guidance or reconciliation after reconnect. Security/Bluetooth and many module settings reboot the radio in the official workflows. Generic reconnect exists, but it does not prove the preceding configuration persisted. Add staged drafts, discard/apply, restart expectations and post-reconnect verification. This compounds A02.

## Feature comparison matrix

“Present” means a source implementation was found, not that a physical-radio test passed.

| Area | MeshCrossway 0.3.1 | Compared with official Android app |
|---|---|---|
| BLE transport | Present: service/characteristic checks, MTU, serialized GATT, notify/read queue draining, polling, keepalive, reconnect | Basic path present; physical multi-device/Android-version testing still needed |
| USB / TCP connections | Absent | Official supports USB and network connections |
| Initial radio synchronization | Present: nonce, my-info, metadata, LoRa, nodes, channels, early packets | No complete settings capability model; restrictive fixed bounds need compatibility tests |
| Ordinary channel text / DM | Present: UTF-8, local history, PKI when advertised, routing ACK handling | Core exists; admin ACK coverage must not be inferred from text ACK tests |
| Message actions | Present: copy/share/favorite, text mentions/quotes and reception report draft | Structured replies/reactions and complete error/priority controls missing |
| User identity | Partial: names, unmessageable, status, licensed flag | A04–A07/A09; hardware display and compatibility incomplete |
| LoRa | Partial: region, preset, coding override, TX, hop, power/frequency/MQTT flags | A01/A10/A11; manual mode and compatibility filters incomplete |
| Channels | Partial: existing names only | Add/remove/reorder/primary selection, key editing, sharing/QR/URL, position precision and MQTT options missing; A08 |
| Security | Partial: signature policy, serial/debug flags, admin-key count, managed status | Key generation/backup/restore, admin-key editing and capability gates missing |
| Device configuration | Partial: role and Bluetooth enable | Position, power, network, display and full Bluetooth editors missing; role guidance incomplete |
| Modules | Partial: MQTT enable and status | Broker settings/proxy and telemetry, serial, external notification, store-forward, range test, canned messages, audio, remote hardware, neighbor info, ambient light, detection sensor, PAX, traffic/TAK/beacon editors absent or version-dependent |
| Administration | Placeholder | Reboot/shutdown/reset, remote admin and other official actions missing for MT |
| Radio config backup/restore | Placeholder | Phone history/GPX backup is separate and does not implement Meshtastic radio-config import/export |
| Map / phone GPS / local tracks | Present as MeshCrossway features | Not equivalent to over-mesh position publication, received/shared waypoints, geofences or complete official map tools |
| Metrics | Partial: battery and packet reception information | Sensor dashboards/history, neighbor info, position log and traceroute missing |
| Notifications | Basic app notifications and reply support exist | Full official categories, filtering and delivery-state parity not established |
| Firmware update | Absent in MT transport | Official supports supported BLE/Wi-Fi/USB update workflows |
| Dual independent networks | Present, scoped sessions and histories | MeshCrossway-specific capability; must remain isolated during every change |

## What is sound in the reviewed implementation

- BLE UUIDs and the FromNum notification / FromRadio read-drain transport pattern agree with the Meshtastic interface design.
- The ToRadio packet envelope, ADMIN_APP=6, set_owner=32, set_channel=33, set_config=34, set_module_config=35 and session_passkey=101 align with official protobuf field assignments.
- Config and ModuleConfig oneof tags are mapped using the corresponding enum index + 1 for the implemented types.
- Config field mutation retains other parsed fields, including unknown fields of supported protobuf wire types, rather than reconstructing every config section from UI defaults.
- Normal text delivery distinguishes submission over BLE, radio acceptance and recipient acknowledgment. Pending messages are marked uncertain on disconnect rather than automatically resent.
- Session/history separation and the existing two-network fixtures are useful foundations.

These are source-review conclusions, not end-to-end radio certification.

## Verification evidence and limits

- Local Git hashes of the four key integration files exactly match Android 0.3.1's published tree.
- `node scripts/test_android_remote_web.cjs`: passed.
- `python3 scripts/test_android_migration.py`: passed.
- `bash scripts/test_android_probe.sh`: could not execute locally because `javac` is not installed. No local Java or Android build result is claimed.
- Existing release CI [42768644976](https://github.com/Renbrant/MeshCrossway/actions/runs/42768644976) succeeded on the exact reviewed commit: protocol/browser/migration checks, unit tests, release assembly/lint, permanent signature/version verification and publication.
- Existing ProtocolClientTest covers bootstrap, normal text, PKI, ACK ordering, duplicate suppression and network isolation. It does not exercise the new setMt* administrative methods, session expiry, firmware-gated controls or UI preset mappings; the green build cannot establish those flows are correct.
- No physical BLE/RF/hardware checks were available in this audit. Firmware-specific Ham transitions, settings persistence/reboots and signature behavior need controlled radio tests after the source defects are fixed.

## Recommended acceptance gates for the next settings release

1. Fix A01–A08 before adding more editable radio settings; add protocol-level regressions for their concrete failures.
2. Implement one shared state reducer for stream/GET/readback and firmware/hardware capability gates.
3. Validate presets/regions against official enum values, name/status UTF-8 byte limits, and numeric boundaries.
4. Exercise GET → edit → SET → success/error/timeout → readback, including session expiry and disconnect/reboot.
5. On a test radio: verify each changed value in the official app, reconnect, and confirm persistence. Cover a default unnamed primary channel, an all-default config, non-ASCII names/status, and at least supported 2.7/2.8 firmware branches.
6. Repeat dual BLE tests while MT settings change and MC receives messages, confirming no history/session crossing.

## Official references

All Android links below are pinned to the reviewed revision.

- [Radio & User settings](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/settings-radio-user.md)
- [Module settings & administration](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/settings-module-admin.md)
- [Connections](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/connections.md)
- [Messages & Channels](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/messages-and-channels.md)
- [Nodes](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/nodes.md)
- [Node metrics](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/node-metrics.md)
- [Map & Waypoints](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/map-and-waypoints.md)
- [Telemetry & Sensors](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/telemetry-and-sensors.md)
- [MQTT](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/mqtt.md)
- [Firmware updates](https://github.com/meshtastic/Meshtastic-Android/blob/4b557c67a731ecc0908de4db2619bae3a696e2f4/docs/en/user/firmware.md)
- [Protobuf definitions](https://github.com/meshtastic/protobufs/tree/4c80682b3b441a4cebab9f3e5c3097857cb5c81d/meshtastic)
