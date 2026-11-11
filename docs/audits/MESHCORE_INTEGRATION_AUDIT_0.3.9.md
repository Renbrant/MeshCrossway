# MeshCore integration re-audit — Android 0.3.9

Audit date: **2026-11-10 (America/Denver)**. Result: **substantial improvement, with three remaining defect cases and incomplete official-app feature coverage**. Eight original defect groups are addressed within the reviewed boundaries; MC01 and MC03 remain partially addressed. The earlier statement that all ten groups were fully corrected was too broad.

This audit changes documentation and audit-only fixtures. It does not publish another APK or alter Android 0.3.9.

## Exact scope and references

- Released MeshCrossway: [`android-v0.3.9`, `527c496e49f67462d6e82ccc0f5b112cfa40aa6e`](https://github.com/Renbrant/MeshCrossway/tree/527c496e49f67462d6e82ccc0f5b112cfa40aa6e), versionCode 29. Source was checked out at the immutable release tag, rather than the website branch or an uncommitted working copy.
- Released official firmware: [`companion-v1.18.0`, `81dbae592ffad40b4212bc0b37e82b38da08f4bc`](https://github.com/meshcore-dev/MeshCore/tree/81dbae592ffad40b4212bc0b37e82b38da08f4bc). GitHub's latest release was rechecked and still identifies this release. Development-only firmware behavior is excluded.
- Official native app reference: publisher [changelog](https://app.meshcore.nz/assets/CHANGELOG.md), still headed **v1.51.1, 9 November 2026** when rechecked. The app is closed source, as identified in the [official FAQ](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/faq.md). This is a source/protocol and documented-behavior comparison, not an audit of its private code or a side-by-side installed-app session. Changelog features requiring Companion 1.19+ are not assumed supported by 1.18.
- Reviewed the production request/response parser, BLE lifecycle, settings transactions, reconnect state, contacts/discovery, channels, direct/group messaging, ACK correlation, metadata, persistence, settings/navigation controls, command coverage and network separation.

Existing regression suites were rerun locally. Newly added audit probes exercise published Java production classes. Prior API 29/35 release emulator and signing-job statuses were rechecked; **the emulators were not rerun during this documentation-only re-audit**. No physical radio, pairing experiment, RF delivery/range measurement or official native-app UI session was performed.

## Remaining defects

P1 means a core workflow should be corrected before treating it as reliable. P2 means a functional defect with a narrower trigger. These are specific reproducible boundaries, not a claim that all ordinary sessions fail.

| ID | Priority | Remaining behavior | Evidence and consequence |
| --- | --- | --- | --- |
| R01 | P1 | A newly auto-added radio contact is treated as requiring manual approval. | Official `NEW_ADVERT` / 0x8A is emitted both after successful auto-add and for discoveries not inserted into the radio contact table. For a key absent from the app's map, 0.3.9 always records `MC_DISCOVERED` and queues no membership lookup. This also happens with manual approval off. The app blocks direct sending to a contact the radio already approved until a full refresh, another resolving event, or an unnecessary explicit approval. |
| R02 | P2 | A contact lookup returning NOT_FOUND is negatively cached for the session, blocking recovery after a new advert. | Ready-state `ERR NOT_FOUND` retires the contact and adds the GET_CONTACT_BY_KEY request to `mcFailedReads`. A later 0x80 advert schedules the same lookup, but `mcRead` silently refuses it. The peer can be valid again in the radio while the app remains stale. Explicitly clearing failed reads, a full contact refresh or reconnection can recover; ordinary lifecycle events do not invalidate this negative cache. |
| R03 | P2 | A possibly transmitted settings write loses pending verification if BLE disconnects before the write callback/response. | `pump` can hand a SET to `BluetoothGatt.writeCharacteristic` without marking the transaction as possibly dispatched. The marker is set only by a successful write callback or SET response. `release` retains only marked edits. A disconnect in that interval drops the edit, so reconnect cannot verify whether the radio applied it. The audit probe demonstrates the missing marker/retention predicate; source review establishes the GATT handoff path. It does not measure how frequently this callback ordering occurs on a phone, or prove that a particular physical radio applied the SET. |

R01 and R02 are separate failure cases in the same MC03 contact lifecycle group. R03 is the remaining MC01 reconnect boundary. Neither requires automatic retransmission: resolve fresh contact membership and retain uncertain settings for GET-only verification.

### Source anchors and reproduction

All Android anchors below point to the exact released commit:

- [ProtocolClient.java, lines 199–201](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java#L199): unknown 0x8A keys are classified as discoveries without GET_CONTACT_BY_KEY (R01).
- [ProtocolClient.java, line 187](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java#L187) and [read guard, line 255](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java#L255): contact NOT_FOUND enters the same failed-read guard as unsupported feature reads (R02).
- [RadioService.java, line 304](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/RadioService.java#L304), [GATT pump, line 312](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/RadioService.java#L312), [write callback, line 411](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/RadioService.java#L411) and [ProtocolClient.written, line 53](https://github.com/Renbrant/MeshCrossway/blob/527c496e49f67462d6e82ccc0f5b112cfa40aa6e/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java#L53): handoff, marking and retention order (R03).
- Official [BaseChatMesh.cpp](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/src/helpers/BaseChatMesh.cpp#L155) and [MyMesh::onDiscoveredContact](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/MyMesh.cpp#L358): successful auto-add and non-added discovery both reach the new-advert push; a fresh contact read distinguishes them.

Run from a full clone containing the release commit, with a JDK on PATH:

```sh
python3 docs/audits/run_meshcore_039_probe.py
```

Result: **22 checks: 19 pass, 3 fail**. The three failing expectations correspond to R01, R02 and R03. Exit **1** is intentional for this audit diagnostic. It is not installed as a failing release-CI job and transmits nothing.

- [Audit fixture](fixtures/MeshCore039Probe.java)
- [Pinned-source runner](run_meshcore_039_probe.py)
- [Recorded probe output](evidence/MESHCORE_0.3.9_PROBE.txt)
- [Validation log and existing CI references](evidence/MESHCORE_0.3.9_VALIDATION.txt)

## Re-evaluation of all ten original groups

“Addressed” below is bounded by source review and regression evidence, not a physical-radio certification.

| Original group | 0.3.9 status | Verified outcome / limit |
| --- | --- | --- |
| MC01 — settings, ERR and readback | **Partial** | Ordinary ERR remains local, no optimistic cache mutation, matching readback is required, and interrupted marked writes use GET-only recovery. R03 leaves a pre-callback gap. |
| MC02 — repeat mode | **Addressed** | Both repeat states are preserved in radio SET; compound preset/path saves serialize and compare actual repeat/path values. A repeat toggle and allowed-frequency editor remain absent features. |
| MC03 — contact lifecycle | **Partial** | Blind approval removed; explicit add/remove, deletion/full-refresh reconciliation and historical retention work in covered cases. R01/R02 prevent complete automatic synchronization. |
| MC04 — UTF-8 names | **Addressed** | Rejects over 31 UTF-8 bytes; existing truncated names remain readable and repairable. |
| MC05 — TX power | **Addressed** | Signed decode and device-advertised maximum; valid negative and above-22 dBm values are supported when advertised. |
| MC06 — capability gates | **Addressed for reviewed commands** | Stats, path and scope gates and read coalescing/error suppression prevent the original unsupported-screen retry loop. Contact NOT_FOUND must not share a permanent feature-failure cache (R02). |
| MC07 — activity timestamps | **Addressed with explicit semantics** | Receiver modification and live local DM receipt replace sender advert creation as the recency proxy. Initial backlog does not mark all contacts freshly heard. Activity/update is not proof of current RF reachability. |
| MC08 — late ACKs | **Addressed within retention limits** | Unique matching ACKs reconcile after estimated timeout, including same-radio BLE reconnection. Bounded to 128 retained entries, ten minutes and service lifetime; process-restart durability is not claimed. |
| MC09 — integer ACK count | **Addressed** | Unrelated edits retain the raw count; explicit enable/disable remains intentional. |
| MC10 — binary backlog | **Addressed** | Response 27 is skipped while immediate draining continues. General binary payload support is still absent. |

The original [0.3.8 audit](MESHCORE_INTEGRATION_AUDIT_0.3.8.md) and [0.3.9 correction report](MESHCORE_REMEDIATION_0.3.9.md) are retained as historical records; this re-audit supersedes any implication of complete closure.

## Integration coverage compared with the official ecosystem

This matrix distinguishes working implementation, functional defects and absent features. Missing advanced functions are not counted as bugs merely because a firmware opcode exists. Native-app comparisons use published behavior; the firmware establishes packet semantics.

| Area | 0.3.9 implementation | Remaining limitation |
| --- | --- | --- |
| BLE transport | NUS service/characteristics, subscription, negotiated MTU, safe packet budget, serialized GATT operations | Physical pairing/background behavior remains untested here; R03 affects uncertain SET recovery. |
| Dual networks | Independent MT/MC sessions, queues, address-scoped histories and reconnect state | No bridge/relay introduced. Physical simultaneous-link acceptance remains necessary. |
| USB / TCP | MC BLE only | No alternate MC transport parity. Phone-hosted HTTPS is a different feature. |
| Bootstrap | Device → self → contacts → channels → backlog | Basic fixtures pass, including response correlation and unsupported bootstrap failures. |
| Radio settings | Frequency/BW/SF/CR, suggested presets, power, path hash, location | Presets require internet; no local preset cache or complete modem-capability editor. |
| Repeat mode | Preserved and verified during radio edits | No toggle or allowed-repeat-frequency query/editor. |
| Identity and clock | UTF-8 name validation/repair; manual clock SET with GET verification | No automatic clock sync, identity/private-key backup/import or complete radio config backup. |
| Self advertisement | Advert reception | No SEND_SELF_ADVERT action; this remains a high-value everyday setup gap. |
| Discovery and contacts | Separate states, manual approval/removal, full refresh/deletion reconciliation, favorites/type filters including sensors | R01/R02; no full permissions, import/export/share or detailed auto-add policy editor. |
| Contact activity | Advert, receiver modification and local receipt retained separately | Update proxy includes edits; group sender names are not used to invent contact activity. |
| Channels | Existing name/key/slot synchronization; key-based conversation identity | No channel create/join/edit/key/QR/share workflow. |
| Direct text | Unique-recipient guard, text send/receive, radio acceptance vs delivery, bounded late-ACK reconciliation | One submitted attempt; no official-style configurable direct/flood retry policy. |
| Channel text | Existing channel send/receive and persistent conversations | Radio acceptance is not remote delivery; binary content is not rendered. |
| Replies / quick messages | Shared mention, favorites and quick-reply tools | Does not claim full native composer feature parity. |
| Rooms / repeaters | Contact-type display and received room text | No room login/post/session/admin flow; no separate room-author identity presentation. |
| Privacy / telemetry | Location/environment policy and extra-ACK controls with raw-value preservation | Base permission editor, contact permission flags and remote telemetry workflows remain incomplete. |
| Statistics | Battery/storage, core uptime, signed signal/noise and airtime on gated firmware | No full packet/error/queue statistics UI. |
| Paths / regions | Own path-hash preference and default scope name/key/clear | No contact path reset/discovery/trace, repeater region tree or per-channel scope workflow. |
| Remote administration | Reboot and existing OS pairing | No remote login/status/CLI, local Companion CLI, radio PIN/Wi-Fi editor or factory reset. |
| RX metadata | Legacy/V3 text, signal/hops and authenticated group-log route enrichment | No complete travelled DM path or arbitrary raw-data tools claimed. |
| Phone maps / tracks | Shared maps, offline areas, GPS, POIs, GPX | Separate phone features; not verification of official repeater tracing/coverage tools. |
| Persistence | Scoped history/drafts/favorites/backup and new MC lifecycle metadata | Radio config export is not replaced by a phone backup. Pending save/ACK recovery is not durable across process death. |
| Alerts / filtering | Shared notifications and conversation preferences | The Message filter route is not a full MC blocked-sender editor. |

## Command coverage

The pinned firmware defines **59 request opcodes**. Production 0.3.9 emits **23**, up from 20 in 0.3.8: the additions are GET_DEVICE_TIME (5), ADD_UPDATE_CONTACT (9) and REMOVE_CONTACT (15). **36 request opcodes remain without an implemented request path.**

The [complete inventory](MESHCORE_COMMAND_COVERAGE_0.3.9.json) was checked against all pinned firmware CMD definitions. This is an inventory, **not a percentage score for correctness or official-app parity**. Some emitted requests provide only partial feature coverage, and the native app need not expose every internal opcode.

## Validation and acceptance judgment

Locally rerun against exact released production source:

- Existing protocol/field/metadata suites: pass.
- Existing MeshCore regression suite: **66 checks pass**.
- Meshtastic conformance: **206 checks pass**.
- Browser chats/map selector and backup migration fixtures: pass.
- Additional re-audit probe: **19 pass, 3 fail**; its new cases expose R01/R02/R03 rather than contradicting the narrower passing cases in the previous suite.

Release checks re-read from GitHub: [PR build/lint/API 29/API 35 smoke](https://github.com/Renbrant/MeshCrossway/actions/runs/42917644014) and [permanent signing/API 29/API 35 storage gates](https://github.com/Renbrant/MeshCrossway/actions/runs/42918486090) succeeded. These are prior release evidence, not fresh emulator runs for this audit.

**Judgment:** 0.3.9 improves basic settings and messaging reliability materially. It should not yet be described as a fully closed contact/reconnect implementation or equivalent to the official app. Correct the three remaining cases before a reliability claim, then prioritize self advertisement and channel setup for everyday usability.

Acceptance steps after correction:

1. Verify both auto-add on/off, type/hop policy exclusions and storage-full behavior by reading actual membership after new adverts. Re-add a contact after NOT_FOUND without restarting the app.
2. Disconnect after GATT accepts a SET but before its write callback/response; reconnect to the same radio and perform GET-only verification. Never replay a SET automatically. Also exercise normal rejection and partial batch failure.
3. On physical supported old/current ESP32/nRF52 firmware, confirm repeat, names, signed power, scopes and privacy against the official app, including a reboot after a confirmed save.
4. Verify late ACKs, initial backlog, process death, approval/removal persistence and full contact refresh while the Meshtastic link stays active. Document radio models/firmware and phone/Android versions.

Additional source-review hardening item: most MC setting dialogs resolve the current session at Save time, whereas the name editor also checks the original client instance. Consider binding every editor to its original radio/session and dismissing stale dialogs. This was not counted as a reproduced wrong-radio defect: a modal editor blocks normal in-app device selection, and no actual target-switch UI scenario was exercised here.
