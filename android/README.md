# MeshCrossway Android

**Development: 0.2.3 beta (versionCode 13), permanently signed release pending maintainer setup** · [Install the Android APK](../docs/android-apk.md) · [Previous 0.2.1 recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1) · [Report a bug](https://github.com/Renbrant/MeshCrossway/issues)

Native ARM64 Android 8.0+ app for **two independent, simultaneous BLE radio sessions**: one Meshtastic and one MeshCore Companion. The application does **not** bridge, route or relay messages across networks. Its primary UI includes chats, channels, contacts, offline MapLibre areas, local phone GPS tracks/GPX and saved message history.

## New in 0.2.2 — Browser chats, channels, and visual offline maps

On the remote browser's **Messages** screen, select Meshtastic or MeshCore, then switch between three lists: **Chats** (private message history), **Channels** (configured channels even with no messages plus saved group conversations), and **Nodes** (start a direct message to a contact). Clicking a channel or node opens its conversation, with read/send actions where supported.

Under **Offline maps**, use the interactive preview: pan/zoom, click **Draw area**, drag a rectangle, set the maximum offline zoom and name, then download the selected region to phone storage. Previously downloaded regions can be shown on the map, resumed, paused, or deleted. No manual latitude/longitude entry. Browser location is opt-in and uses the computer's geolocation permission, not automatic sharing of the phone's position.

**Data sources are separate:** the browser only views tiles currently on screen from OpenStreetMap's raster map with visible attribution. It never bulk/offline downloads those raster tiles. Offline packs are downloaded using the phone's existing OpenFreeMap/MapLibre provider and its quota, with a 2,400-tile/45 MB per-region cap. The live map preview needs internet on the computer and new offline region downloads need internet on the phone.

## Existing 0.2.1 behavior — Short numeric PIN

From Home, tap **Remote Access**. The app suggests a six-digit numeric PIN; you can enter your own **four to eight digits** using the phone keypad. Tap Enable, then open the displayed HTTPS address from a computer on the **same trusted Wi-Fi network** and enter **the exact same PIN**. The long certificate SHA-256 fingerprint is behind **View HTTPS certificate fingerprint** instead of cluttering the main screen. Verify it before overriding the browser's self-signed certificate warning.

The browser workspace includes an independent Meshtastic/MeshCore dashboard, Chats/Channels/Nodes tabs, interactive visual rectangle selection for offline map areas, and track folders with GPX export.

Remote Access is **off by default** and binds only to the local Wi-Fi IP. It has no internet relay, no open port forwarding and no unattended remote login. Stop it from the phone when finished. The PIN is not stored between server activations: a fresh six-digit suggestion is generated every time. Use **Change PIN** at any time to immediately log out browser sessions and require the new PIN. The TLS private key is encrypted at rest by an AES-GCM key held in Android Keystore.

## New in 0.2.3 — data durability and portable recovery

Public Android APKs will use **one permanent release signing identity**, so future updates can be installed over the existing app without uninstalling or erasing private data. The release workflow refuses to publish an APK unless its certificate matches the pinned long-lived signer fingerprint; the maintainer must configure the private signing-key GitHub secrets once.

**Settings → Data & backup** now includes **Export complete backup** and **Restore complete backup**. The backup ZIP covers both radio histories, channel/node lists, favorites, pinned/muted chats, POIs, drafts, selected radios, app/map preferences, all GPS tracks with every point, and track folders. Import merges rather than clearing existing data. Existing individual JSON history and GPX export remain; new **Import GPX track** supports migration from 0.2.2.

**Important:** 0.2.2 and earlier debug-signed APKs used different certificates and may require one final manual migration. Do not uninstall an older version before exporting its history/GPX or using the optional one-time ADB migration script. The current 0.2.2 APK cannot retroactively export the new complete ZIP; the ADB migration helper can extract this data from debuggable installations. Android operating-system permissions and offline map tile blobs are not part of the ZIP. Normal in-place updates do retain offline maps and permissions.

**Windows ADB backup (one-click):** with the old app still installed and USB debugging authorized, run `scripts/MeshCrossway-Backup-Windows.cmd` from the [Android source branch](https://github.com/Renbrant/MeshCrossway/tree/baseline/android-0.1.0). The launcher requires Android Platform Tools and Python 3, checks that the debug APK is accessible through `adb run-as`, and saves a timestamped, CRC-checked and schema-checked ZIP at `%USERPROFILE%\\MeshCrossway-Backups`. It does **not** uninstall the existing app or upload any data. The optional `verify` command can recheck an existing ZIP. Preserve your 0.2.2 installation until the first permanent-key release and the migration are safe. **Subsequent updates:** drag an officially signed APK onto `scripts/MeshCrossway-Update-Windows.cmd` to run a safe `adb install -r` without uninstalling; an incompatible signature is rejected rather than wiping private data.

See [Permanent signing & data migration](../docs/android-permanent-signing-backup.md) for detailed precautions and private GitHub secret setup. The complete ZIP contains private messages and GPS history **without encryption**: store it securely.

## Build and validation

```sh
# JDK 17 and Android SDK 35
bash scripts/test_android_probe.sh
cd android
./gradlew --no-daemon assembleDebug lintDebug
./gradlew connectedDebugAndroidTest
```

The `android/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk` output is a development APK; public prerelease downloads are linked above. GitHub Actions validates the protocol fixtures, Java build/lint and API 29/API 35 instrumented emulator tests. The previous [0.2.1 successful CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42655097137) validated HTTPS login with a custom PIN and session changes. Version 0.2.2 adds JavaScript geometry/navigation tests and Android API 29/35 browser asset checks.

**Still requires physical acceptance:** Galaxy S25 + both radio devices active, Wi-Fi reachability from a computer, background/Doze behavior, Wi-Fi changes, actual messaging, map downloads and GPX.

## Versioning and recovery

- **0.1.0**: preserved functional beta baseline ([release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0)).
- **0.1.1**: paired Cairn-style Meshtastic/MeshCore Home dashboard ([recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.1)).
- **0.2.0**: local HTTPS remote workspace, browser messaging, offline region management and track folders ([recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.0)).
- **0.2.1**: user-chosen four-to-eight-digit numeric PIN; random six-digit suggestion, simplified phone/browser sign-in and immediate session revocation on PIN change ([recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1)).
- **0.2.2**: browser Chats, Channels and Nodes tabs, visual rectangle-based offline map selection and saved-area previews ([recovery beta](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.2)).
- **0.2.3**: permanent release signing and complete backup/restore; one-time recovery from old debug-signed APKs.
- **0.2.3+**: subsequent small fixes; **0.3.0** or later for larger feature releases; **1.0.0** remains a future production milestone.

The installed app may be signed with a different CI debug key. If Android rejects an update due to a signing conflict, first export JSON history and required GPX tracks, then uninstall/reinstall. **Uninstalling deletes local application data.**

Technical implementation and limitations: [Android remote workspace design](../docs/android-remote-workspace.md) · [Android feature references](../docs/android-feature-references.md).
