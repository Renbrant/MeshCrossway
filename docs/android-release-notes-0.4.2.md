# MeshCrossway Android 0.4.2 beta — MeshCore repeater field feedback

**Experimental field-test update · Android 0.4.2, versionCode 36**

This patch responds to the first real-radio MeshCore repeater diagnostic test on Android 0.4.1. Both independent Meshtastic and MeshCore radios reached **Ready** and the app displayed an approved MeshCore repeater. A user-triggered remote status request received a tag-correlated Companion response but showed **"Invalid or unsupported remote response format"**. That does not demonstrate successful status parsing or prove why the payload differed.

## Changes from 0.4.1

- **30-second per-repeater request limit** (previously five minutes) for hands-on tests on repeaters you own or are authorized to query. Status, neighbor requests and additional neighbor pages share the same cooldown. The interval is enforced in memory and in persistent Android preferences across reconnects/restarts.
- **Actionable, privacy-safe response diagnostics**: when a tag-correlated status response does not match the documented 52- or 56-byte repeater status layout, show its **data byte length** and expected lengths instead of a generic error. Unexpected neighbors data shows its length and expected record structure.
- Continue to reject unsupported or malformed responses without inventing battery, packet or neighbor statistics. The app does **not** display raw payloads, packet tags, keys or identifiers in these errors. This update is **not a confirmed fix** for the remote repeater's as-yet-unidentified response format.
- Preserve a single in-flight request per radio, explicit manual confirmation and warning about potential FLOOD routing and RF airtime. There is **no background polling, automatic retry or cross-network relay**.
- Unchanged: dual BLE radios, Meshtastic/MeshCore messages and channels, maps, tracks and locally saved data.

## Field-test instructions

1. Make a secure backup of your data. Install the **permanently signed** `MeshCrossway-0.4.2.apk` over 0.4.1 (or compatible permanent-key earlier builds). Do **not** uninstall or clear storage.
2. Connect **two separate radios**, one with Meshtastic and one with MeshCore; verify both show **Ready**.
3. Open a **repeater you own or have permission to query**, then read the FLOOD airtime warning and tap **Request remote status** once.
4. If the result shows **Received**, inspect whether the values look plausible. If it shows **Error**, report **only** the displayed number of response data bytes (and the MeshCore repeater firmware version); avoid posting full public keys, packet contents or credentials.
5. After 30 seconds, you may test **Request neighbors** once. Check pagination and SNR only if the firmware returns real structured data. No request is transmitted automatically.
6. If you can identify firmware version and access/login state of the repeater, include those facts in the [tracking issue #239](https://github.com/Renbrant/MeshCrossway/issues/239). A successful **local radio acknowledgement** is not proof of an authenticated remote response.

## Upgrade, versioning and rollback

- Android package `com.meshcrossway.mobile`, version `0.4.2`, **versionCode 36**; v0.4.1 used 35, v0.4.0 Stage 1 used 34.
- The original permanent signing certificate remains mandatory; the guarded GitHub workflow checks its SHA-256 fingerprint and refuses to publish with another key. Verify the published `SHA256SUMS.txt` and `SIGNING-CERT-SHA256.txt` assets.
- Signed v0.4.1 and v0.4.0 remain immutable for reference. A normal in-place Android downgrade from 0.4.2 to an older versionCode is not supported. Back up **before** upgrading.
- This release exists **only after** an explicit maintainer `workflow_dispatch` from `baseline/android-0.1.0` with `version=0.4.2` passes and creates the GitHub release. CI debug APKs are not a substitute for signed updates.

## Scope and safety

A 30-second interval is intended for **controlled tests on your own or authorized repeater**, not frequent diagnostics on busy public networks. FLOOD requests and their responses can create multiple LoRa transmissions. Physical compatibility, firmware differences, and end-to-end response provenance require further validation before this feature is considered complete. The beta is **not certified for emergency or life-safety use**.

References: [Stage 2 #229](https://github.com/Renbrant/MeshCrossway/issues/229), [remote repeater diagnostics #239](https://github.com/Renbrant/MeshCrossway/issues/239), [Android 0.4.1 release](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.1).
