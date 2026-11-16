# Google Play release

Maintainer checklist for publishing MeshCrossway Android on Google Play. The code-side requirements (target SDK, AAB, 16 KB alignment, account deletion, privacy policy) live in this repository; the remaining items are done in Play Console, Clerk and Cloudflare.

## Already handled in the code

| Requirement | Where |
|---|---|
| Target Android 16 (API 36) for new apps and updates | `android/app/build.gradle` (`targetSdk 36`) |
| Edge-to-edge layouts (enforced for apps targeting API 35+) | `MainActivity` and `ClerkAccountActivity` pad content by system-bar insets |
| Android App Bundle (AAB) | The release workflow builds `bundleRelease` and keeps `MeshCrossway-<version>.aab` as a private 30-day workflow artifact |
| 16 KB memory page support | `scripts/check_android_16kb.sh` fails CI if any 64-bit native library is not 16 KB aligned |
| In-app account deletion | App: **Account & sync → Delete account** (calls `DELETE /v1/account`) |
| Web account deletion | `https://meshcrossway.renbrant.com/delete-account/` and the account page's **Delete account** section |
| Privacy policy | `https://meshcrossway.renbrant.com/privacy/`, linked from **Settings → About** and **Account & sync** |

## 1. Signing: keep one certificate for GitHub and Play

Use **Play App Signing with your existing permanent key** (Play Console → *Test and release → App integrity → App signing → Use a different key / Export and upload a key from Java keystore*). Play gives you the `pepk` tool and an encryption key; upload the encrypted `MeshCrossway-release.p12` private key there.

Do **not** let Play generate a new app signing key: GitHub APK users would then need to uninstall (losing local data) to switch to the Play version.

Upload key: Play accepts the app signing key as the upload key, and the workflow signs the AAB with it by default. For better security, create a separate upload key and add these GitHub secrets; the workflow will use it automatically:

| GitHub secret | Value |
|---|---|
| `ANDROID_UPLOAD_KEYSTORE_B64` | Base64 of the upload `.p12` |
| `ANDROID_UPLOAD_STORE_PASSWORD` | Upload keystore password |
| `ANDROID_UPLOAD_KEY_ALIAS` | Upload key alias |
| `ANDROID_UPLOAD_KEY_PASSWORD` | Upload key password |

Register the upload certificate in Play Console (*App integrity → Upload key certificate*).

Registering the app in Play Console also completes Android developer verification for the package and its signing key, which covers the GitHub APK too.

## 2. Cloud and Clerk production

1. Create the Clerk **production** instance. Enable Google (and Apple if wanted). Register the Android app (`com.meshcrossway.mobile`) with the **SHA-256/SHA-1 of the permanent app signing key**, which is the same key Play uses.
2. In Google Cloud, add the same signing-certificate SHA-1 to the Android OAuth client.
3. Set the GitHub repository variable `CLERK_PUBLISHABLE_KEY` to `pk_live_…`.
4. In `cloud/wrangler.toml`, set `CLERK_ISSUER` to the production issuer. Then run `npx wrangler secret put CLERK_SECRET_KEY` with the matching `sk_live_…` key. Account deletion returns `ACCOUNT_DELETION_NOT_CONFIGURED` until this secret exists.
5. Deploy the Worker (merging to `main` runs **Deploy cloud Worker**), then test deletion with a throwaway account on both the app and the website.

## 3. Build the bundle

Run **Android permanent-key release** on `main` with the new version and an increased `versionCode`. Download the `meshcrossway-play-bundle-<version>` artifact from the workflow run and upload the `.aab` to Play Console.

## 4. Play Console forms (App content)

- **Privacy policy:** `https://meshcrossway.renbrant.com/privacy/`
- **App access:** most features need a Meshtastic/MeshCore BLE radio, and an account is optional. Explain this, and attach a short video showing radios connected, messaging, map and track recording.
- **Ads:** no ads.
- **Data safety.** Suggested answers, based on the current code:
  - Collected only with an optional account or sync: email address, name and user IDs (Clerk account); messages (cloud sync); precise location (only GPX tracks the user syncs).
  - Purpose: app functionality and account management. Shared with third parties: no. These are service providers (Clerk, Cloudflare).
  - Encrypted in transit: yes. Users can request deletion: yes, with the URL `https://meshcrossway.renbrant.com/delete-account/`.
  - Not collected: location used only on the device, camera images (QR scanning on the device), Bluetooth data and local messages.
- **Foreground service permissions:**
  - `FOREGROUND_SERVICE_CONNECTED_DEVICE`: keeps the Bluetooth link to the user's radios while they receive messages.
  - `FOREGROUND_SERVICE_LOCATION`: user-started GPS track recording, with a visible notification and Pause action.
  
  Provide a video of each.
- **Content rating (IARC):** users can communicate with each other (mesh messaging) and the app shares the user's location if they choose to.
- **Target audience:** 13+ or 18+. Not designed for children.
- **Account deletion:** in-app path plus the web URL above.

## 5. Store listing

- App icon 512×512 PNG; a 1024×1024 source exists at `android/app/src/main/res/drawable-nodpi/meshcrossway_app_icon.png`.
- Feature graphic 1024×500.
- At least 2 phone screenshots. Current app screenshots are in `docs/site/media/android-2026-11-15/`.
- Short description (≤80 characters) and full description. State clearly that compatible Meshtastic and/or MeshCore Bluetooth radios are required and that the app is not an emergency communicator.

## 6. Testing track (new personal developer accounts)

Personal Play accounts created after November 13, 2023 must run a **closed test with at least 12 testers opted in for 14 consecutive days** before applying for production access. Recruit 15–20 testers so drop-offs don't reset the clock. Use internal testing for quick iteration, since it doesn't count toward the requirement.
