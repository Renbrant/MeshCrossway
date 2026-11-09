# Android 0.2.0 — Local HTTPS remote workspace

> Development branch for [PR #191](https://github.com/Renbrant/MeshCrossway/pull/191). The accepted 0.1.1 APK stays available as recovery until phone testing confirms 0.2.0. No cloud backend or Meshtastic-to-MeshCore forwarding.

## Overview

The Android app can serve a private HTTPS workspace over the **same local Wi-Fi network**. It is OFF by default. On Home, tap **Remote Access**, enable the server, then read the address, numeric access PIN (4–8 digits, with a suggested six-digit PIN) and SHA-256 certificate fingerprint on the phone. Open the address in a computer browser. Since the phone uses a self-signed local certificate, verify its SHA-256 fingerprint on the phone before accepting the browser's certificate warning or entering the code.

The private TLS key is generated in the app and stored at rest only as Android Keystore AES-GCM authenticated ciphertext; the Keystore AES key is non-exportable. The server listens on the Wi-Fi IPv4 address (port 8080), not 0.0.0.0. The computer must be able to reach the phone on the same private Wi-Fi; client isolation/firewalls and changing Wi-Fi addresses can prevent access. No LTE/mobile, hotspot mode, port forwarding, NAT traversal, global access or remote cloud relay is provided. Do **not** expose port 8080 to the public internet.

## Three stages in one branch

1. **Secure local access.** TLS RSA identity stored encrypted with a non-exportable Android Keystore AES-256-GCM key and locally generated HTTPS certificate; on-device custom 4–8 digit PIN or automatically generated six-digit numeric PIN, rate-limited login, Secure/HttpOnly/SameSite=Strict cookie, one-hour session expiry, Same-Origin/Host check, CSRF header check for mutations, strict CSP, no-store API responses. The numeric PIN is never sent to unauthenticated pages. It is entered only on the phone and again in the authenticated HTTPS browser login. PIN changes immediately invalidate existing browser sessions; PINs are not written to the local database and disappear on server stop. Stopping the server invalidates sessions; on the next activation, the phone suggests a new six-digit PIN and the user may edit it. The temporary PIN is *not* a persistent user password; it is chosen on the phone and stored in process memory only. Short PINs are easier to enter but less resistant to guessing. The login retains a five-attempt lockout and local Wi-Fi binding.
2. **Messages and contacts.** Browser status for both BLE radios, per-network node/channel/conversation summaries, separately filtered conversation history, remote sending and read markers. Existing `RadioService.send` and `HistoryStore` handle message validation, persistence and delivery semantics; there is no second GATT client. The web response only promises radio submission, not recipient delivery.
3. **Maps and tracks.** List MapLibre offline areas, download a named bounding rectangle to phone storage, resume/pause/delete, and display map source quotas. Tracks have SQLite schema v2 (additive `folder_id` plus `folders` table). The browser can create/rename/delete virtual folders, move saved tracks, set colors/visibility, preview up to 5,000 points, and export GPX to the computer. Deleting a virtual folder moves its tracks to Unfiled, not disk deletion.

## Browser usability changes in Android 0.2.2

**Messages now has three distinct tabs**, independently for Meshtastic and MeshCore:

- **Chats:** private/direct conversations, sorted by unread and recent activity. Select an entry to read, reply, and mark it as read. No existing chats? The empty state leads to Nodes.
- **Channels:** enabled and disabled radio channels are listed even without prior messages; enabled channels open an empty conversation ready to send. MeshCore group conversations not represented by a configured channel slot are also listed.
- **Nodes:** all synchronized nodes, with their messageability state and last-heard time. Selecting a messageable node can start a private chat even if no messages exist. It uses the native per-radio Target and send queue; no cross-network routing.

**Offline Maps now uses a visual selection:** pan/zoom a live map, click **Draw area**, drag a rectangle, choose **Offline detail (max zoom)** and a name, then click **Download area to phone**. The browser computes the rectangle automatically, previews estimated tiles, and disables downloads above the phone's 2,400-tile limit. The native server revalidates all coordinates and limits before creating a MapLibre offline region.

Saved regions appear as cyan outlines on the map. **Show on map** zooms to the saved area, and the existing pause/resume/delete actions remain available. **Browser location** requires explicit permission on the computer; it never automatically transmits the phone's GPS.

The interactive background uses *only the OpenStreetMap raster tiles currently displayed in the browser*. Those raster tiles are **not** downloaded for offline use: the actual region download continues exclusively through Android's **OpenFreeMap / MapLibre** source, within its provider quotas. The computer needs internet for the map preview and the phone needs internet to download new offline regions; existing downloaded regions remain on the phone. The map visibly credits OpenStreetMap contributors, uses the browser's normal tile cache, and sends the browser origin as Referer for provider compliance. No message content, radio identity, or login PIN is included in tile URLs.

References:
- [OpenStreetMap raster tile policy](https://operations.osmfoundation.org/policies/tiles/) (interactive browsing is permitted; bulk/offline downloading from OSM's standard tile server is not)
- [OpenFreeMap quick start and offline limits](https://openfreemap.org/quick_start/)

## Data model and concurrency

- Remote lifecycle belongs to the existing BLE foreground `RadioService`, and Remote Access has an explicit stop action in the phone. No implicit server startup at boot or automatic cloud synchronization.
- API reads/writes of `RadioService`, `OfflineMaps`, and `TrackService` are marshalled to Android's main looper; the server's socket workers never directly mutate BLE state.
- `HistoryStore` and `TrackStore` keep their existing asynchronous workers. Remote sends are committed before they reach the BLE queue; a timeout means **unknown** and must not cause automatic resend.
- MapLibre's native offline region store is authoritative; the web app does not copy its private cache to a directory. Existing 2,400-tile and 45-MB budgets remain enforced. Browser download requires phone internet access to the selected provider, but already-saved regions and the web UI can be used locally offline.
- Folder migration is additive, and empty folder ID means Unfiled. User message backups do not include tracks; export GPX files before replacing incompatible APK signatures.

## Limitations and test matrix

- The remote map download UI now uses a draggable live preview and rectangle selection. Preview tiles come from the browser's online OSM raster provider; offline region downloads use the phone's OpenFreeMap MapLibre SDK. A blank preview when the PC is offline does not mean the phone's saved map data was deleted.
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
