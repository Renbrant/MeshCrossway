# Android 0.2.0 — Local HTTPS remote workspace

> Development branch for [PR #191](https://github.com/Renbrant/MeshCrossway/pull/191). The accepted 0.1.1 APK stays available as recovery until phone testing confirms 0.2.0. No cloud backend or Meshtastic-to-MeshCore forwarding.

## Overview

The Android app can serve a private HTTPS workspace over the **same local Wi-Fi network**. It is OFF by default. On Home, tap **Remote Access**, enable the server, then read the address, one-time activation access code and SHA-256 certificate fingerprint on the phone. Open the address in a computer browser. Since the phone uses a self-signed local certificate, verify its SHA-256 fingerprint on the phone before accepting the browser's certificate warning or entering the code.

The server listens on the Wi-Fi IPv4 address (port 8080), not 0.0.0.0. The computer must be able to reach the phone on the same private Wi-Fi; client isolation/firewalls and changing Wi-Fi addresses can prevent access. No LTE/mobile, hotspot mode, port forwarding, NAT traversal, global access or remote cloud relay is provided. Do **not** expose port 8080 to the public internet.

## Three stages in one branch

1. **Secure local access.** Android Keystore RSA keypair and locally generated HTTPS certificate; on-device randomly generated session code, rate-limited login, Secure/HttpOnly/SameSite=Strict cookie, one-hour session expiry, Same-Origin/Host check, CSRF header check for mutations, strict CSP, no-store API responses. The access code is never sent to unauthenticated pages. Stopping the server invalidates sessions and rotates the code on its next activation. The temporary code is *not* a persistent user password.
2. **Messages and contacts.** Browser status for both BLE radios, per-network node/channel/conversation summaries, separately filtered conversation history, remote sending and read markers. Existing `RadioService.send` and `HistoryStore` handle message validation, persistence and delivery semantics; there is no second GATT client. The web response only promises radio submission, not recipient delivery.
3. **Maps and tracks.** List MapLibre offline areas, download a named bounding rectangle to phone storage, resume/pause/delete, and display map source quotas. Tracks have SQLite schema v2 (additive `folder_id` plus `folders` table). The browser can create/rename/delete virtual folders, move saved tracks, set colors/visibility, preview up to 5,000 points, and export GPX to the computer. Deleting a virtual folder moves its tracks to Unfiled, not disk deletion.

## Data model and concurrency

- Remote lifecycle belongs to the existing BLE foreground `RadioService`, and Remote Access has an explicit stop action in the phone. No implicit server startup at boot or automatic cloud synchronization.
- API reads/writes of `RadioService`, `OfflineMaps`, and `TrackService` are marshalled to Android's main looper; the server's socket workers never directly mutate BLE state.
- `HistoryStore` and `TrackStore` keep their existing asynchronous workers. Remote sends are committed before they reach the BLE queue; a timeout means **unknown** and must not cause automatic resend.
- MapLibre's native offline region store is authoritative; the web app does not copy its private cache to a directory. Existing 2,400-tile and 45-MB budgets remain enforced. Browser download requires phone internet access to the selected provider, but already-saved regions and the web UI can be used locally offline.
- Folder migration is additive, and empty folder ID means Unfiled. User message backups do not include tracks; export GPX files before replacing incompatible APK signatures.

## Limitations and test matrix

- The map download UI currently takes **numeric latitude/longitude bounds and maximum zoom**, not a full draggable basemap; previously downloaded area bounds can be loaded into the form. Full PC map selection is a follow-up usability improvement.
- Only conversations associated with a synchronized channel/contact can be addressed; sending is still blocked when the selected radio is disconnected or the native protocol rejects it.
- All track metadata changes are asynchronous; refresh the list after a write.
- Physical tests: phone S25, both Heltec V3 radios connected, computer on same Wi-Fi, cert fingerprint match, login failure lockout, logout/stop invalidation, send+receive on both networks, Wi-Fi network change, screen-off/Doze, offline map creation/cancel/delete, folder migration from a 0.1.1 app database, GPX export and reinstallation.
- Android emulator tests cannot establish long-running background reliability, RF coexistence, or browser trust configuration on user hardware.
- No accessibility claim for TLS trust UX or other Android devices until verified.

## Build / validate

```sh
bash scripts/test_android_probe.sh
cd android
./gradlew assembleDebug lintDebug
./gradlew connectedDebugAndroidTest
```

Package: `com.meshcrossway.mobile`. Version name: `0.2.0`. Version code: `10`.

Dependency: NanoHTTPD 2.3.1 (BSD-3-Clause). The server uses only app-bundled web assets and Android Keystore credentials.

## Security caveat

The browser trust warning for a self-signed certificate is unavoidable without a trusted local CA or hostname and certificate provisioning. Users must manually compare the fingerprint shown by the browser with the fingerprint displayed **on the phone**. Do not suggest disabling certificate validation broadly; do not publish passwords, location histories or private messages in logs or bug reports.
