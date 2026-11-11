# MeshCore contact and reconnect corrections — Android 0.3.10

This release prioritizes R01–R03 from the [0.3.9 re-audit](MESHCORE_INTEGRATION_AUDIT_0.3.9.md). It does not add the separate advanced feature gaps such as channel configuration, self-advertisement or remote repeater administration.

| Finding | Correction | Regression evidence |
| --- | --- | --- |
| R01 — auto-added contacts classified as pending | After NEW_ADVERT, request actual membership with GET_CONTACT_BY_KEY regardless of manual/automatic policy. Successful lookup promotes the contact; NOT_FOUND leaves a discovery pending. No automatic ADD is sent. | Auto-add on/off, actual membership, send eligibility, excluded/unapproved discovery, repeated advert coalescing and interleaving with settings verification. |
| R02 — NOT_FOUND blocks future contact reads | Treat NOT_FOUND as a normal contact lifecycle result rather than an unsupported-feature failure. A fresh advert invalidates contact-specific read failures; successful lookup clears its own stale error. Preserve a newer advert that arrived during an older lookup. | Remove/re-add within one connection; fresh activity after NOT_FOUND and other read errors; no busy retry loop or automatic approval; newer-event ordering. |
| R03 — disconnect before write callback loses verification | Mark a settings SET as possibly transmitted only after GATT accepts the exact active payload. The production release path retains this edit even before the write callback/response. Queued or unaccepted writes are not treated as sent. Reconnect issues GETs without replaying SETs. | Queued/selected/accepted boundaries, mismatched payload, disconnect recovery with matching and mismatching actual values, normal later callback and ordinary read control. |

Implementation: `ProtocolClient` owns membership/read-error state, transport acceptance and the pending-edit accessor; `RadioService.pump` reports successful GATT acceptance and `release` uses that accessor. Existing write completion and radio confirmation remain separate stages. Contact lookups wait for active settings verification, but can continue when verification is paused.

## Validation

- `bash scripts/test_android_probe.sh`: **96 MeshCore checks** (30 added to the previous 66), **206 Meshtastic conformance checks**, and existing protocol/metadata/field suites pass locally.
- `node scripts/test_android_remote_web.cjs` and `python3 scripts/test_android_migration.py`: pass.
- Existing API 29/35 smoke/storage fixtures, build, lint, unit tests and permanent signing checks are release gates. Publication requires successful completion; use the release workflow linked below for final results.
- Android **0.3.10 / versionCode 30** retains the package and permanent certificate for in-place updates. Do not uninstall or clear app data to update.

The additional production regressions are in `McIntegrationTest.membershipRegressions` and `earlyWriteRegressions`, run by both the local script and its Gradle JUnit wrapper. [Implementation PR #224](https://github.com/Renbrant/MeshCrossway/pull/224) contains the source changes and CI evidence. [Release notes and signed APK](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.3.10) provide the final release assets. The [permanent-release workflow](https://github.com/Renbrant/MeshCrossway/actions/workflows/android-permanent-release.yml) includes the API 29/35 and signing gates.

The 0.3.9 audit runner remains pinned to the old release and intentionally exits 1. It is historical evidence, not a test of the corrected source and not a release gate.

## Limits and physical acceptance

These are bounded source/automated-regression results, not a physical-radio certification or a claim that every possible integration defect is closed. Real Bluetooth callback ordering and radio/RF behavior still need hardware acceptance. Pending-edit and late-ACK restoration are limited to service lifetime and reconnection to the same selected radio. Process-death durability and automatic retransmission are not introduced.

On physical radios, test automatic/manual approval, policy-excluded contacts, full contact storage, deletion/re-addition, and repeated adverts. Interrupt Bluetooth immediately after a settings write is accepted, reconnect, and compare actual values with the official app; verify both applied and unapplied outcomes without automatic SET replay. Keep Meshtastic connected and verify the separate histories remain intact.
