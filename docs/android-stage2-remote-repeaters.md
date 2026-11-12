# Android Stage 2 — Remote MeshCore repeater status and neighbors

Roadmap: [#229](https://github.com/Renbrant/MeshCrossway/issues/229) · follow-up [#239](https://github.com/Renbrant/MeshCrossway/issues/239) · passive diagnostic baseline [PR #238](https://github.com/Renbrant/MeshCrossway/pull/238).

**This is a reviewable implementation, not a published APK nor proof of field interoperability with every firmware.** It is based on the Android 0.4.0 development branch; the signed `android-v0.4.0-stage1` release remains unchanged.

## User workflow

1. Open **Settings → Advanced network diagnostics**.
2. Select a saved **MeshCore repeater** (an approved current-radio contact, type 2, with a 32-byte public key). The existing Contacts node menu also offers the repeater dossier.
3. The dossier shows its saved contact membership and last local observation. These do **not** prove that the repeater is online.
4. Tap **Request remote status** or **Request neighbors**, read the explicit **FLOOD routing / mesh airtime warning**, then confirm. This sends **exactly one** request through the selected MeshCore radio. The next request for that repeater, even for the other operation or a neighbor page, requires a five-minute interval.
5. The screen distinguishes **Queued for local radio**, **Awaiting remote repeater response**, **Received**, **Timed out** and **Error**, with explicit freshness labels. Local command acceptance is never called a remote reply. After the local radio accepts the request, its acknowledgment reports whether it used **flood** (which can involve multiple repeaters) or a known path; this information is displayed with the request result, not guessed in advance.
6. If neighbors are available, display the reported total and **at most 8 neighbors per page**, with prefix, age and SNR. A saved-contact name is shown only when its prefix is an unambiguous local match. Tap **Request next neighbors page** separately to request more.

## Protocol integration

Uses Companion `CMD_SEND_BINARY_REQ` **50** with a full 32-byte destination pubkey:
- Status: binary payload **0x01**.
- Neighbors: binary payload **0x06**, format version **0**, maximum **8** records, offset (LE), newest-first, **6-byte public-key prefixes**, 4-byte random anti-duplicate suffix.
- Local submission: **RESP_CODE_SENT (6)**, 4-byte tag at offset 2, estimated timeout at offset 6. This is **not remote delivery**.
- Remote response: asynchronous **PUSH_CODE_BINARY_RESPONSE (0x8C)**, reserved byte then 4-byte **matching tag**, followed by binary response content.
- Remote status parsing accepts **52-byte** documented status data and the **56-byte** extension containing RX error count, with exact width and endian checks.
- Neighbor response parsing requires two uint16 counts and exactly `count × (6+4+1)` bytes: public-key prefix, seconds since hearing and quarter-dB signed SNR.
- The radio's `RESP_CODE_SENT` byte 1 indicates whether the **local radio used flood** for the outbound request. Before the request the app warns that flood routing **may** occur; it does not claim to know the route in advance. Neither a known path nor a flood indication predicts the return route or total airtime.\n- The firmware can deliver a binary reply without including the full remote key; the client attributes it **only to the single pending request and matching returned tag within the same BLE session**. This is session/tag correlation, **not cryptographic proof of who responded**.
- A timeout or disconnect does **not** automatically issue another request. Reconnecting creates a fresh protocol client and no pending request is replayed.
- Limited to one in-flight request per connected MeshCore session and **one request every 5 minutes per repeater, shared by status, neighbors and every additional neighbors page**. This applies even when a query fails or receives no remote response. The local throttle is stored in private Android preferences so Bluetooth reconnections or app restarts do not immediately reset it; an in-memory monotonic limit also applies while the session is alive. Responses are capped to the existing **176-byte** Companion frame limit; history is bounded to **24** repeater/kind records.
- The app does not store passwords or issue remote login, reboot, erase, modify configurations or bridge traffic to Meshtastic.
- Unsupported firmware (Companion version before 10), missing contact, disconnected/switched radio, malformed packets and uncorrelated tags are rejected rather than synthesized.

## Primary code references

- [Official MeshCore Companion implementation](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) — CMD_SEND_BINARY_REQ, RESP_CODE_SENT, PUSH_CODE_BINARY_RESPONSE
- [MeshCore repeater implementation](https://github.com/meshcore-dev/MeshCore/blob/main/examples/simple_repeater/MyMesh.cpp) — REQ_TYPE_GET_NEIGHBOURS version 0
- [MeshCore Python parser](https://github.com/meshcore-dev/meshcore_py/blob/main/src/meshcore/parsing.py) — remote status field layout
- [MeshCore Python client](https://github.com/meshcore-dev/meshcore_py/blob/main/src/meshcore/commands/binary.py) — status and paginated neighbors request

These are protocol references, not evidence that the test devices support every optional remote reply.

## Validation required

- [ ] Unit tests: status and neighbors commands, valid 52/56-byte status, paging, exact record lengths, quarter-dB signed SNR, tag mismatch, replay, malformed, cooldown, unsupported target, timeout and session isolation.
- [ ] Android unit tests, wire fixtures, build/lint, API29 and API35 emulator smoke pass in GitHub Actions.
- [ ] Real Samsung Android phone with two independently paired MT/MC devices and an actual MeshCore repeater contact; no hidden traffic when screens open.
- [ ] User confirms that local accepted/queued remains distinct from the **remote** response; unknown/unsupported/timeout states do not crash or reconnect BLE.
- [ ] Confirm **five-minute shared per-repeater guard** across status → neighbors → next page, after disconnect/reconnect and after restarting Android; verify the 8-neighbor pagination, ambiguous 6-byte prefix handling and no automatic requery.
- [ ] Ensure normal chats, QR channels, tracks, map, backup/restore and local browser workspace are unchanged.

**Limitation:** These requests are capability-gated and deliberately conservative. Results are in-memory per connected session; they are not uploaded or persisted as permanent telemetry. A remote repeater's response is not a guarantee of mesh availability or fitness for life-safety use.
