# Android Stage 1 — outgoing status, manual draft recovery and late ACK safety

Roadmap: [#229](https://github.com/Renbrant/MeshCrossway/issues/229) (Stage 1). This is the third stacked implementation increment, built over [#230](https://github.com/Renbrant/MeshCrossway/pull/230) and [#231](https://github.com/Renbrant/MeshCrossway/pull/231).

This change **does not retransmit historical messages** and does not add a background outbox sender.

## User experience

- The Home **MT · CHATS** and **MC · CHATS** cards summarize **recent** outgoing messages awaiting radio/peer confirmation or needing attention. Unread message counts remain unchanged.
- In **Chats → Outbox & delivery status**, the user sees separate Meshtastic and MeshCore counts for the currently selected radio, an explanation of the limits, and safe recovery instructions.
- The outgoing bubble explicitly distinguishes:
  - **Saved locally** — queued, not yet confirmed submitted.
  - **Submitted over Bluetooth** — a completed GATT write, not proof the radio transmitted.
  - **Accepted by radio, group** — no claim that other group members received it.
  - **Accepted by radio, direct** — waiting for a peer ACK where the protocol can provide one.
  - **Peer acknowledged** — protocol ACK, **not** a human read receipt.
  - **Rejected** — failed operation; no automatic resend.
  - **Delivery unknown** — radio outcome could not be verified; no automatic resend.
- For **Rejected** or **Delivery unknown** messages, long-press and choose **Prepare as new draft (manual)**. The app first warns that sending again could create a duplicate; confirmation copies only the text into the composer. The user must edit/check the original radio, protocol, channel, target and hit **Send** separately.
- A draft can only be prepared within the exact originating **network + radio + topic + channel-key identity + peer + channel slot + direct/group** context. The UI refuses to prepare it in a different chat/radio.
- The copy operation resets stale reply/reaction targets and **never** changes the original message status. An existing draft is replaced only after explicit confirmation.
- At the final confirmation, the app rechecks the most recent saved message state and radio selection; if a late ACK has arrived or the destination context changed while the warning was open, the copy action is refused.

## Storage and protocol boundary

- Existing SQLite message state strings are preserved: `queued`, `submitted`, `radio accepted`, `delivered`, `rejected`, `uncertain`.
- Existing `HistoryStore.uncertain()` on service initialization/disconnect, protocol-specific ACK matching, and no-blind-retry behavior remain unchanged.
- A delivery-state callback is additionally restricted to `outgoing=1` and the exact `network + radio + uid` row, so it can never rewrite a received message with a coincidentally matching ID.
- The existing SQL transition conditions still prevent late `submitted` or `uncertain` callbacks from downgrading a stored `delivered` message. A late correlated peer ACK can upgrade an `uncertain` state to `delivered`.
- Counts are deliberately **snapshot-scoped** (recent loaded messages, default up to 2000 per radio); they are not advertised as a complete lifetime transmission tally.
- No message content is sent to diagnostic exports and no extra location/keys are recorded.

## Tests

- `OutgoingStatusTest`: status labeling, group-vs-DM acknowledgement distinction, exact network/radio/recipient/channel-key checks, and count filtering.
- `SmokeInstrumentation.outboxRecovery`: persistent queue outcome after disconnect, late ACK ordering, inbound/outgoing isolation, other-radio isolation and database reopen.
- Existing radio protocol fixtures, QR channel link tests and baseline Android device smoke tests remain mandatory.

## Hardware validation before a non-draft merge/release

- [ ] S25 + both Heltec V3 radios connected: send MT and MC group and direct messages, verify independent counters and descriptions.
- [ ] Disconnect each radio mid-send; verify the saved record says **Delivery unknown**, does **not** auto resend, and survives Android process restart.
- [ ] Test late ACKs arriving before and after the local timeout; they must not be overwritten by a delayed GATT completion.
- [ ] Long-press an **unknown** message: confirm the action creates only a new composer draft, with a duplicate warning. Canceling leaves both messages and existing draft intact.
- [ ] Switch protocol, radio, or channel configuration before drafting; ensure no historical message can silently be re-addressed to the new target.
- [ ] Check still-working channel/QR import and share, offline maps, GPX tracks, browser workspace, and Bluetooth reconnect.

## Limitations and follow-up

- This increment **does not add an always-transmitting offline queue**; delivery/retry behavior is intentionally conservative.
- Peer ACK availability and meaning depend on firmware, radio configuration, destination and protocol.
- Outstanding follow-up: run field validation; reconcile source/tag and staged PR merge order; decide whether full-database outbox counts or additional visible error states are worthwhile **after** real-world feedback.
- Leave the published `android-v0.3.12` APK, signing key and versionCode unchanged.
