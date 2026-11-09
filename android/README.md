# MeshCrossway Android

**Development: 0.2.5 beta (versionCode 15), using the permanent signing identity established by 0.2.3** · [Install the Android APK](../docs/android-apk.md) · [Previous 0.2.1 recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1) · [Report a bug](https://github.com/Renbrant/MeshCrossway/issues)

Native ARM64 Android 8.0+ app for **two independent, simultaneous BLE radio sessions**: one Meshtastic and one MeshCore Companion. The application does **not** bridge, route or relay messages across networks. Its primary UI includes chats, channels, contacts, offline MapLibre areas, local phone GPS tracks/GPX and saved message history.

## New in 0.2.2 — Browser chats, channels, and visual offline maps

On the remote browser's **Messages** screen, select Meshtastic or MeshCore, then switch between three lists: **Chats** (private message history), **Channels** (configured channels even with no messages plus saved group conversations), and **Nodes** (start a direct message to a contact). Clicking a channel or node opens its conversation, with read/send actions where supported.

Under **Offline maps**, use the interactive preview: pan/zoom, click **Draw area**, drag a rectangle, set the maximum offline zoom and name, then download the selected region to phone storage. Previously downloaded regions can be shown on the map, resumed, paused, or deleted. No manual latitude/longitude entry. Browser location is opt-in and uses the computer's geolocation permission, not automatic sharing of the phone's position.

**Data sources are separate:** the browser only views tiles currently on screen from OpenStreetMap's raster map with visible attribution. It never bulk/offline downloads those raster tiles. Offline packs are downloaded using the phone's existing OpenFreeMap/MapLibre provider and its quota, with a 2,400-tile/45 MB per-region cap. The live map preview needs internet on the computer and new offline region downloads need internet on the phone.

## Existing 0.2.1 behavior — Short numeric PIN

From Home, tap **Remote Access**. The app suggests a six-digit numeric PIN; you can enter your own **four to eight digits** using the phone keypad. Tap Enable, then open the displayed HTTPS address from a computer on the **same trusted Wi-Fi network** and enter **the exact same PIN**. The long certificate SHA-256 fingerprint is behind **View HTTPS certificate fingerprint** instead of cluttering the main screen. Verify it before overriding the browser's self-signed certificate warning.

The browser workspace includes an independent Meshtastic/MeshCore dashboard, Chats/Channels/Nodes tabs, interactive visual rectangle selection for offline map areas, and track folders with GPX export.

Remote Access is **off by default** and binds only to the local Wi-Fi IP. It has no internet relay, no open port forwarding and no unattended remote login. Stop it from the phone when finished. The PIN is not stored between server activations: a fresh six-digit suggestion is generated every time. Use **Change PIN** at any time to immediately log out browser sessions and require the new PIN. The TLS private key is encrypted at rest by an AES-GCM key held in Android Keystore.

## Recommended from a clean installation: install once, then update in place

If you have **already uninstalled** the previous debug-signed 0.2.2 and are
starting fresh, **do not run the ADB migration or ZIP restore process.** There
is nothing left in the old app to migrate. Wait for the first
**permanently signed** 0.2.3 APK before creating new messages and tracks.

After that, normal Android updates signed with the **same certificate**, with
the same package ID and increasing versionCode, preserve local SQLite histories,
favorite flags, selected Meshtastic/MeshCore radios, settings, track databases
and all existing MapLibre offline regions **automatically**. The user simply
opens the official new APK and chooses **Update** over the existing install.
Do not choose Uninstall, Clear storage or Clear data.

The one-time private release-signing key setup is a **maintainer task**, not a
requirement for app users to install ADB or Python. The current release
workflow blocks publication if the permanent key is missing or different.

Optional advanced export/restore and ADB recovery tools remain available for
older devices that **still have an incompatible debug APK installed**, or for
manual migration to another phone. They are NOT the normal update procedure.
An uninstall deliberately destroys the app-private data; no app can silently
read those erased files later. Android Auto Backup is currently disabled for
private messages and location histories, and is not needed for normal updates.

## New in 0.2.5 — Compact Home and Bluetooth signal strength

The Home opens with the **Meshtastic** and **MeshCore** connection cards side by side, immediately under the app header. The redundant global `MT ready · MC ready` line and full-width `Dual mesh dashboard` summary were removed from Home, as were the repeated headings between **Nodes, Chats, Channels, GPS and Tracks**. Card margins and vertical spacing are tighter, while every existing card retains its tap action and the independent per-radio colors.

Each connection card now has **four Bluetooth signal bars** and a measured **BLE RSSI in dBm**, read with Android's `BluetoothGatt.readRemoteRssi()` only when the corresponding radio's normal BLE operation queue is idle (approximately once every 20 seconds). The last valid RSSI reading is stored *for that exact radio address*, and shown as **Last BLE** in muted colors after disconnection. If no measurement is available, the card says **BLE signal unavailable**. The value is the signal strength of the **phone-to-radio BLE connection**, **not** a LoRa link-quality measurement. Skipping or failing an RSSI read does not disconnect the radios or interrupt messaging.

**Remote Access** remains available lower on Home and retains the same HTTPS/PIN functionality. No message, node, channel, GPS-track, offline-map or firmware protocol schema has changed. The release still uses the same permanent Android signing certificate introduced in 0.2.3; install newer APKs **on top of** the existing app without uninstalling, to preserve local data.

## New in 0.2.4 — Appearance and color themes

Go to **Settings → Appearance & colors**. Four styles are available:

- **System** — automatically follows Android's light/dark system appearance; the original dark style keeps its orange accent.
- **Color** — choose any hue using the slider or preset swatches; tinted surfaces and app buttons update throughout the UI.
- **Gradient** — choose any hue and switch between **Linear** and soft multi-point **Mesh** backgrounds.
- **Photo** — choose a picture from the phone and adjust **Dim** and **Blur** separately. MeshCrossway re-encodes the photo (stripping EXIF information) and saves a private copy under app storage so it survives future in-place APK updates. The source image is not uploaded.
- **Reset** — switch back to System at any time; in Photo mode, Remove background photo deletes the app's private copy.

All theme settings are stored in the existing `ui` preferences file, and **survive future same-signature APK updates**, just like local chats and recorded GPS tracks. Color changes do not alter BLE or LoRa protocols. Meshtastic remains orange and MeshCore remains cyan in their network-status cards and map markers. An offline browser companion currently keeps its own dark presentation independent of these native Android theme options.

A complete portable ZIP backup includes the theme preference values but **not the user's photo file**; a full uninstall or app data wipe removes that private image, even though ordinary updates do not. In that case, reselect the photo after reinstalling.

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
- **0.2.3**: first permanent release signing and complete backup/restore; one-time recovery from old debug-signed APKs.
- **0.2.4**: System, Color, Gradient (Linear/Mesh) and Photo appearance options, with persistent customization.
- **0.2.5**: compact Home, two leading connection cards, true per-radio BLE RSSI bars, no duplicate section headings.
- **0.2.3+**: subsequent small fixes; **0.3.0** or later for larger feature releases; **1.0.0** remains a future production milestone.

The installed app may be signed with a different CI debug key. If Android rejects an update due to a signing conflict, first export JSON history and required GPX tracks, then uninstall/reinstall. **Uninstalling deletes local application data.**

Technical implementation and limitations: [Android remote workspace design](../docs/android-remote-workspace.md) · [Android feature references](../docs/android-feature-references.md).
