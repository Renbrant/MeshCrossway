# Android release history and numbering

The canonical current beta is **0.3.11**. The four MeshCore everyday tools were mistakenly published as **0.4.0** and are republished as 0.3.11 with a higher internal versionCode. Historical tags and APKs remain unchanged for traceability.

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
| 0.4.0 | Presence announcements, channel management/sharing, support diagnostics and contact state filters | Mistaken number; superseded by 0.3.11 |
| 0.3.11 | Same four tools and safeguards, with corrected numbering and versionCode 32 | Current beta |

## Numbering rule

Keep the full three-component Android version: **0.3.9 → 0.3.10 → 0.3.11 → 0.3.12 → …**. The final component is an integer, not a decimal digit; reaching 9 does not require a minor-version increase. Continue the 0.3.x series for subsequent revisions. A new minor series or 1.0.0 requires an explicit release decision. ESP32 firmware has a separate version sequence.

Android installation ordering uses `versionCode`, not the displayed version name. 0.3.10 used 30, the mistaken 0.4.0 used 31, and corrected 0.3.11 uses 32. Keep the package ID and permanent signing key unchanged. Install 0.3.11 as an update; do not uninstall or clear storage.

## Source and release distinction

Recent Android releases are built from `baseline/android-0.1.0`. That branch name is historical, but its role is active. Website/docs are on `main`, whose Android sources currently lag the released app. Use each release tag for the exact shipped source, rather than assuming `main` matches the APK. Consolidating the source branches is separate work.

Older alpha labels (including 0.4.0-alpha05) predate the public beta numbering and are not the corrected 0.3.x sequence. Do not rename historical alpha artifacts.

[Current APK](https://github.com/Renbrant/MeshCrossway/releases/download/android-v0.3.11/MeshCrossway-0.3.11.apk) · [Release notes](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.3.11) · [MeshCore tools guide](android-meshcore-everyday-tools.md)
