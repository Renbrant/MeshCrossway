# Android release history and numbering

The current published beta is **Android 0.4.8**, versionCode **42**, tag **`android-v0.4.8`**. Latest GPS/map and repeater administration changes still require field testing. The historical tag `android-v0.4.0` is the mistakenly numbered versionCode 31; the later significant 0.4.0 milestone uses `android-v0.4.0-stage1` (34). Historical tags remain unchanged.

## Recent revisions

| Version | Change | Status |
|---|---|---|
| 0.2.6 | Message actions and official icon | Historical beta |
| 0.2.7 | Account sign-in | Historical beta |
| 0.2.8 | Opt-in cloud conversation sync | Historical beta |
| 0.2.9 | GPX track cloud sync | Historical beta |
| 0.3.0 | MeshCore device settings | Historical beta |
| 0.3.1–0.3.5 | Meshtastic settings, protocol corrections and save verification | Historical betas |
| 0.3.6–0.3.7 | Compact Home, recent nodes and account toolbar layout | Historical betas |
| 0.3.8 | Meshtastic settings audit corrections | Historical beta |
| 0.3.9 | MeshCore settings/readback, names, power, contact lifecycle and late ACK handling | Historical beta |
| 0.3.10 | Contact membership resolution, NOT_FOUND recovery and verification after interrupted settings writes | Historical beta |
| 0.4.0 (historical tag `android-v0.4.0`) | Presence announcements, channel management/sharing, support diagnostics and contact state filters | Mistaken number; superseded by 0.3.11 |
| 0.3.11 | Same four tools and safeguards, with corrected numbering and versionCode 32 | Historical beta |
| 0.3.12 | Both-network channel management, official links and QR camera/image imports; versionCode 33 | Historical beta |
| 0.4.0 (tag `android-v0.4.0-stage1`) | Accurate BLE RSSI age, LoRa reception history, safer outgoing ACK and manual retry drafts; versionCode 34 | Historical signed beta |
| 0.4.1 | Passive dual-radio diagnostics, MeshCore remote repeater status and paginated neighbors, five-minute manual airtime guard; versionCode 35 | Previous signed field beta |
| **0.4.2** | Controlled 30-second manual repeater queries, privacy-safe unexpected response byte length, automatically refreshed cooldown buttons; versionCode 36 | Historical signed field beta |
| 0.4.3 | MeshCore status extensions and count-driven neighbor decoding; versionCode 37 | Historical signed beta |
| 0.4.4 | Signed follow-up beta; see tagged source and release notes; versionCode 38 | Historical signed beta |
| 0.4.5 | Direct repeater configuration cards, automatic initial reads after administrator login; versionCode 39 | Historical signed beta |
| 0.4.6 | Editable repeater settings and corrected save/readback and console feedback; versionCode 40 | Historical signed beta |
| 0.4.7 | Continuous GPS following and travel direction; versionCode 41 | Historical signed beta |
| 0.4.8 | Vector travel arrow and map camera padding corrections; versionCode 42 | Current signed beta; field testing pending |

## Numbering rule

Keep the full three-component Android version: **0.3.9 → 0.3.10 → 0.3.11 → 0.3.12 → …**. The final component is an integer, not a decimal digit; reaching 9 does not require a minor-version increase. A deliberate major feature milestone introduced **0.4.0** after **0.3.12**; crossing a patch boundary alone never requires a minor bump. A later `1.0.0` requires separate acceptance. ESP32 firmware has a separate version sequence.

Android installation ordering uses `versionCode`, not the displayed version name. 0.3.10 used 30, the mistaken 0.4.0 used 31, and corrected 0.3.11 uses 32. Version 0.3.12 uses 33; the corrected 0.4.0 Stage 1 uses **34**, 0.4.1 uses **35**, and 0.4.2 uses **36**. Keep the package ID and permanent signing key unchanged. The current 0.4.8 uses **42**. Install 0.4.8 over a compatible permanently signed previous version; do not uninstall or clear storage. Back up your data because Android normally disallows an in-place downgrade to a lower versionCode.

## Source and release distinction

Android releases through 0.4.8 were built from `baseline/android-0.1.0`. That history is preserved. The 0.4.8 Android source is now integrated into `main`, the canonical branch for development, contributions and subsequent releases. Use each immutable release tag to audit the exact source of its shipped APK. See [the contribution and release workflow](../CONTRIBUTING.md).

Older alpha labels (including 0.4.0-alpha05) predate the public beta numbering. Historical artifacts retain their original tags; the new 0.4.0 beta's special `-stage1` tag prevents overwriting the earlier mistakenly numbered 0.4.0.

[Current 0.4.8 APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.4.8/MeshCrossway-0.4.8.apk) · [0.4.2 release notes](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.2) · [MeshCore tools guide](android-meshcore-everyday-tools.md)
