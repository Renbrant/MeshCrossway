# Android permanent signing and safe data migration

## Why reinstalling loses your data

Android requires the **same signing certificate and package ID** for an in-place update. Earlier public MeshCrossway Android betas were published from GitHub Actions debug builds with runner-generated keys. The next debug build may not match. Uninstalling removes SQLite histories, saved tracks, favorites and preferences.

From the first permanently signed version onward, every public APK must use the same signing key. No reset, Clear Data or uninstall is required during ordinary future upgrades. Android system permissions are retained during normal upgrades but need to be granted again after an uninstall.

## Maintainer: one-time signing setup

Generate the permanent keystore **once**, outside the repository, with JDK 17 or later:

    keytool -genkeypair -v -keystore MeshCrossway-release.p12 -storetype PKCS12 -alias meshcrossway-release -keyalg RSA -keysize 3072 -validity 12000 -dname "CN=MeshCrossway Release, OU=Software, O=MeshCrossway"

Use the interactive password prompt. Store the keystore and password in **two separate secure private locations**. Never commit/upload them to a public GitHub repo or share them in issues/chat.

In the repository open Settings > Secrets and variables > Actions > New repository secret. Create these five secrets:

| GitHub secret | Value |
|---|---|
| ANDROID_RELEASE_KEYSTORE_B64 | Base64 text of the private .p12 file |
| ANDROID_RELEASE_STORE_PASSWORD | Keystore password |
| ANDROID_RELEASE_KEY_ALIAS | meshcrossway-release |
| ANDROID_RELEASE_KEY_PASSWORD | Key password (normally same as keystore password for PKCS12) |
| ANDROID_RELEASE_CERT_SHA256 | SHA-256 digest of the signing certificate; colons optional |

Windows PowerShell to encode the .p12 file:

    [Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\private\MeshCrossway-release.p12"))

To get the signer fingerprint (enter the password when requested):

    keytool -list -v -keystore MeshCrossway-release.p12 -alias meshcrossway-release

Publish by running Actions > Android permanent-key release > Run workflow, from the main branch, and entering the version (0.2.3). The workflow refuses publication unless the five secrets exist, the version matches the Gradle app, and the signed APK's SHA-256 signer certificate matches the pinned fingerprint. A release cannot overwrite an existing version tag.

**Never publish a CI debug APK as a public release again.** Keep the .p12 for every future Android version; losing it breaks compatibility.

## IMPORTANT: one-time transition from 0.2.2 and earlier

The original debug signing key is no longer available through the published APK. The **first** change to the new stable signing identity may require uninstalling the old build. No code change can bypass the Android signing check.

**Before uninstalling the existing version**, preserve its data outside the private app folder. Existing Android 0.2.2 can export Settings > Data & backup > Export history JSON, plus individual GPX tracks. The history JSON includes both mesh histories, favorites and local points; it does not include the complete track database or all interface preferences.

For a more complete one-time recovery, the repository includes an optional extraction script that can read the old app's full private databases *while it is installed and still debuggable*. With Python 3.9+, Android Platform Tools, USB debugging enabled and the phone connected:

    python scripts/migrate_android_debug_install.py --output MeshCrossway-complete-migration.zip

The script uses adb run-as locally, force-stops the installed debug app for a consistent snapshot, exports the history/track databases and radios/ui/map preferences, and writes a ZIP that can be imported into 0.2.3. The script never uploads anything. If run-as is blocked or the installed APK is not debuggable, this approach cannot read the data; do not uninstall until you have appropriate backups.

**Verify the ZIP before uninstalling**: it must open and contain manifest.json, settings.json, history.json and tracks.json. Save it in a secure location outside the app. After installing the permanently signed 0.2.3 APK, select Settings > Data & backup > Restore complete backup. It merges records without deleting existing history or tracks. Bluetooth/location permissions from the Android operating system must be granted again.

## Normal upgrades after the permanent-key release

1. Download the official new APK.
2. Tap Install / Update **without uninstalling**. Keep the same signing certificate and increasing versionCode.
3. Existing private SQLite histories, favorites, radio selections, settings, GPS tracks, folders and downloaded MapLibre offline areas remain in the app.
4. If Android unexpectedly requires uninstalling, **stop** and check the signer identity. Do not erase data just to install a new beta.

A full device loss, Clear Data, or uninstall is different from an ordinary update. The new Settings > Data & backup > Export complete backup action creates a portable ZIP for these situations. It is **unencrypted** and may contain private messages and location history, so save it in a protected personal folder. Never post it publicly.

| Data | In-place update with same certificate | Complete ZIP migration |
|---|---|---|
| Both radio histories, chats/channels/nodes | Retained | Included |
| Favorites, pinned/muted, drafts, POIs | Retained | Included |
| Selected radios and reconnect preferences | Retained | Included; OS authorization separate |
| UI and map settings, quick replies | Retained | Included |
| GPS tracks, every recorded point, virtual folders | Retained | Included |
| Offline MapLibre tiles | Retained | Not included; re-download after uninstall |
| Android BLE/location/notification permissions | Generally retained | Not included, grant again |
| Remote Access temporary PIN and browser sessions | Intentionally temporary | Not included |
| Private TLS encryption key in Android Keystore | Retained | Not included, regenerated after uninstall |

Android Auto Backup/cloud is deliberately disabled due to the sensitivity of stored messages and private location history and the potentially very large offline map database. A saved ZIP outside the app is essential for recovery after a full uninstall or damaged/lost phone.

## Test plan

Verify a same-signature APK upgrade retains all databases and preferences; signing digest remains unchanged across releases; ZIP export and import restore every track point and both radio histories; malformed track backups roll back without deleting the existing records; Android API29 and API35 builds/tests pass.

Official sources: https://developer.android.com/studio/publish/app-signing and https://developer.android.com/identity/data/autobackup
