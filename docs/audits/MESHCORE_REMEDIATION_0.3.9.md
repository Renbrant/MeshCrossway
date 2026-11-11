# MeshCore integration corrections — Android 0.3.9

> Re-audit: see the [Android 0.3.9 integration re-audit](MESHCORE_INTEGRATION_AUDIT_0.3.9.md) for the current assessment, including three remaining cases.


Addresses MC01–MC10 from the [0.3.8 audit](MESHCORE_INTEGRATION_AUDIT_0.3.8.md). The historical audit, opcode inventory and its pinned failing probe remain unchanged. This release adds a separate regression suite against the corrected production implementation.

## Correction matrix

| Finding | Implemented behavior | Regression coverage |
| --- | --- | --- |
| MC01 | Serialized settings transaction; fresh readback and radio identity verification before success. A normal ERR reports failure without closing GATT. Interrupted transactions verify without resending SET commands. Partial failure refreshes actual settings; repeated recovery failure pauses for an explicit choice. Reject nonfinite values. | Accepted/rejected/mismatched writes; partial batch failure; recovery, timeout, discard and finite bounds. |
| MC02 | Parse the device's repeat-mode flag and preserve it in radio SET. A combined preset/path edit is serialized and checked as one transaction. | Repeat enabled/disabled, readback drift and combined radio/path change. |
| MC03 | Separate discovered, approved and retired contacts. Explicit approval/removal is checked against fresh contact reads. Reconcile deletion, path updates and full refreshes; show contact-capacity errors. Keep history/favorites and prevent sending to unapproved contacts. | Discovery approval/removal, deleted/full-refresh contacts, capacity warning, metadata persistence and send eligibility. |
| MC04 | Validate names against 31 UTF-8 bytes. Display an existing malformed name with replacement characters and offer repair instead of rejecting the identity frame. | ASCII/UTF-8 limits and legacy truncated-name recovery. |
| MC05 | Decode signed TX power; use the advertised maximum and preserve valid negative values in the editor. | Negative power, advertised higher maximum and invalid bounds; Android settings fixture. |
| MC06 | Gate stats at Companion protocol 8, path hash at 10 and default scope at 11. Coalesce reads and suppress automatic retries after errors until explicit refresh. | Older firmware opens settings without unsupported requests; repeated read/error/manual refresh checks. |
| MC07 | Preserve advert, receiver modification and local receipt separately. Refresh known contacts on DM reception; live DMs advance local activity. Initial backlog does not make every contact freshly heard. Home labels the combined measure as activity/update rather than exact RF visibility. | Receiver timestamps, live DM activity, initial backlog and persisted metadata. |
| MC08 | Keep bounded late-ACK correlation for ten minutes, at most 128 retained entries. Restore it across reconnection to the same radio while the service lives. Confirm only a unique matching ACK, including hash zero. | Late ACK, BLE reconnection and zero hash. |
| MC09 | Retain the raw extra-ACK integer when changing unrelated privacy/contact settings. An explicit enable/disable change remains intentional. | Count three survives unrelated edits; explicit toggle behavior. |
| MC10 | Skip unsupported binary channel payloads and immediately continue draining until no more messages. | Text following response 27 drains without the idle polling delay. |

## Validation

- `bash scripts/test_android_probe.sh`: 66 MeshCore regression checks, 206 Meshtastic conformance checks and existing protocol/field suites.
- `node scripts/test_android_remote_web.cjs`: remote UI regression checks.
- `python3 scripts/test_android_migration.py`: migration checks.
- Gradle build, lint and unit tests; Android API 29/35 emulator fixtures cover settings capability gates, TX power/name/pending-save presentation and contact metadata/history persistence.
- Permanent release signing certificate and increasing versionCode 29 are checked before publication. Install as an update; do not uninstall or clear data.

Core tests live in `McIntegrationTest.java` and its Gradle JUnit wrapper. Android UI/persistence checks live in `SmokeInstrumentation.java`. These checks exercise production parsers and state transitions; they do not establish RF performance on physical hardware.

## Reference and limits

Protocol expectations follow the pinned [official Companion source](https://github.com/meshcore-dev/MeshCore/tree/81dbae592ffad40b4212bc0b37e82b38da08f4bc). The [Companion 1.15.0 protocol header](https://github.com/meshcore-dev/MeshCore/blob/dee3e26ac081a5c668c69b66c16a6544a44ddc5b/examples/companion_radio/MyMesh.h) identifies protocol 11 for the scope gate. The official native application is closed source; this is a protocol/behavior comparison, not a source audit of that application.

BLE recovery preserves pending verification and late delivery correlation only while the Android service remains alive and reconnects to the same radio. No automatic message retransmission or automatic replay of settings writes is introduced. Receiver modification timestamps can also reflect edits, so activity/update is not a promise that a node is currently reachable. Physical-radio testing remains necessary. The advanced editors, remote repeater administration and other feature gaps documented in the original audit are separate work; this release does not claim full official-app parity.
