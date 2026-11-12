# Android Stage 1 — signal freshness and reception provenance

Tracking: [#229](https://github.com/Renbrant/MeshCrossway/issues/229), Stage 1. This is **the first implementation slice**, not the completion of Stage 1 and not a released APK.

## Baseline review (Android 0.3.12)

- Independent BLE radio sessions already record measured RSSI every approximately 20 seconds while GATT is idle.
- Received messages already store LoRa metadata (RSSI/SNR/hops where reported) in `rx_meta` alongside protocol/radio-scoped SQLite history; incoming MeshCore verified channel-log copies can enrich full observed path hashes.
- Messages already track `queued`, `submitted`, `radio accepted`, `delivered`, `rejected`, and `uncertain`. The app does **not** automatically resend uncertain submissions. The service marks pending messages uncertain on a disconnect and on a cold start.
- Channel configuration and QR import/export are present in 0.3.12 and are explicitly **not** changed by this slice.

## Implemented here

1. **Freshness for Bluetooth RSSI.** A BLE RSSI sample is considered *current* only if the session is verified, the GATT reading belongs to the current link, and its recorded wall-clock time is not older than 90 seconds. A still-cached sample is shown as **Last BLE** with age, not as live signal. Missing/future/>24-hour samples are shown as unavailable. The existing independent polling, storage, and retry behavior remain unchanged.
2. **Automatic display aging.** The Home refresh fingerprint now incorporates the reading timestamp and elapsed minute bucket, so the 30-second Home refresh timer can update the shown age and stale/current state without any additional GATT request.
3. **LoRa provenance.** Message details now identify whether the information is Meshtastic companion packet metadata, a MeshCore companion report, or a validated observed MeshCore channel copy. A Meshtastic MQTT flag is explicitly *not* proof of RF reception. No hop count is treated as proof of a full path.
4. **Source caveat.** The details sheet states that stored metrics describe historical packet metadata, **not** live reachability or a guaranteed end-to-end RF measurement.
5. **Unit regression tests.** Tests cover valid/future/stale BLE samples, reconnect/sample ownership flags, age labels, full-path provenance, MQTT ambiguity, and MeshCore direct routing.

This slice does **not** add an RF history database/chart or change message delivery semantics. Those are later independent Stage 1 deliverables, after a capability inventory and reliable real-radio test vectors.

## Developer validation

```sh
cd android
./gradlew testDebugUnitTest lintDebug assembleDebug
```

Also run the repository's existing protocol fixtures and Android instrumentation (API 29/35) in CI. For manual acceptance, use two radios (Meshtastic and MeshCore) with independent BLE connections:

- Verify fresh RSSI is shown as **BLE** for each radio after a successful poll.
- Keep one radio busy until RSSI polls cannot complete for over 90 seconds: its reading becomes **Last BLE** with an age while the other radio remains current. No BLE reconnection, message resend, or extra RF packet should occur.
- Disconnect/reconnect each radio and confirm that the previous RSSI is not labeled live before a new successful reading for the current link.
- Inspect received MT, MC direct, and MC channel messages with and without observed channel logs; distinguish metadata, validated full-path copy, and unknown values.
- Verify the existing channels/QR workflow, message sending, saved maps, tracks, Home/navigation, and local browser workspace.

**No change to versionName, versionCode, the permanent signing certificate, or the published 0.3.12 APK.** This PR should target the active Android release branch rather than the lagging `main` sources.
