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

Publish by running Actions > Android permanent-key release > Run workflow, selecting the **baseline/android-0.1.0** branch (this is the Android app source branch; `main` hosts the site), and entering the version (0.2.3). The workflow refuses publication unless the five secrets exist, the version matches the Gradle app, and the signed APK's SHA-256 signer certificate matches the pinned fingerprint. A release cannot overwrite an existing version tag.

**Never publish a CI debug APK as a public release again.** Keep the .p12 for every future Android version; losing it breaks compatibility.

## IMPORTANT: one-time transition from 0.2.2 and earlier

The original debug signing key is no longer available through the published APK. The **first** change to the new stable signing identity may require uninstalling the old build. No code change can bypass the Android signing check.

**Before uninstalling the existing version**, preserve its data outside the private app folder. Existing Android 0.2.2 can export Settings > Data & backup > Export history JSON, plus individual GPX tracks. The history JSON includes both mesh histories, favorites and local points; it does not include the complete track database or all interface preferences.

## Easiest ADB backup on Windows (Galaxy S25 and other Android devices)

**You can do this while version 0.2.2 is still installed. Do not uninstall it.** This is a one-time backup of the *installed app*, not a new APK installation.

1. On the Windows PC, install [official Android Platform Tools](https://developer.android.com/tools/releases/platform-tools) (the `adb.exe` utility) and [Python 3](https://www.python.org/downloads/windows/). Add ADB to PATH, or put the official `platform-tools` directory directly inside your local MeshCrossway project folder.
2. On the phone, enable **Developer options** (Settings → About phone → Software information → tap Build number seven times) and then **USB debugging** in Developer options. Connect the phone to the PC by USB, unlock it, and approve the computer's debugging authorization.
3. Download/clone this repository's **Android source branch** `baseline/android-0.1.0`. In Windows Explorer double-click `scripts/MeshCrossway-Backup-Windows.cmd`. The batch launcher detects Python and ADB; no command-line typing or Android root access is necessary.
4. The utility first verifies the device connection and access to the *debuggable* MeshCrossway package `com.meshcrossway.mobile`. Only then does it temporarily **force-stop** the application to snapshot its SQLite databases and preferences. It **never** uninstalls, wipes, installs or sends data to any server. Your radios may disconnect until the next time you open the app.
5. A timestamped file, such as `MeshCrossway-migration-20261109-190000-UTC.zip`, is written in **`%USERPROFILE%\MeshCrossway-Backups`**. The script checks SQLite integrity, counts records, validates ZIP CRCs, manifests, settings and backup schemas, and prints the ZIP's SHA-256. It never prints private message content or locations.
6. **Verify and keep a second copy** of this ZIP outside the app and preferably on another local drive. You can verify a saved file again by opening Command Prompt in the repository and running:

       scripts\MeshCrossway-Backup-Windows.cmd verify "C:\Users\YourUser\MeshCrossway-Backups\YOUR-BACKUP.zip"

   Or independently using:

       python scripts/migrate_android_debug_install.py --verify-only "C:\path\backup.zip"

7. Once a **permanently signed** Android 0.2.3 release is actually published, you can install it and use **Settings → Data & backup → Restore complete backup** to import the ZIP. **Do not uninstall the installed app until the backup is saved, inspected and your migration path is confirmed.** The old/new signature mismatch still makes this a one-time migration.

The script is fully offline and does not require internet after Python and Platform Tools have been installed. It never uploads contacts or location. If ADB reports `run-as: Package is not debuggable`, the installed APK is not an accessible debug build; no script can bypass Android security safely. Use the old app's **Export history JSON** and individual **Export GPX** functions instead, and keep the old installation until sufficient backups exist.

The ZIP is **unencrypted**. It includes private chats, GPS tracks and device preferences, and should never be attached to a public GitHub issue or uploaded to a public service. The tool does not back up downloaded MapLibre tiles, Android OS permission grants, hardware Bluetooth pairings or the Android Keystore. Subsequent same-signature updates preserve the app's private tiles, but a full uninstall requires downloading offline regions again.

**Limits of verification:** passing ZIP/SQLite checks does not guarantee that every record will import. The final acceptance check is to restore it and compare message, favorites and track counts. The script itself has no automatic uninstall/restore step by design.

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
