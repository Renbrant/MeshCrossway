# MeshCore integration audit — Android 0.3.8

Date: 2026-11-10. Result: **basic interoperability works, but the integration is partial and has ten confirmed defect groups**. Four groups are P1 and six are P2 under the priorities defined below. This is an audit of the released application, not a new APK or a remediation release.

## Scope and reference versions

Audited MeshCrossway Android [`android-v0.3.8`, `dcc0a56`](https://github.com/Renbrant/MeshCrossway/tree/dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23). Reviewed the native Companion protocol, BLE service lifecycle, commands, identity/contact/channel synchronization, settings, messaging/ACKs, metadata, persistence, user-facing controls and available feature coverage.

The official comparison is Liam Cottle's **MeshCore Companion App**, not MeshCore Open. The official project's [FAQ](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/faq.md) explicitly identifies the native apps as closed source. Consequently this audit compares documented official-app behavior and public protocol implementations; it does not claim a source-code audit of the closed native app, a side-by-side installed-app test, or UI parity established by running every official screen.

References:

| Reference | Pinned version / evidence |
| --- | --- |
| Official application | [Publisher changelog](https://app.meshcore.nz/assets/CHANGELOG.md), headed **1.51.1, 2026-11-09**, and [publisher download directory](https://files.liamcottle.net/MeshCore/). This identifies the published app reference, not the version currently delivered to every Play Store region. |
| Released Companion firmware | [`companion-v1.18.0`, `81dbae592ffad40b4212bc0b37e82b38da08f4bc`](https://github.com/meshcore-dev/MeshCore/tree/81dbae592ffad40b4212bc0b37e82b38da08f4bc), released 2026-10-15. Main/development features are not assumed available on this release. |
| Official JavaScript library | [`@liamcottle/meshcore.js` 1.15.0, `26193c0`](https://github.com/meshcore-dev/meshcore.js/tree/26193c059fcd1d0524f1ad5b19fd88287ecc5c61). Supplemental reference; its API is not automatically equivalent to every native-app screen. |
| Official Python library | [`meshcore` 2.3.18, `c5ffc38`](https://github.com/meshcore-dev/meshcore_py/tree/c5ffc383730db311ac91b5ef5067e2808584ef92). Supplemental command/decoder reference. |
| Protocol documentation | [Companion protocol at the firmware pin](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/companion_protocol.md). Its own accuracy caveat makes firmware handlers the final reference for frame layouts. |

No physical radio, RF delivery test, Android pairing experiment or network range measurement was performed. The JVM fixtures exercise released production classes without transmitting anything. Meshtastic corrections are not re-audited here; shared network-isolation/persistence behavior is included where relevant to MeshCore.

## Confirmed defects

P1: correct before expanding or presenting the affected workflow as reliable. P2: a functional defect with narrower impact. A failing fixture is evidence for a particular boundary case, not a claim that all normal messages or all radios fail.

| ID | Priority | Finding | Reproduction and impact |
| --- | --- | --- | --- |
| MC01 | P1 | Settings mutate the displayed model before confirmation; ordinary settings errors terminate the connection. | `setName`, `setRadio`, location/power/other-parameter/scope setters change local values immediately after queueing. A normal `ERR` for a non-message command throws; `RadioService.received` then closes GATT. The fixture sees 22 dBm after a radio advertising a 20 dBm maximum rejects it. Nonfinite radio/location inputs are also accepted at the setter boundary and converted into invalid/zero commands. There is no MeshCore settings transaction/readback equivalent to the Meshtastic flow. |
| MC02 | P1 | Saving radio parameters silently disables native Companion repeat mode. | DEVICE_INFO byte 80 can report repeat enabled, but MeshCrossway ignores it. `setRadio` always sends byte 11 as zero; the released firmware applies that byte with `setRepeatEn(false)`. This changes a setting the radio editor did not offer to change. Simply omitting the optional byte is also insufficient: the audited firmware defaults it to zero. |
| MC03 | P1 | Discovered, approved and deleted contacts are conflated. | Firmware can emit NEW_ADVERT (0x8A) for a discovery that was **not added** because manual approval/type/hop limits/storage policy prevented it. MeshCrossway promotes it directly to the active contact map and allows a send that the radio can reject as not found. It ignores CONTACT_DELETED (0x8F), CONTACTS_FULL (0x90) and PATH_UPDATED (0x81); a full refresh returning zero contacts also leaves old entries active. There is no add/approve command/UI. Historical records should be retained separately from currently usable radio contacts. |
| MC04 | P1 | Names allow 32 bytes where the firmware stores 31. | A 32-byte ASCII name is silently shortened. A 32-byte UTF-8 name such as sixteen `é` characters is accepted by the app, then the firmware can truncate midway through a code point. MeshCrossway's strict SELF_INFO decoding rejects the resulting identity frame and the service reconnects. The Android character-count filter does not enforce the correct UTF-8 budget. |
| MC05 | P2 | TX power is decoded unsigned and the editor has a fixed ceiling. | SELF_INFO TX power -5 dBm becomes 251. The editor initializes negative values as zero, labels negatives as unknown, and restricts values to -9…22 regardless of the radio's advertised maximum. A device advertising 30 dBm cannot select a supported 27 dBm value through this UI. Bounds must come from modem/firmware capability, with signed decoding. |
| MC06 | P2 | Newer commands are offered without the firmware gates used elsewhere. | Bootstrap accepts protocol v3, while the stats page unconditionally sends v8+ GET_STATS; default-scope commands are also offered to old protocols. Opening a page auto-queues these reads. A valid unsupported-command error takes the MC01 disconnect path; returning to the same auto-fetching screen can repeat the failure after a successful reconnect. Path-hash editing already has a gate, so gating is inconsistent rather than universally absent. |
| MC07 | P2 | “Heard recently” uses advert creation time and ignores received-DM activity. | Contacts contain advert timestamp at byte 132 and receiver-maintained `lastmod` at byte 144. The app maps only the former to `heard`. A fixture where a DM advanced receiver activity but the last advert is old is shown as old; receiving a subsequent DM also leaves `heard` unchanged. Track local receive time and keep advert/last-modified/activity concepts separate; `lastmod` itself can also reflect edits and must not blindly become an exact RF timestamp. |
| MC08 | P2 | A valid late delivery ACK cannot reconcile a message. | After the firmware's estimated timeout, `next()` marks the message uncertain and deletes its ACK correlation entry. A later valid SEND_CONFIRMED frame matching the sent hash is ignored. Keep bounded correlation state long enough to reconcile confirmed late delivery; this does not require enabling automatic retransmission. |
| MC09 | P2 | Unrelated settings edits reduce the extra-ACK count to a boolean. | SELF_INFO carries the firmware's extra-ACK **count**. A count of three becomes `true`; changing location privacy sends one in SET_OTHER_PARAMS byte 4. The protocol and official Python setter preserve an integer. The editor may expose an enable/disable choice, but unrelated edits must preserve the raw count. |
| MC10 | P2 | Consuming unsupported channel data pauses backlog draining. | SYNC_NEXT_MESSAGE recognizes CHANNEL_DATA_RECV (27) as a completed response, but does not process it or re-arm the waiting flag. Text queued behind it waits for the next idle poll, up to 20 seconds per such item. Skip unsupported payloads explicitly while continuing to drain until NO_MORE_MESSAGES, or implement a supported data handler. |

### Source anchors

MeshCrossway links are pinned to the audited release:

- [ProtocolClient.java](https://github.com/Renbrant/MeshCrossway/blob/dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java): `next`/`Sent` for MC08; `mcContact`/`meshcore` for MC03, MC05–MC07 and MC10; `setName`/`setRadio`/`setOtherParams` and other setters for MC01, MC02, MC04 and MC09.
- [RadioService.java](https://github.com/Renbrant/MeshCrossway/blob/dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23/android/app/src/main/java/com/meshcrossway/android/RadioService.java): `received` closes the connection on `IllegalArgumentException`; `release` preserves pending administrative transactions only for Meshtastic; callbacks and each session maintain independent GATT/command state.
- [MainActivity.java](https://github.com/Renbrant/MeshCrossway/blob/dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23/android/app/src/main/java/com/meshcrossway/android/MainActivity.java): `editMcName`, `editMcPower`, `meshCoreStats`, `meshCoreTelemetry`, `meshCoreRegions`, `meshCorePathSettings` and privacy/contact editors establish the corresponding UI behavior.
- [MeshData.java](https://github.com/Renbrant/MeshCrossway/blob/dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23/android/app/src/main/java/com/meshcrossway/android/MeshData.java): `Node` lacks separate discovery/approval, path, advert and receiver activity fields.

Official sources establishing the expected meanings:

- [MyMesh.cpp](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/MyMesh.cpp): DEVICE_QUERY/APP_START layouts; settings handlers; contact push codes and `writeContactRespFrame`; `processAck`; scope and statistics formats.
- [BaseChatMesh.cpp](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/src/helpers/BaseChatMesh.cpp): `onAdvertRecv` can announce an unapproved discovery without inserting it; incoming messages advance `lastmod`.
- [NodePrefs.h](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/NodePrefs.h): persisted parameter semantics, integer extra acknowledgements and location/telemetry policy values.
- [Python device commands](https://github.com/meshcore-dev/meshcore_py/blob/c5ffc383730db311ac91b5ef5067e2808584ef92/src/meshcore/commands/device.py): parameter setters, including integer `set_multi_acks`.
- [JavaScript decoder](https://github.com/meshcore-dev/meshcore.js/blob/26193c059fcd1d0524f1ad5b19fd88287ecc5c61/src/connection/connection.js): complementary response parsing, particularly signed radio statistics.

## Functional comparison with the official app

This matrix separates confirmed defects from absent features and shared phone functionality. Official-app presence is based on its publisher documentation/changelog; low-level implementation requirements are established by firmware/library source. Native-app algorithms not exposed publicly are not inferred.

| Area | MeshCrossway 0.3.8 | Assessment |
| --- | --- | --- |
| Companion BLE | NUS UUIDs, service validation, notifications, negotiated MTU and serialized operations | Present; real pairing/background behavior still needs devices. |
| Parallel networks | Independent MT and MC sessions, addresses, queues, histories and reconnect state | Present; preserves the project's separate-network architecture. |
| USB and Wi-Fi/TCP | No MeshCore transports besides BLE | Missing transport parity. The phone's local HTTPS workspace is a separate feature. |
| Initial synchronization | DEVICE_QUERY → APP_START → contacts → channels → message backlog | Basic interoperability works. Protocol documentation is not used as a strict command-order oracle where firmware supports negotiation first. |
| Time | Manual Sync Time command | Partial; automatic radio-clock synchronization/readback is absent. |
| Presence advertisement | No SEND_SELF_ADVERT command/action | Missing core onboarding/discovery action documented by the official app. |
| Discovery and contact approval | Advert reception and public-key lookup, with one shared contact model | Partial and incorrect in MC03; no separate approval/discovery workflow. |
| Contact management | Name/key/type/position summaries, favorites and filters | Partial; radio add/remove/import/export, permission flags, current path and full sensor-type controls are absent. |
| Channel synchronization | Reads existing channel name/key/slot and derives a key-based identity | Present for existing channels. |
| Channel editing and sharing | No SET_CHANNEL or channel create/join/key/QR/share workflow | Missing parity; local history does not replace radio channel configuration. |
| Direct text | Send/receive with six-byte recipient prefix, ACK hash and timeout | Basic operation present; late delivery fails MC08. |
| Direct/flood attempts | One submitted attempt; no official-app-style configurable direct/flood retry policy | Feature gap, not proof of an incorrect on-air packet. Automatic retransmission needs an explicit product decision. |
| Public/private channel text | Plain text, sender-prefix display handling and persistent conversations | Basic operation present; radio acceptance is correctly distinct from remote delivery. |
| Replies and quick messages | Shared compose/mention, favorites and quick-reply tools | Partial shared-app coverage, not a claim of all official composer actions. |
| Rooms and repeaters | Contact types can be displayed; room signed text can be received as a direct conversation | Partial receive path only. No login/post/admin/session workflow. Signed room-author prefix is not exposed as a separate author identity. |
| Radio frequency/BW/SF/CR | Manual editor and online suggested presets | Present with MC01/MC02; presets require internet and have no local cache. Live preset-service availability was not validated in this audit. |
| Repeat mode and frequency permission | Not retained/offered; no allowed-repeat-frequency query | Missing control and unintended write in MC02. |
| TX power | Read/edit | Incorrect in MC05. |
| Path-hash size | Own-node preference with protocol gate | Partial; contact path viewing/reset/discovery/trace tools are absent. |
| Privacy/telemetry permissions | Advert location and location/environment permission bits | Partial; base telemetry control and full contact permissions are absent; MC09 affects preservation. |
| Contact auto-add policy | Manual on/off | Partial; per-type rules, max hops and storage-overwrite policy are absent. |
| Local statistics | Battery/storage, core uptime and radio signal/airtime | Basic formats correct on supported firmware; MC06; packet counters/error/queue fields are not fully exposed. |
| Remote telemetry | No request/login/decoder workflow for remote sensor/status responses | Missing parity; local battery statistics are not remote-node telemetry. |
| Regions | Default scope name read; name/key set and clear | Partial; no repeater region tree, discovery, per-channel overrides or radio configuration sharing. |
| BLE PIN/security administration | OS pairing and existing identity/public key display | Partial; no radio PIN change, identity backup/restore or complete radio configuration backup. |
| Companion CLI/Wi-Fi configuration | No CLI command or Wi-Fi editor | Missing documented official-app features. |
| RX metadata | Legacy/V3 text, SNR/hop metadata, authenticated group-log route enrichment | Present within bounds; direct paths are not mislabeled as complete travelled routes. |
| Raw/binary data | Group logs used for metadata; no general datagram/raw UI | Partial; MC10 affects draining. |
| Maps and tracks | Shared offline maps, contact markers, phone GPS, POIs and GPX tracks | Present as phone features; this is not verification of the official app's repeater trace/coverage tools. |
| Local persistence | Scoped SQLite histories, drafts, favorites, replay dedup and complete phone backup | Present; phone backup does not export the radio's complete identity/channel/permission configuration. |
| Notifications/filtering | Shared app alerts and conversation preferences | Partial; the “Message filter” row routes to notification preferences, not a full MeshCore sender-blocking/filter editor. |
| Reset/reboot | Reboot command | Partial; factory reset is absent. Firmware flashing is outside this Companion integration audit. |

## Protocol coverage inventory

The pinned released firmware defines **59 Companion request opcodes**. MeshCrossway emits **20** of them; the remaining **39** have no implemented request path. Some emitted opcodes are only partially supported. This is **not** a percentage score for correctness or native-app parity: the official app need not expose every raw/internal command.

The machine-readable [command inventory](MESHCORE_COMMAND_COVERAGE_0.3.8.json) lists every opcode and the audit classification. Implemented request paths are:

`APP_START`, `DEVICE_QUERY`, `GET_CONTACTS`, `GET_CONTACT_BY_KEY`, `GET_CHANNEL`, `SYNC_NEXT_MESSAGE`, direct/channel text send, advert name/location setters, radio/TX-power setters, `SET_OTHER_PARAMS`, `SET_DEVICE_TIME`, battery/storage, core/radio stats, own path-hash mode, default scope get/set and reboot.

Missing high-value command families include self advertisement, approving/updating/removing contacts, channel editing, radio PIN/configuration backup, path discovery/reset/trace, remote login/status/telemetry/CLI and detailed auto-add policy. Treat raw-packet/signing/private-key operations according to product requirements rather than adding them merely to increase command coverage.

## Validation and retained correct behavior

Existing `bash scripts/test_android_probe.sh` passes, including its MeshCore fixtures, metadata tests, network isolation checks and all 206 Meshtastic conformance checks. Those tests did not cover the MeshCore boundaries found here.

New audit fixture: **28 checks: 9 pass, 19 fail**, grouped into the ten findings above. The passing truncated-UTF-8 check proves the decoder rejects the malformed identity produced by firmware-style truncation; it does not make accepting the original 32-byte name correct. The timestamp fixture models `lastmod` advanced by a received message, not by a local edit.

Positive evidence includes successful basic handshake/contact/channel synchronization, correct NUS identifiers and MTU budget, correct group-send frame offsets, signed radio-statistics decoding, scope-name decoding and rejection of ambiguous outgoing recipient prefixes. Existing tests additionally verify on-time ACKs, backlog dedup, MAC-checked raw-log enrichment and separation from Meshtastic history.

No current test measures BLE recovery on physical ESP32/nRF52 radios, range, RF timing or the closed native app's actual UI behavior.

Reproduce from a full clone containing the audited commit, with a JDK on PATH:

```sh
bash scripts/test_android_probe.sh
python3 docs/audits/run_meshcore_038_probe.py
```

The second command extracts the pinned production source into a temporary directory and intentionally exits **1** for the audited version. It is an audit diagnostic, not a new failing release-CI job. It does not modify production code or transmit commands.

- [Audit fixture](fixtures/MeshCore038Probe.java)
- [Pinned-source runner](run_meshcore_038_probe.py)
- [Recorded output](evidence/MESHCORE_0.3.8_PROBE.txt)
- [Complete opcode inventory](MESHCORE_COMMAND_COVERAGE_0.3.8.json)

## Recommended correction and device acceptance order

1. Add explicit MeshCore settings transactions, per-command errors and readback; preserve repeat state and raw unrelated parameters. A rejected setting must not masquerade as an applied value or require a GATT reconnect.
2. Separate discovered/approved/historical contacts, implement approval and deletion reconciliation, and enforce 31 UTF-8 bytes before sending names.
3. Correct signed/dynamic power limits, firmware gates, receiver activity tracking, late ACK retention and backlog draining.
4. Add self advertisement and channel/contact management before claiming complete everyday setup parity. Expand remote management and other advanced tools in later, separately validated work.
5. On physical radios, test repeat on/off during radio changes, rejected saves, accented names, negative/high power, manual contact approval, full storage/overwrite, old firmware, late ACKs, backlog interleaving, loss/reconnect and app restart. Keep Meshtastic connected throughout and verify histories remain separate. Confirm radio values using the official app on the same hardware.

The audit and reproducible evidence are documentation-only. Android 0.3.8 and the website's download version remain unchanged by this audit.
