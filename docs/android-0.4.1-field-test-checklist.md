# Android 0.4.1 Stage 2 — Release and real-radio acceptance

**Release-candidate checklist. Signed v0.4.0 must remain published and immutable.** VersionCode 35 is greater than the permanently signed 0.4.0 beta's versionCode 34.

## CI gate

- [ ] Merge this version bump only after JUnit/protocol fixtures, Android debug build/lint and API29/API35 emulator smoke pass.
- [ ] Verify `.github/workflows/android-permanent-release.yml` push run on `baseline/android-0.1.0` completes storage instrumentation without auto-publishing.
- [ ] Check manual workflow `workflow_dispatch` is required for the `release` job; `version` must exactly equal `0.4.1` and build.gradle must have versionName 0.4.1/versionCode 35.
- [ ] Verify release keystore is the same permanent signing key as 0.4.0; CI fails closed if fingerprint differs.
- [ ] Preserve historical release tags `android-v0.4.0` and `android-v0.4.0-stage1`; never overwrite them.
- [ ] Confirm no release or public website points at `android-v0.4.1` until release + asset checks pass.
- [ ] Maintainer explicitly triggers manual release only when ready to distribute an **experimental field beta**. This is for real-radio validation; it is not a stability certification.

## Physical field gate (after installing signed experimental beta)

- [ ] 0.4.1 installs over signed 0.4.0 **without uninstall**; chats, channel keys, local maps, GPX tracks and settings preserved.
- [ ] Both independent BLE radios Ready simultaneously; no bridging or relay between protocols.
- [ ] Passive advanced diagnostics show **GATT frames vs saved messages** with accurate labels; no RF query on page open.
- [ ] MeshCore repeater inventory shows only verified type-2 saved contacts, accurately labels stale observations, and can open saved position.
- [ ] Manual remote status request has an explicit **flood airtime warning**. Local radio acceptance does **not** count as remote response.
- [ ] Response parsing shows authentic remote stats or clear **timeout/unsupported** state and remains stable with malformed/unavailable replies.
- [ ] Manual neighbor request returns accurate total and max 8 neighbors/page with prefixes/ages/SNR where firmware supports; ambiguous prefixes are not misidentified.
- [ ] Five-minute throttle per **full repeater identity**, across status/neighbor/pages, after BLE disconnect and process restart; one in-flight request only, no blind retry.
- [ ] Test RF traffic conservatively on an authorized repeater and compare observed actual mode with app flood/known-path indicator. No background polling.
- [ ] Validate normal Meshtastic and MeshCore chats, channel QR/import, maps, saved tracks and cloud account opt-in remain unchanged.
- [ ] Record exact radio firmware versions and sanitized support report before closing #239.

## Publication sequencing

1. Integrate candidate PR and validate **final baseline commit** with GitHub Actions.
2. Execute **Android permanent-key release** manually with `version=0.4.1` on `baseline/android-0.1.0`; normal pushes never publish.
3. Verify prerelease tag `android-v0.4.1`, signed `MeshCrossway-0.4.1.apk`, `SHA256SUMS.txt`, `SIGNING-CERT-SHA256.txt` and correct source commit/versionCode/certificate.
4. Only then update `main` README and landing page download links. Keep older version available; label 0.4.1 **experimental field beta** until real device acceptance.
5. Do not close #239 until hardware validation; do not mark this beta stable or claim verified repeater status on all firmware versions.
