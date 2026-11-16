# Google Play launch guide

This guide takes MeshCrossway from GitHub-only APKs to a Google Play listing. The
repository changes are already done; the steps marked **(you)** happen in Play
Console, Clerk, Cloudflare or GitHub settings and cannot be automated from the repo.

## What the repository now provides

| Play requirement | Where it is handled |
|---|---|
| Target API level 36 (required for new apps and updates since Aug 31, 2026) | `android/app/build.gradle` (`targetSdk 36`, version 0.4.9 / code 43) |
| Android App Bundle (.aab) upload format | `bundleRelease`; `.github/workflows/android-play-release.yml` |
| Same permanent signing certificate as GitHub APKs | Play workflow verifies the AAB certificate against `ANDROID_RELEASE_CERT_SHA256` |
| In-app account deletion | Account & sync → *Delete account and cloud data* (`ClerkAccountActivity.kt`, `CloudSyncClient.deleteAccount`) |
| Web account deletion resource | https://meshcrossway.renbrant.com/delete-account/ and the account page |
| Server-side deletion | `DELETE /v1/account` in `cloud/src/index.ts` (D1 rows, R2 GPX objects, Clerk user) |
| Privacy policy URL, linked inside the app | https://meshcrossway.renbrant.com/privacy/ (`docs/site/privacy/`) |
| Store listing text and graphics (en-US, pt-BR) | `fastlane/metadata/android/` |

## 1. Before the first upload

1. **(you) Merge to `main`** so the website deploys `/privacy/` and `/delete-account/`.
   Check both URLs open before submitting the listing.
2. **(you) Move Clerk to a production instance.** The app and Worker still use the
   development instance (`humorous-boxer-8586.clerk.accounts.dev`, `pk_test_…`).
   Development instances are capped in users and show a development banner, so they
   are not suitable for a public store release. In Clerk:
   - Create the production instance and its domain (e.g. `clerk.meshcrossway.renbrant.com`).
   - Configure the same sign-in providers, the Android app (package
     `com.meshcrossway.mobile`, permanent certificate SHA-256) and the account site
     origin `https://meshcrossway.renbrant.com`.
   - Update GitHub repository variable `CLERK_PUBLISHABLE_KEY` to the `pk_live_…` key,
     `CLERK_ISSUER` in `cloud/wrangler.toml`, and `docs/site/account/config.js`.
   - Existing test accounts do not move automatically between instances.
3. **(you) Enable identity deletion in the Worker.** Without this secret, account
   deletion removes all cloud data but leaves the Clerk sign-in identity:
   ```bash
   cd cloud && npx wrangler secret put CLERK_SECRET_KEY
   ```
   Then deploy the Worker (`cloud-deploy.yml` or `npx wrangler deploy`).
4. **(you) Field-test 0.4.9** on real radios as for every release, including
   *Delete account and cloud data* with a throwaway account, and confirm the
   Account & sync screen, edge-to-edge layout and back navigation on Android 16.

## 2. Play Console account and app

1. **(you)** Create a Google Play developer account (one-time US$25 fee, identity
   verification). Choose *personal* or *organization*.
   - **Personal accounts created after Nov 13, 2023 must run a closed test with at
     least 12 testers opted in for 14 consecutive days** before production access is
     granted. Recruit the current beta testers early. Organization accounts (D-U-N-S
     number required) skip this.
2. **(you)** Create the app: name *MeshCrossway*, default language *English (United
   States)*, type *App*, *Free*. Free apps cannot later become paid.

## 3. App signing — important

GitHub APK users and Play users must be able to update from each other. Android only
allows that if both builds have the **same signing certificate**. When Play Console
asks how to sign the app:

1. Choose **Use existing app signing key** → *Export and upload a key from Java
   keystore*, and follow Google's PEPK tool instructions with the permanent
   keystore (the same one stored in `ANDROID_RELEASE_KEYSTORE_B64`). Do this on your
   own computer; never upload the keystore anywhere else.
2. Register the same certificate as the **upload key**, or create a separate upload
   key later. The Play workflow signs with the permanent key, so using it as the
   upload key needs no extra secrets.

If you let Google generate a new app signing key instead, Play installs and GitHub
APKs become incompatible: users would need to uninstall (losing local data unless
backed up) to switch. This choice cannot be undone after the first release.

## 4. Build and upload the bundle

1. Run **Actions → Android Google Play bundle → Run workflow** on `main` with
   version `0.4.9`. It runs unit tests and lint, builds a signed `.aab`, verifies its
   certificate and target SDK, and attaches it as the artifact
   `MeshCrossway-0.4.9-play-bundle`.
2. **(you)** First upload is manual: Play Console → *Testing → Internal testing*
   (then *Closed testing*) → *Create release* → upload the `.aab` from the artifact.
3. Optional automation for later releases: create a Google Cloud service account,
   grant it *Release to testing tracks* (and *production* if wanted) in Play Console
   → *Users and permissions*, and save its JSON key as GitHub secret
   `PLAY_SERVICE_ACCOUNT_JSON`. The workflow then uploads to the chosen track (as a
   draft by default).
4. Keep publishing GitHub APKs with `android-permanent-release.yml`. Both workflows
   use the same `versionCode`, so bump it once per release.

## 5. Store listing

Copy from `fastlane/metadata/android/<locale>/` (or upload with `fastlane supply`):

| Field | File | Limit |
|---|---|---|
| App name | `title.txt` | 30 |
| Short description | `short_description.txt` | 80 |
| Full description | `full_description.txt` | 4000 |
| App icon 512×512 | `en-US/images/icon.png` | 32-bit PNG |
| Feature graphic 1024×500 | `en-US/images/featureGraphic.png` | |
| Phone screenshots (5, 1080×1920) | `en-US/images/phoneScreenshots/` | 2–8 |
| Release notes | `changelogs/default.txt` | 500 |

Screenshots are the November 15, 2026 app captures padded to 9:16 (the originals
exceed Play's 2:1 maximum ratio). Add 7-inch/10-inch tablet screenshots only if you
want the app featured for tablets.

- **Category:** Communication. **Tags:** e.g. Messaging, Maps & Navigation.
- **Contact details:** an email address you monitor (required, shown publicly) and
  website `https://meshcrossway.renbrant.com/`.
- **Privacy policy:** `https://meshcrossway.renbrant.com/privacy/`.

## 6. App content declarations

**Privacy policy:** URL above.

**Ads:** No.

**App access:** All functionality is available without special access. Suggested
reviewer note: *"The app connects to user-owned Meshtastic and MeshCore LoRa radios
over Bluetooth LE. Without radios the app opens normally and shows the Home, Chats,
Map, Contacts and Settings screens with empty data; map browsing, offline map
downloads, GPS tracks and Remote Access work without radios. An account is optional
(Account & sync) and only enables cloud sync."*

**Content rating (IARC questionnaire):** category *Communication / social*; no
violence, sexual content, gambling or drugs. Answer *yes* to "users can interact or
exchange content" (users can message each other over the radio networks) and *no* to
sharing location with other users from the app (the phone's location is never sent
over the mesh). Answer *no* to digital purchases.

**Target audience:** 18 and over (recommended: the app is designed for radio
hobbyists and is not intended for children; this also keeps it out of the Families
policy scope).

**News app:** No. **Government app:** No. **Financial features:** none. **Health:** No.

**Foreground service permissions** (Play Console → *App content → Foreground service
permissions*), each needs a short description and a demo video link (e.g. unlisted
YouTube):

| Type | Declared for | What to show in the video |
|---|---|---|
| `connectedDevice` (`RadioService`) | Keeps the Bluetooth LE connections to the user's Meshtastic and MeshCore radios alive so incoming mesh messages are received and notified while the app is in the background. | Connect a radio, leave the app, receive a message notification, show the ongoing notification. |
| `location` (`TrackService`) | Records a GPS track the user explicitly starts, continuing while the screen is off until the user stops it. | Start a track recording from the map, lock the screen, walk/drive, stop recording and show the track. |

**Location permission:** foreground only (`ACCESS_FINE_LOCATION`; no
`ACCESS_BACKGROUND_LOCATION`), so no background-location declaration is required.

**Data safety form** (only data that leaves the device counts; everything processed
only on the phone is not "collected"):

| Question | Answer |
|---|---|
| Does the app collect or share user data? | Yes (only with the optional account) |
| Is all collected data encrypted in transit? | Yes |
| Can users request data deletion? | Yes — `https://meshcrossway.renbrant.com/delete-account/` |
| Personal info → Email address | Collected, not shared, **optional**; purpose: *Account management* |
| Personal info → User IDs | Collected (Clerk user ID), not shared, optional; *Account management* |
| Messages → Other in-app messages | Collected, not shared, optional (cloud sync opt-in); *App functionality* |
| Location → Precise location | Collected, not shared, optional (GPX track sync opt-in); *App functionality* |
| Everything else (contacts, photos, files, app activity, device IDs, diagnostics, financial, health) | Not collected |

Clerk and Cloudflare act as service providers processing data on MeshCrossway's
behalf, so their processing is not "sharing". Map tile requests to OpenFreeMap and
OpenStreetMap transmit only what any web request does (IP address, requested map
area) and no user data. No data is used for advertising or analytics, and none is
processed ephemerally only.

## 7. Testing tracks to production

1. **Internal testing** — up to 100 testers, available within minutes; use it to check
   the Play-installed build updates over a GitHub APK and vice versa.
2. **Closed testing** — at least 12 opted-in testers for 14 days (personal accounts).
   Share the opt-in link with beta users; ask them to keep the app installed.
3. Review the **pre-launch report** (automated device tests) and any 16 KB page-size
   or policy warnings in Play Console → *Release → App bundle explorer*.
4. **(you)** Apply for production access (personal accounts answer a short
   questionnaire about the closed test), then create a production release, ideally
   with a staged rollout (e.g. 20%).
5. After approval, add the Play badge/link to `README.md` and the website. Keep the
   APK download for users without Google Play.

## Release checklist for future versions

- [ ] Bump `versionCode` and `versionName` in `android/app/build.gradle`.
- [ ] Add `docs/android-release-notes-<version>.md` and update
      `fastlane/metadata/android/*/changelogs/default.txt` (≤500 characters).
- [ ] Field-test with real radios.
- [ ] Run *Android permanent-key release* (GitHub APK) and *Android Google Play bundle*.
- [ ] Promote the Play release from internal → closed/production.
- [ ] Keep `targetSdk` within Google's yearly requirement (a new level is usually
      required every August 31).
