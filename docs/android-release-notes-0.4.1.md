# MeshCrossway Android 0.4.1 beta — Stage 2 field testing

**Experimental beta for real-device validation · Android 0.4.1 (versionCode 35)**

This update lets you explore network diagnostics and MeshCore repeater information using your phone and **two separate BLE radios**, one running Meshtastic and one running MeshCore. **The networks remain isolated: no bridging or forwarding.**

## What's new

- **Advanced network diagnostics:** Settings → Advanced network diagnostics contains independent Meshtastic and MeshCore status panels. They display the actual Bluetooth MTU and received GATT frame count, plus recently saved incoming/outgoing messages and available LoRa RSSI/SNR metadata. These are *not* total LoRa traffic counters or spectrum-analyzer readings.
- **MeshCore repeater inventory:** browse saved repeater contacts from the current MeshCore radio, with contact identity, local-observation freshness, and cached position when available. Cached activity **does not prove current repeater connectivity**.
- **On-demand remote repeater status (experimental):** choose a currently approved repeater and explicitly request uptime, battery/power information, queue depth, noise floor, last RSSI/SNR and packet counters *when returned by a supported firmware*. A command accepted by your local radio is **not** confirmation that the repeater responded.
- **On-demand repeater neighbors (experimental):** inspect up to eight reported neighbor entries per request and request additional pages separately. Short public-key prefix matches are presented as ambiguous unless exactly one saved contact matches.
- **Network airtime protection:** every status request and neighbor page requires confirmation, warns that **flood routing may cause many relay transmissions**, and shares a strict **five-minute cooldown per repeater**. This cooldown survives BLE reconnects and app restarts. Nothing is polled or retried automatically.
- **Cleaner Home outbox indicators:** old, uncertain group broadcasts are retained in Chats → Outbox but are no longer emphasized as urgent Home alerts; Home highlights recent actionable send states.

## Experimental feature limitations

**Remote MeshCore status and neighbors have passed automated protocol and Android simulator tests but have not yet been confirmed against your specific physical MeshCore repeater hardware.** A request can time out, be unsupported by a radio or repeater firmware, or traverse several LoRa hops. Treat observed values as historical after receipt. The app shows the local radio's reported flood/known-path flag only *after* acknowledgement; this does not measure total RF airtime.

These tools are intended for experimentation, not high-rate monitoring. Do not repeatedly query busy public networks or rely on the results for emergency/life-safety decisions.

## Safe upgrade and rollback

- Package: `com.meshcrossway.mobile`; Android version **0.4.1**, **versionCode 35**.
- Permanent signing certificate **must match** prior 0.4.0; GitHub Actions verifies the configured expected SHA-256 fingerprint and refuses publishing if it changes.
- Install the signed `MeshCrossway-0.4.1.apk` **over** the existing signed 0.4.0 without uninstalling or clearing data. Preserve message history, maps, tracks, channel keys and settings.
- Use the included `SHA256SUMS.txt` to verify APK integrity.
- The published 0.4.0 beta at [android-v0.4.0-stage1](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.0-stage1) remains unchanged. **Android does not normally install a lower versionCode over a higher one**; reverting from 0.4.1 to 0.4.0 may require a separate compatible higher-code rollback APK. Export and securely back up your app data **before updating**.
- This 0.4.1 release is only available **after** a maintainer explicitly runs the guarded permanent-key release workflow with version `0.4.1`. Do not mistake CI debug artifacts for signed, updatable APKs.

## Field-test procedure

1. Install 0.4.1, open Home and verify separate Meshtastic and MeshCore BLE connections both reach **Ready**.
2. Confirm chats, channels, maps, GPS tracks and messages remain available from the previous installation.
3. Open **Settings → Advanced network diagnostics**, check each radio's information separately, and select a saved MeshCore repeater with current approval.
4. Tap **Request remote status**. Read the flood warning and confirm only if you have permission to use the repeater/network. Verify whether the result is a real remote response, an unsupported status, or a timeout.
5. Confirm the **five-minute cooldown** blocks immediate **Request neighbors** and remains effective after reconnecting the radio. Request neighbors after the interval, then compare its total/page to the known repeater.
6. Test a reachable local repeater before testing long multi-hop paths. Do not spam retries.
7. If something goes wrong, capture the app's **Support diagnostic** report, Android OS, Meshtastic and MeshCore firmware versions, and the screen sequence. Exclude passwords, keys, IDs, precise GPS and message content from public reports.

GitHub tracking: [Stage 2 roadmap #229](https://github.com/Renbrant/MeshCrossway/issues/229), [remote repeater tests #239](https://github.com/Renbrant/MeshCrossway/issues/239), [outbox UI #236](https://github.com/Renbrant/MeshCrossway/issues/236).
