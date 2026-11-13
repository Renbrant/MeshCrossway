# Android 0.4.0 field follow-up — Home outbox summary

Tracking: [#236](https://github.com/Renbrant/MeshCrossway/issues/236), with Stage 1 roadmap [#229](https://github.com/Renbrant/MeshCrossway/issues/229).

## Real-radio connection test

The initial Meshtastic BLE `GATT 133` failures occurred while the radio was physically far away. After the tester moved the Meshtastic radio closer, **both radios reached Ready** in the existing **0.4.0 beta** without a software update:

- Meshtastic: Ready, synchronized, MTU 255, 254 incoming frames, zero reconnection attempts.
- MeshCore: Ready, synchronized, MTU 176, 420 incoming frames, zero reconnection attempts.

The original GATT 133 result was a generic transport failure; RF range is a plausible explanation, but its root cause is not proven. **No BLE retry/scheduling/GATT changes are included in this follow-up.** The earlier exploratory BLE branch must not be merged without a close-range reproduction.

## Home improvement

Before this change, Home displayed a large `Recent: N need review` indicator for **historical** uncertain sends. In MeshCore group conversations, `Delivery unknown` can simply mean the radio could not verify delivery to every participant. A historic unknown broadcast does not warrant an urgent Home warning.

New behavior:
- The MT and MC Home cards independently summarize statuses from **the last 15 minutes** for the currently selected radio only.
- Home mentions **queued**, **submitted**, and direct-message **radio accepted** records as awaiting confirmation.
- Home mentions recently **rejected** messages (including group) and **uncertain direct** messages as failed/unknown.
- Home does **not** promote historical unknown sends or uncertain **group broadcasts** into an urgent status, and continues to show the latest message preview.
- The existing Chats **Outbox & delivery status** remains unfiltered and retains full saved send history, including old uncertain group messages. No stored message is changed or deleted.
- A Home fingerprint includes the derived current outbox summary so it can update when an old pending message falls outside the 15-minute view without another send.

## Tests

- Added pure JUnit cases for direct vs group, pending/rejected/uncertain/delivered, 15-minute boundary, expired/missing/future timestamps, inbound messages, and historical Outbox retention.
- CI must run existing protocol fixtures, JUnit, debug build/lint and Android emulator smoke API 29/35.

**Release policy:** this is a proposed follow-up PR, not a modification to the signed `android-v0.4.0-stage1` tag. Do not silently reissue the already-published APK, and do not change BLE firmware/retries based on the range-sensitive observation alone.
