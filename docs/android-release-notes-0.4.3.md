# MeshCrossway Android 0.4.3 beta — MeshCore repeater protocol compatibility

**Experimental field update · Android 0.4.3 (versionCode 37).**

This update follows a real test of signed Android 0.4.2 with two independent radios **Ready**. The user's authorized MeshCore repeater returned a **60-byte binary status reply**, where MeshCrossway had incorrectly required exactly 52 or 56 bytes. The repeater also returned **13 neighbors** (8 in the first page) successfully, but a subsequent 60-byte neighbors reply was rejected.

## Changes

- **Upstream-compatible status decoding:** follow the public, maintained [MeshCore Python](https://github.com/meshcore-dev/meshcore_py/blob/main/src/meshcore/parsing.py) and [MeshCore JavaScript](https://github.com/meshcore-dev/meshcore.js) client decoding approach. Interpret the 52-byte known status fields (and optional 4-byte receive-error field when present), while allowing additional firmware data bytes without inventing their meaning. A 60-byte status response is now accepted and the four unknown extra bytes are reported as safely ignored.
- **Count-driven neighbor-page decoding:** read only the declared number of records, with the requested six-byte identity prefix, 4-byte reported age and quarter-dB SNR. Validate the reported total, page offset, count and minimum data length. Accept up to four bounded extension/padding bytes after a valid page. In particular, a reported final page of 5 entries from 13 neighbors, at offset 8, may occupy **60 bytes**, including one trailing byte. Reject unrelated payloads, malformed headers, truncated records or page-count overflows.
- **Shorter confirmation dialog:** before the one manual request, warn concisely about potential **flood routing/multiple retransmissions**, the 30-second shared per-repeater cooldown and lack of automatic retry.
- **Data provenance:** show a non-sensitive count of additional ignored firmware bytes. Values are historical and are based only on the request's session/tag-correlated response, not a cryptographic attestation of sender identity.

## Safety and limitations

- Requests still require explicit user confirmation; opening a repeater never causes an RF transmission.
- One in-flight remote request, **30-second shared cooldown** per repeater covering status, neighbor pages, reconnect and process restart. No automatic retries or background polling.
- No traffic bridging or forwarding between Meshtastic and MeshCore; two separate compatible BLE radios are required for simultaneous operation.
- The original MeshCore Android client is proprietary. This change follows **public official protocol libraries**, not copied closed-source code.
- Passing Android/emulator fixtures will **not** prove compatibility until repeated with a real repeater. Firmware differences and reply tags may still produce timeout or errors. Do not use this beta as a sole emergency communication system.

## Installation / rollback

- Same app package `com.meshcrossway.mobile`; **versionCode 37** follows 0.4.2's 36. The permanent signing certificate must match earlier signed releases, with GitHub Actions verifying its fingerprint.
- Back up important messages, maps, tracks and settings before updating. Install signed 0.4.3 **over** 0.4.2 without uninstalling or clearing app data.
- Lower versionCode APKs cannot normally be installed over 0.4.3 as direct Android downgrades.
- Public 0.4.2 and 0.4.1 signed releases remain immutable.
- This signed 0.4.3 APK exists **only if** the maintainer explicitly dispatches the guarded permanent-key GitHub release workflow with `version=0.4.3`, after approved PR CI and final baseline validation. Never use a CI debug build as a permanent-signing substitute.

## Targeted field test

1. Install the signed 0.4.3 update on the same Android phone with two compatible radios, both showing **Ready**.
2. Select your owned/authorized MeshCore repeater and request **remote status** once. Check that the former 60-byte rejection becomes actual parsed status data, including battery, uptime, RX/TX and reported RF metrics.
3. Verify the UI labels any four unknown status-extension bytes as ignored, and that the status values appear plausible. If not, report the displayed error and repeater firmware version only; no raw packets/public keys/credentials.
4. After the 30-second limit, request **neighbors**. Verify 13 reported total, eight first-page entries, and the separately requested five-entry final page (offset 8). Check that no unknown trailing byte is presented as a neighbor.
5. Confirm no automatic next-page query, no flood from merely viewing the screen, and no cross-network relaying.
6. Record field results in [issue #239](https://github.com/Renbrant/MeshCrossway/issues/239).

Related: [Stage 2 #229](https://github.com/Renbrant/MeshCrossway/issues/229) and [previous signed Android 0.4.2](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.2).
