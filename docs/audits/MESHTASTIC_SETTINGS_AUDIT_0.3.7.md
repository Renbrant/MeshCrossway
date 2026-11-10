# Meshtastic settings audit — Android 0.3.7

Date: 2026-11-10. Result: **partial conformance; corrections required before claiming parity with the official app**.

## Scope and pinned references

Audited the released MeshCrossway Android `android-v0.3.7` source at [`380e14acca507656660da3657659b8e18698abbc`](https://github.com/Renbrant/MeshCrossway/tree/380e14acca507656660da3657659b8e18698abbc), not an unreleased working branch. Compared settings UI, protobuf fields, validation, firmware compatibility and save/readback handling with:

- [Official Android app v2.8.4, `927b385`](https://github.com/meshtastic/Meshtastic-Android/tree/927b38549110091fd7051daa24cc77150fd45849).
- [Official protobuf definitions, `4c80682`](https://github.com/meshtastic/protobufs/tree/4c80682b3b441a4cebab9f3e5c3097857cb5c81d).
- [Official firmware develop, `d9c9916`](https://github.com/meshtastic/firmware/tree/d9c9916e73952caa1740d4becc367374c5e28e44), used to explain normalization and hardware-dependent behavior. Develop is not a guarantee of support on every released radio; the official app's firmware gates remain the comparison baseline.

This is source analysis plus executable JVM protocol probes. No Bluetooth pairing, physical radio save, on-air RF measurement, or end-to-end Android UI test was performed. The account toolbar, MeshCore configuration and ESP32 controller are outside this audit.

## Confirmed findings

P1 means fix before extending the affected settings workflow; P2 means a functional or parity defect with narrower exposure. These priorities do not imply that every ordinary preset change fails.

| ID | Priority | Finding and observable effect | Required correction |
| --- | --- | --- | --- |
| M01 | P1 | `MtLora.presetCodingRate()` returns 4/5 for LONG_TURBO, NARROW_FAST, NARROW_SLOW and TINY_SLOW. Official defaults are respectively 4/8, 4/6, 4/6 and 4/6. The UI can offer overrides below the actual preset default, and readback normalization can falsely reject equivalent values. | Match the complete official preset table; compare effective coding rates and test every preset. |
| M02 | P1 | `MtAdmin.matches()` verifies only the fields changed by the edit. A hop-limit change from 3 to 4 is marked saved even if the readback unexpectedly changes LONG_FAST to MEDIUM_FAST. The changed hop limit is verified, but the displayed success does not surface other LoRa drift. | Check the complete desired effective LoRa state, allowing documented firmware normalization; identify unexpected differences. |
| M03 | P2 | LONG_TURBO is offered starting at firmware 2.7.0. The official app requires 2.7.17. Older firmware can reject or normalize the unsupported choice. | Use the official 2.7.17 minimum and exercise version boundaries. |
| M04 | P2 | Frequency-slot validation uses a legacy span table ending at region 26, and ignores spacing/padding. For EU_866 + LITE_FAST it accepts slot 5, while the official calculation permits four slots. New regions also fall through to a generic power ceiling of 36 dBm instead of a complete regional model. This is a validation gap; actual radio enforcement was not tested. | Port regional spacing, padding, frequency ranges and power constraints; match the official slot formula. |
| M05 | P2 | Bluetooth enabled and Security serial-console enabled use a fallback of `1` when the protobuf field is absent. Proto3 omitted boolean fields mean false. The UI can show enabled while the decoded setting and toggle handler see disabled. | Decode absent booleans consistently as false; test empty/default config objects at the UI boundary. |
| M06 | P2 | Manual SF5/SF6 is rejected whenever the hardware name contains TBEAM, V1 or V2. This also rejects an SX1262-equipped TBEAM S3 CORE. Official firmware decides using the actual RF95 radio type, not a name substring. | Model modem capability; distinguish RF95 from SX1262 variants and handle unknown capabilities explicitly. |
| M07 | P2 | Licensed-mode onboarding accepts up to 24 bytes for `HamParameters.long_name`; the protocol permits 14 usable bytes. Its composed-name check uses a space instead of firmware's `//` separator. It accepts a seven-byte callsign plus a sixteen-byte description, exceeding both the field budget and the composed owner-name budget. Verification checks only `is_licensed`, not the resulting names. | Enforce the field's 14-byte limit, compose with `//`, and verify returned identity as well as licensed state. |
| M08 | P2 | The Modules page is gated by MQTT availability and unconditionally exposes Status message after loading MQTT. It does not independently honor the status module's firmware/exclusion/load checks, although the setter does. This creates unavailable controls and can hide the entire page when MQTT is excluded. | Gate and load each module independently; reuse the guarded status control already used on the User page. |

### Source evidence and reproductions

MeshCrossway files below are all pinned to the audited release:

- [MtLora.java](https://github.com/Renbrant/MeshCrossway/blob/380e14acca507656660da3657659b8e18698abbc/android/app/src/main/java/com/meshcrossway/android/MtLora.java): M01 at `presetCodingRate` / `codingSelection`; M03 at `selectable`; M04 at `span`, `powerLimit` and `validate`; M06 at the manual-SF hardware-name check.
- [MtAdmin.java](https://github.com/Renbrant/MeshCrossway/blob/380e14acca507656660da3657659b8e18698abbc/android/app/src/main/java/com/meshcrossway/android/MtAdmin.java#L49): M02, delta-only matching; M01, semantic CR matching delegated to the incorrect table.
- [MainActivity.java](https://github.com/Renbrant/MeshCrossway/blob/380e14acca507656660da3657659b8e18698abbc/android/app/src/main/java/com/meshcrossway/android/MainActivity.java#L898): M05 in `mtDeviceSettings` and `mtSecuritySettings`; M08 in `mtModuleSettings`.
- [ProtocolClient.java](https://github.com/Renbrant/MeshCrossway/blob/380e14acca507656660da3657659b8e18698abbc/android/app/src/main/java/com/meshcrossway/android/ProtocolClient.java#L109): M07 in `setMtHam`.

Official comparison evidence:

- [ChannelOption.kt](https://github.com/meshtastic/Meshtastic-Android/blob/927b38549110091fd7051daa24cc77150fd45849/core/model/src/commonMain/kotlin/org/meshtastic/core/model/ChannelOption.kt): preset defaults and version gates around lines 497–525; slot calculation around lines 70–88; EU_866 spacing/padding around lines 350–373.
- [HamName.kt](https://github.com/meshtastic/Meshtastic-Android/blob/927b38549110091fd7051daa24cc77150fd45849/core/model/src/commonMain/kotlin/org/meshtastic/core/model/HamName.kt): 7-byte callsign, 14-byte description, `//` composition.
- [admin.options](https://github.com/meshtastic/protobufs/blob/4c80682b3b441a4cebab9f3e5c3097857cb5c81d/meshtastic/admin.options): HamParameters field sizes include the terminating NUL.
- [MeshRadio.h](https://github.com/meshtastic/firmware/blob/d9c9916e73952caa1740d4becc367374c5e28e44/src/mesh/MeshRadio.h#L124): RF95-specific SF5/SF6 restriction.
- [TBEAM S3 variant](https://github.com/meshtastic/firmware/blob/d9c9916e73952caa1740d4becc367374c5e28e44/variants/esp32s3/tbeam-s3-core/variant.h): SX1262 and RF95 build support. Hardware identity alone does not establish the installed modem.
- [AdminModule.cpp](https://github.com/meshtastic/firmware/blob/d9c9916e73952caa1740d4becc367374c5e28e44/src/modules/AdminModule.cpp): coding-rate normalization and `handleSetHamMode` composition.

## Coverage compared with the official app

“Partial” means the screen exists but does not provide the full official editor. Missing controls are feature gaps, separate from incorrect values above.

| Settings area | MeshCrossway 0.3.7 coverage | Assessment |
| --- | --- | --- |
| Identity | Local owner long/short names, licensed-mode onboarding, status text | Partial; ordinary name-field mappings are correct, licensed flow needs M07. |
| LoRa | Region, preset/manual parameters, coding-rate override, hop limit, TX, power, frequency slot/override, duty-cycle override, RX boost and MQTT flags | Broad coverage; M01–M04 and M06 prevent full conformance. |
| Device | Role selection | Partial; most device options are absent. |
| Position, Power, Network, Display | Rows explain that editing is unavailable in this release | Not implemented as configuration editors. |
| Bluetooth | Enabled toggle | Partial; pairing mode and fixed PIN are absent; M05 affects displayed state. A disconnect warning already exists. |
| Security | Serial console, debug API and supported packet-signature policy; key/managed summaries | Partial; full key and administration controls are absent; M05 affects serial state. |
| Channels | Rename an existing channel, preserving other fields | Partial; add/delete, roles, PSK and sharing workflows are absent. |
| Modules | MQTT enable and status message | Partial; MQTT connection fields and most official module editors are absent; M08 affects availability. |
| Administration | Local BLE settings operations | Partial; shell, complete import/export and other official administration workflows are not implemented. |

## Correct behavior retained

The previous remediation did fix important protocol problems. Inspection and the existing regression suite confirm explicit preset/region wire IDs, typed config wrappers, default-valued empty config presence, preservation of untouched/unknown fields, correlated admin responses, session-key refresh, ACK/readback sequencing and reboot recovery. A changed preset that reads back with a different value is rejected. LONG_FAST's stored CR5 versus requested default CR0 is accepted, as intended.

The save flow reads settings back and does not automatically resend a previously submitted write. These are useful foundations; the findings above concern remaining compatibility and comparison gaps.

## Reproducible validation

The existing `bash scripts/test_android_probe.sh` passed on OpenJDK 21, including Meshtastic settings regressions. It does not cover every official table/default.

Additional audit probes: **18 checks, 6 passed, 12 failed**, grouped into M01–M07. M08 is a static UI-path finding. Multiple failures exercise M01; these are not twelve independent production bugs. UI boolean probes reproduce the exact decoding expressions, not an instrumented Android screen. The SF6 probe demonstrates the name-based rejection and assumes an SX1262 variant; it cannot detect a connected modem.

Run from a full repository clone with the audited commit available and a JDK on PATH:

```sh
python3 docs/audits/run_meshtastic_settings_037_probe.py
```

The runner extracts production code from the pinned release commit into a temporary directory. It leaves application source unchanged and intentionally exits **1** for this release's nonconformance. These audit probes are not added to release CI.

- [Probe fixture](fixtures/MeshtasticSettings037Probe.java)
- [Runner](run_meshtastic_settings_037_probe.py)
- [Recorded output](evidence/MESHTASTIC_SETTINGS_0.3.7_PROBE.txt)

## Recommended correction order and acceptance

1. Correct the preset table and compare the complete effective LoRa configuration after reboot, with explicit handling of firmware normalization (M01–M02).
2. Match version gates and region math, then boolean defaults, modem capabilities and licensed identity limits (M03–M07).
3. Correct per-module availability (M08), then expand the missing editors incrementally.
4. Validate on physical radios: LONG_FAST ↔ SHORT_FAST, LONG_TURBO where supported, default/override CR, reconnect interruption, Bluetooth disable, empty/default settings and identity updates. Confirm the fresh returned settings against the official app on the same radio. Keep MeshCore connected during the tests.

The findings can explain some false “change not confirmed” states, particularly coding-rate normalization. They do **not** prove the cause of the reported LONG_FAST screenshot without the requested values and returned config. The separate stale remote-node username report also requires a NodeInfo/cache audit; ordinary text reception does not prove updated identity metadata was received.

This audit publishes evidence only. It does not alter the APK, firmware or the current download version.
