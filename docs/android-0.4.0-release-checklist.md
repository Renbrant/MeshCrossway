# Android 0.4.0 beta — release readiness and two-radio acceptance

Roadmap: [#229](https://github.com/Renbrant/MeshCrossway/issues/229) · Implementations [#230](https://github.com/Renbrant/MeshCrossway/pull/230), [#231](https://github.com/Renbrant/MeshCrossway/pull/231), [#232](https://github.com/Renbrant/MeshCrossway/pull/232).

## Release identity

| Field | Value |
|---|---|
| Android display version | **0.4.0** |
| Android versionCode | **34** (previous current beta 0.3.12 was 33) |
| App identifier | `com.meshcrossway.mobile` (unchanged) |
| Release Git tag | **`android-v0.4.0-stage1`** |
| Signed arm64 APK | **`MeshCrossway-0.4.0.apk`** |
| Distribution | GitHub Releases, marked **prerelease** |
| Signing | Original permanent signing certificate, verified before publishing |
| Source | `baseline/android-0.1.0` at the selected workflow commit |
| Out of scope | Controller firmware version; app store launch; automatic RF bridging |

**Why the tag suffix?** GitHub already contains the immutable historical tag `android-v0.4.0` (old wrongly numbered release, versionCode 31) which was superseded by `0.3.11`. Do not delete, overwrite, or reuse that old tag. New 0.4.0 has a higher versionCode and its own unambiguous release URL.

## Automated release gates

- [ ] Current commit passes protocol wire fixtures, map/browser tests, backup migration tests and Android JUnit.
- [ ] Android `assembleDebug`, `assembleRelease`, `lintDebug`/`lintRelease` succeed.
- [ ] Android emulator instrumentation passes API 29 and 35.
- [ ] Workflow verifies permanent signing certificate SHA-256 against configured secret.
- [ ] APK manifest reports **`versionName='0.4.0'`** and **`versionCode='34'`**.
- [ ] New tag has not previously been published, and HEAD still equals the validated baseline commit.
- [ ] Signed-release workflow is manually dispatched: ordinary Git pushes run validation only.

## Physical acceptance (phone + two independent radios)

Baseline known hardware: Samsung Galaxy S25 + one Meshtastic Heltec V3 and one MeshCore Heltec V3.

- [ ] Install permanent-key **0.4.0** over existing signed **0.3.12** via Android Update, not uninstall; confirm history, favorites, radio selection, tracks and offline maps remain intact.
- [ ] Pair and connect both radios simultaneously. Confirm MT and MC cards show *their own* verified Bluetooth RSSI and age; verify disconnected/90-second stale/last reading states.
- [ ] Receive MT group and direct messages and MC channel/direct messages. Open message details; confirm reported SNR/RSSI/hops are identified correctly and absent metrics are unknown rather than invented.
- [ ] Open **Nodes → select → Reception history**, compare per-node source attribution and ensure MeshCore group display names do not create misleading charts. Verify the history still loads offline.
- [ ] Send MT and MC direct and channel messages. Confirm **Submitted over Bluetooth**, **Accepted by radio**, **Peer acknowledged**, **Rejected** and **Delivery unknown** never claim more than is actually acknowledged.
- [ ] Disconnect each Bluetooth radio during a send and restart the app. Check persistent unknown outcomes and **no automatic resend**.
- [ ] Long-press an unknown/rejected outgoing message and choose **Prepare as new draft**. Verify duplicate warning, same protocol/radio/channel/recipient, copy only after confirm, and a separate manual **Send** action.
- [ ] Open a manual redraft confirmation, then receive a late acknowledgement; the app should not prepare a new draft from a message now marked delivered.
- [ ] Regression: channels and sharing/scan QR, GPS tracking and GPX export, offline tiles/areas, local browser workspace, app themes, backup/restore and opt-in account functions.

## Publication control

After the required tests are accepted, GitHub maintainer:

1. Open **Actions → Android permanent-key release**.
2. Choose **Run workflow** with branch **`baseline/android-0.1.0`**, version **`0.4.0`**; run only when the above checks are accepted.
3. Wait for all jobs to complete successfully. The workflow creates a **new** prerelease at `android-v0.4.0-stage1` and attaches the signed APK, its SHA-256 checksum and the certificate fingerprint.
4. Verify a clean update installation. Only after the release URL and file exist, update the **main** README and landing page download button from 0.3.12 to the new 0.4.0 release. Do not point public links to a nonexistent APK.

**No command in this document will replace or modify the old `android-v0.4.0` tag or 0.3.12 release.** Stage 1 remains subject to real-radio acceptance; #229 stays open for Stages 2–4.
