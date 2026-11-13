# Android Stage 1 — RF reception history by node

Parent roadmap: [#229](https://github.com/Renbrant/MeshCrossway/issues/229) — Stage 1. Depends on the initial RSSI/provenance fix in [#230](https://github.com/Renbrant/MeshCrossway/pull/230). This increment does not change the release version or publish a new APK.

## User workflow

1. Open **Nodes**.
2. Select a Meshtastic or MeshCore node/contact.
3. Tap **Reception history**.
4. If the existing local message history contains attributable LoRa metrics, view bounded RSSI/SNR charts and a list of recent samples. If not, the dialog explains why.

The charts use **saved message timestamps**, which may reflect historic backlog or an inaccurate remote radio clock. They are not live radio scans, realtime link-quality reports, or proofs of reachability.

## Implementation

- Uses the existing SQLite `messages.rx_meta`; no database migration, no new permissions, no automatic network traffic, and no increase in BLE polling frequency.
- Queries at most **4096 saved incoming messages** under an exact `network + radio` scope, retaining up to **96 eligible samples** for the selected node. Parsing and selection occur on the existing private history worker thread, not on the UI/GATT thread.
- Meshtastic attribution requires its protocol-reported **numeric source node ID** (`rx.source`). Group messages are attributable when this ID is present.
- MeshCore attribution requires an incoming **direct message** whose sender contact was resolved to a **full 32-byte public-key identity**. MeshCore group sender display names and ambiguous truncated DM prefixes are not treated as authoritative identities.
- Missing, implausible, or unsupported RSSI/SNR/hop values are omitted instead of being converted to zeros.
- Every sample retains the source provenance label introduced in PR #230. A Meshtastic packet marked MQTT is explicitly **not** treated as confirmed direct radio reception.
- The chart uses separate RSSI and SNR scales and provides textual sample values for accessibility. This history records reported **per-message** metrics, not a synthetic combined network quality score.
- No changes to outbox/delivery state machine, transmitted packet encoding, Cloudflare sync, offline maps, QR channels, APK signing, or firmware settings.

## Important limitations

- This is history **derived from messages only**; it will not populate during quiet periods or from signal-only packets not attached to a message in the local database.
- Different protocol/firmware builds expose different fields; there may be no RSSI for MeshCore via Companion even when SNR or hop counts exist.
- A MeshCore channel sender name is unauthenticated free-form text; attributing such messages to a saved node would be misleading.
- The existing message database retains its current retention/backup semantics. The chart includes only a limited sample window rather than an unlimited archive.
- If the app has older messages from before `rx_meta` was available, those messages may not contribute samples.

## Tests / acceptance

- [ ] `RfHistoryTest` passes for MT numeric identity, MC full-key DM, MC group exclusion, invalid signal metrics, missing metadata, and cross-protocol identity isolation.
- [ ] `./gradlew testDebugUnitTest assembleDebug lintDebug` passes on CI.
- [ ] Android device smoke tests API 29/35 pass.
- [ ] Real Samsung S25 with independent MT and MC Heltec V3 radios: send group MT packets and MC direct messages; inspect charts and ensure separation.
- [ ] Check absent/partial metrics, stale/backlog timestamps, long history, no saved history, and radio disconnect.
- [ ] Confirm ordinary chat, reconnect, QR channel creation/import, tracking, offline maps and local browser workspace still work.

Next Stage 1 work is a **separate** review of pending-message UX, ACK correlation, process death, queue uncertainty, and duplicate safety. Do not silently resend uncertain submissions.
