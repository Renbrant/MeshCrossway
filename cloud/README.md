# MeshCrossway Cloud — first storage limits

This directory contains the first Cloudflare Worker + D1 + R2 backend slice for optional account sync. It is not deployed or connected to the Android APK yet. The phone app remains local-first; no message or location is uploaded until the native client is connected to an authenticated account-sync flow.

## Initial free-account limits

- Ordinary conversation messages: retain the latest **7 days** on the server.
- Favorite conversations: keep all their messages until the user unfavorites or deletes the account.
- Tracks: at most **5 tracks**, **5 MiB per GPX**, and **25 MiB total** per account.
- Oversize tracks stay complete on the phone and are not accepted by this cloud endpoint. A future client can offer a simplified/resampled copy for cloud storage while preserving the original local track.
- Meshtastic and MeshCore values stay in separate records. The Worker does not connect to either radio and cannot relay between the networks.

The 5 MiB / five-track cap is a deliberately small starting proposal. It can be revised before deploying or exposing the feature to users. Quotas are checked by the server and also protected by SQLite triggers; a modified APK cannot increase the limit.

## API implemented

All `/v1/*` endpoints require a Clerk session JWT in `Authorization: Bearer <token>`. The Worker verifies the RS256 signature using the Clerk JWKS, checks `iss` and expiry, and scopes every query and R2 object to the verified Clerk `sub`.

| Route | Behavior |
|---|---|
| `GET /health` | Public health check; no account data |
| `GET /v1/messages?cursor=<opaque-cursor>` | Returns messages inside seven-day retention and older messages from favorite conversations; pages contain up to 1,000 rows |
| `POST /v1/messages` | Upserts up to 100 messages per batch; ignores expired non-favorite messages |
| `GET /v1/favorites` | Lists favorite conversations |
| `PUT /v1/favorites` | Body: `{ "network": "MESHTASTIC"|"MESHCORE", "conversationId": "…", "favorite": true|false }` |
| `GET /v1/tracks` | Lists the user's tracks and current quota usage |
| `PUT /v1/tracks/{trackId}?name=…` | Uploads one GPX as `application/gpx+xml`; enforces file, count and total caps |
| `GET /v1/tracks/{trackId}` | Downloads one of the authenticated user's GPX files |
| `DELETE /v1/tracks/{trackId}` | Deletes the user's track and R2 object |

Messages use Unix timestamps in seconds, matching the Android app's current local history. Request batches are bounded. The daily Worker cron removes expired non-favorite messages; writes also prune expired messages for that user. Use the returned `nextCursor` to fetch another page; send no cursor for the first page.

## Provisioning before use

1. Create a Cloudflare D1 database named `meshcrossway-cloud` and an R2 bucket named `meshcrossway-user-data`.
2. Replace `database_id` in `wrangler.toml` with the D1 ID. Set `CLERK_ISSUER` to the exact issuer for the Clerk application. Configure `CLERK_JWKS_URL` only if the issuer's standard JWKS URL is not correct.
3. Set `WEB_ORIGIN` to the eventual account-site origin. Native Android calls omit `Origin`; browser calls from other origins are rejected.
4. Apply `migrations/0001_initial.sql` with Wrangler, then deploy the Worker.
5. The Android source branch has the first Clerk native sign-in slice, but the authenticated sync client is not connected yet. Do not enable uploads until the API is deployed and the app's sync client is implemented. Do not add service secrets or private credentials to this repository.

## Privacy and current scope

The API accepts message text in plaintext after the app has decrypted/decoded the radio payload. This first backend slice does not yet provide client-side end-to-end encryption. The app must disclose cloud storage clearly and require the user to enable it before upload. Authentication, per-user authorization, HTTPS, bounded input and account-scoped object keys are included, but production deployment should wait until account deletion, data export, recovery and client-side encryption decisions are completed.

This slice does not include billing, account-site UI, Clerk application credentials, or Android upload/download integration. The current public APK does not contain the Clerk changes, and no network data is uploaded by it.
