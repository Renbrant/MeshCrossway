# Meshtastic audit remediation — Android 0.3.2

The 0.3.1 audit is retained as a historical review. This release fixes its configuration defects and adds the reply, reaction and reported-node-data support described below. It does not claim complete feature parity with the official app.

| Finding | Change | Evidence |
|---|---|---|
| A01 | Explicit preset/region wire IDs; unknown values remain labeled as unknown | MtLora, MtSettingsTest enum fixtures |
| A02 | Request IDs, routing NAKs/timeouts, and a correlated GET/readback determine save success; no optimistic config update | MtAdmin, positive/negative ACK/readback fixtures |
| A03 | Keys refresh after 240 s; writes defer until a fresh key; sender/destination/request correlation protects local state | MtAdmin expiry and foreign/unsolicited-response fixtures |
| A04 | Licensed onboarding uses set_ham_mode with callsign and preserved power/frequency; owner readback and channel refresh follow | Ham envelope fixture; physical encryption/reboot validation still needed |
| A05 | Decode module responses including typed empty messages | Field-8 module fixtures |
| A06 | MT state/admin revisions contribute to UI refresh independently of MC | MainActivity settingsFingerprint |
| A07 | Cache presence represents loading separately from zero-byte proto3 defaults; unsynchronized writes are blocked | Empty Config/Module and owner-before-read fixtures |
| A08 | Insert missing channel name; 11-byte limit; update Groups/chat models after readback; key fingerprint is stable across renames; exact legacy fingerprints migrate scoped history | Channel rename fixture; database instrumentation |
| A09 | Retain firmware/hardware/excluded-module/signature capabilities; byte-based names/status validation; filter unsupported controls/roles | Version, metadata, UTF-8 and capability fixtures |
| A10 | LoRa draft with Save/Discard, complete manual mode, finite frequency/rate/SF/bandwidth/power/slot checks, region preset-map constraints and false MQTT default | MtLora validation fixtures; UI draft implementation |
| A11 | One reducer handles streamed and administrative Config/Module data and refreshes send guards | ProtocolClient applyMtConfig |
| A12 | Data.reply_id/emoji on transmit/receive; durable associations in rx_meta; reply/reaction actions and associated-message rendering | Wire fixtures, database reopen instrumentation |
| A13 | Retain short name, role, hardware and status; decode named device/environment/power/air-quality and other telemetry, recent samples, position samples, neighbors and requested traceroute; expose metrics tools | Telemetry/status fixtures, node backup instrumentation |
| A14 | No confirmed-save claim before readback; expected-restart status; reconnect retains the pending verification for the same BLE address; no automatic SET resend; staged LoRa draft and per-control Save/Discard dialogs | Reconnect verification fixture, MtAdmin and RadioService |

## Remaining feature inventory (not represented as working controls)

The audit matrix also listed broader official-app functionality absent from MeshCrossway. Remote administration/shell, USB/TCP transports, firmware updates, full channel management/sharing, radio backup/key management and every module/device editor remain outside 0.3.2. Local phone backups and local GPS tracks do not substitute for these radio features. Unsupported packet types remain ignored without presenting invented values.

## Validation

- Existing protocol, message delivery, two-network isolation and receive-metadata fixtures.
- New MtSettingsTest, also invoked by JUnit in the Android unit-test task.
- Existing browser and backup/migration scripts.
- Release CI gates publication on SQLite migration, reopen/backup persistence instrumentation on API 29 and 35, unit tests, assembly/lint, and permanent signing-certificate/version verification.
- Physical BLE/RF tests are still required to confirm firmware-specific Ham transitions, radio restarts, externally observed delivery and settings persistence on the user's radios. A simulator fixture is not a physical radio result.

Database schema advances from 4 to 5 by adding node_meta; existing messages, flags, tracks and drafts are retained. Old JSON history backups supply an empty metadata object when the new node column is absent. Reply/reaction metadata reuses the existing rx_meta column.

Protocol and enum facts were compared against official protobufs revision 4c80682b3b441a4cebab9f3e5c3097857cb5c81d and Android revision 4b557c67a731ecc0908de4db2619bae3a696e2f4. The implementation is independent code; upstream GPL implementation files are references, not copied source.
