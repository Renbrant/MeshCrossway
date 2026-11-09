# MeshCrossway Android

**Public beta: 0.2.1 (versionCode 11)** · [Install the Android APK](../docs/android-apk.md) · [Version 0.2.1 release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.1) · [Report a bug](https://github.com/Renbrant/MeshCrossway/issues)

Native ARM64 Android 8.0+ app for **two independent, simultaneous BLE radio sessions**: one Meshtastic and one MeshCore Companion. The application does **not** bridge, route or relay messages across networks. Its primary UI includes chats, channels, contacts, offline MapLibre areas, local phone GPS tracks/GPX and saved message history.

## New in 0.2.1 — Simple short PIN for Remote Access

From Home, tap **Remote Access**. The app suggests a six-digit numeric PIN; you can enter your own **four to eight digits** using the phone keypad. Tap Enable, then open the displayed HTTPS address from a computer on the **same trusted Wi-Fi network** and enter **the exact same PIN**. The long certificate SHA-256 fingerprint is behind **View HTTPS certificate fingerprint** instead of cluttering the main screen. Verify it before overriding the browser's self-signed certificate warning.

The browser workspace includes an independent Meshtastic/MeshCore dashboard, radio-specific messaging, contacts/channels, offline map area management and tracks organized in virtual folders with GPX export. New offline map areas use geographic coordinate bounds and zoom; the map drawing UI is not yet implemented remotely.

Remote Access is **off by default** and binds only to the local Wi-Fi IP. It has no internet relay, no open port forwarding and no unattended remote login. Stop it from the phone when finished. The PIN is not stored between server activations: a fresh six-digit suggestion is generated every time. Use **Change PIN** at any time to immediately log out browser sessions and require the new PIN. The TLS private key is encrypted at rest by an AES-GCM key held in Android Keystore.

## Build and validation

```sh
# JDK 17 and Android SDK 35
bash scripts/test_android_probe.sh
cd android
./gradlew --no-daemon assembleDebug lintDebug
./gradlew connectedDebugAndroidTest
```

The `android/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk` output is a development APK; public prerelease downloads are linked above. GitHub Actions validates the protocol fixtures, Java build/lint and API 29/API 35 instrumented emulator tests. [0.2.1 successful CI run](https://github.com/Renbrant/MeshCrossway/actions/runs/42655097137) includes HTTPS login with a custom PIN, leading-zero support, PIN changes, session invalidation and existing protocol/track regression tests.

**Still requires physical acceptance:** Galaxy S25 + both radio devices active, Wi-Fi reachability from a computer, background/Doze behavior, Wi-Fi changes, actual messaging, map downloads and GPX.

## Versioning and recovery

- **0.1.0**: preserved functional beta baseline ([release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.0)).
- **0.1.1**: paired Cairn-style Meshtastic/MeshCore Home dashboard ([recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.1.1)).
- **0.2.0**: local HTTPS remote workspace, browser messaging, offline region management and track folders ([recovery release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.2.0)).
- **0.2.1**: user-chosen four-to-eight-digit numeric PIN; random six-digit suggestion, simplified phone/browser sign-in and immediate session revocation on PIN change.
- **0.2.2+**: subsequent small fixes; **0.3.0** or later for larger feature releases; **1.0.0** remains a future production milestone.

The installed app may be signed with a different CI debug key. If Android rejects an update due to a signing conflict, first export JSON history and required GPX tracks, then uninstall/reinstall. **Uninstalling deletes local application data.**

Technical implementation and limitations: [Android remote workspace design](../docs/android-remote-workspace.md) · [Android feature references](../docs/android-feature-references.md).
