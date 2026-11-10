# Meshtastic settings corrections — Android 0.3.8

This release addresses all eight finding groups in the [0.3.7 audit](https://github.com/Renbrant/MeshCrossway/blob/main/docs/audits/MESHTASTIC_SETTINGS_AUDIT_0.3.7.md). It corrects the existing editors; it does not add every editor present in the official Meshtastic app.

| Finding | Correction | Verification |
| --- | --- | --- |
| M01: coding-rate defaults | LONG_TURBO uses 4/8; NARROW_FAST, NARROW_SLOW and TINY_SLOW use 4/6. Effective default rates recognize stored values at or below the preset rate. | Every preset's default and override boundary. |
| M02: incomplete readback | Compare the complete effective LoRa state, including omitted zero/default fields and preserved unknown fields. Report a specific differing setting. Inactive manual/preset values and default CR/TX power normalization remain acceptable. | Hop-only write with unexpected preset drift, omitted flag drift, manual SF drift, automatic/explicit power and packed ignore lists. |
| M03: firmware gate | LONG_TURBO requires 2.7.17+. | Below, at and above the boundary. |
| M04: regional limits | Explicit supported frequency/power profiles, regional spacing/padding, rounded slot counts and amateur-mode requirements. Reserved EU_874/EU_917 IDs are not offered without a defined profile. | Maximum valid and first invalid slot across 14 representative profiles; explicit EU power limits and unknown-profile rejection. |
| M05: boolean defaults | UI and toggle actions share proto3 false-default decoding. | Empty loaded configs and actual Android Bluetooth/serial settings views. |
| M06: modem capability | Reject SF5/SF6 only for unambiguous RF95 models. Do not infer an RF95 from TBEAM/V1/V2 substrings. Explain ambiguous variants and require matching radio readback. | RF95, Heltec V3, TBEAM S3 and Wireless Tracker V2 cases; SF normalization mismatch. |
| M07: licensed identity | Enforce 14 UTF-8 description bytes, compose CALLSIGN//description and verify returned long name, short name and licensed state. Firmware before 2.8.0 permits callsign-only onboarding and explains the description limitation. | UTF-8 limits, empty description, returned identity mismatch and legacy behavior. |
| M08: module availability | MQTT and status load independently. Excluded/old/unloaded modules have no edit action. Refresh requests both supported modules. | Independent capability tests and actual Android module views with MQTT/status exclusions and old firmware. |

## Validation

- `bash scripts/test_android_probe.sh`: existing protocol regression fixtures plus **206 additional conformance checks** pass on OpenJDK 21.
- `node scripts/test_android_remote_web.cjs`: passes.
- `python3 scripts/test_android_migration.py`: passes.
- `MtSettingsJUnitTest` runs the new conformance fixture during Gradle tests.
- `SmokeInstrumentation` includes real settings-view assertions during both full smoke tests and the release workflow's API 29/35 storage run.
- CI builds/lints the APK and verifies the permanent signing certificate before publishing.

Source references remain pinned in the original audit. Licensed-name compatibility was additionally checked against official firmware [2.7.17](https://github.com/meshtastic/firmware/blob/217abc4c1065da05b19c7aefb10311386e29212b/src/modules/AdminModule.cpp) and [2.8.0](https://github.com/meshtastic/firmware/blob/47db0e3020a608e06fb65cce70cd2f093021bd82/src/modules/AdminModule.cpp): the older handler stores only the callsign; the newer handler composes the optional description.

Physical-radio/on-air validation is still required. The remote-node stale username report requires separate NodeInfo/cache investigation. No data migration or reset is introduced; install the permanent-key APK as an in-place update.
