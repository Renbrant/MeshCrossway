# Android Stage 2 — repeater observability and network diagnostics

Parent: [#229](https://github.com/Renbrant/MeshCrossway/issues/229) · **Stage 2**. This is **slice 1 / passive observability**, based on the signed Android 0.4.0 source branch. No APK is created or released by this PR.

## Existing capabilities audited

- Contacts already has **Repeaters** as a type filter. Its scope comes from MeshCore contact type **2**.
- Each saved MeshCore contact has a **full 32-byte public-key identity**, contact type, saved name, optional position, membership state, metadata update timestamp and (when the radio reported it locally) a receipt timestamp.
- The app already displays an observed MeshCore channel message's **validated flood path**. This is the path of **that message copy**, not a reusable routing table.
- Meshtastic already exposes an explicit on-demand traceroute request from node metrics.
- The companion client already knows **local radio** stats such as firmware, battery/noise floor where supported. **Do not label these as remote repeater stats.**
- Existing sanitized support diagnostics report GATT events and client frames; these are not raw LoRa packets.

## Implemented in Stage 2 slice 1

1. **Settings → Advanced network diagnostics**: two protocol-specific read-only panels, each showing connection readiness, negotiated Bluetooth MTU, BLE frames received, and **saved incoming/outgoing messages over the last 15 minutes**.
2. For each separate radio, show how many recently saved incoming messages have an actual reported LoRa RSSI, SNR, or authenticated observed flood path. Show MQTT-marked items separately without claiming direct RF reception.
3. **MeshCore repeater inventory**: up to 60 contacts with type `2` and an unambiguous **full 64-character hexadecimal public key**, sorted primarily by actual local receipt timestamp. Show current radio membership vs saved/historical data.
4. **Repeater dossier**: full public identity, last local observation where known, last saved update, radio-membership context and optional previously saved map position. The dossier is also available from the existing per-node context menu.
5. Unsupported remote status (uptime, remote battery, noise floor, neighbors) is clearly marked **Not queried / Unavailable**, not synthesized.
6. No commands are issued on opening a diagnostics screen, repeater list or dossier; no new persistence, permissions, extra BLE polling, LoRa transmissions or protocol bridging.

## Capability matrix and work still needed

| Feature | Source | Slice 1 | Next action |
|---|---|---|---|
| Repeater identity/type/membership | Saved MeshCore contact response | Read only | Validate with multiple firmware builds |
| Repeater activity and position | Cached contact update and local receipt | Read only, marked historical | Validate time semantics against actual devices |
| Remote status incl. uptime, noise floor and counters | MeshCore Companion **status request** command 27; status push 0x87 | **Not implemented** | Read-only user-triggered RPC; validate firmware format and signed response correlation |
| Remote neighbors | MeshCore Companion remote repeater protocol (firmware/version specific) | **Not implemented** | Inspect official command and complete-response pagination before UI |
| Per-hop path trace | MeshCore Companion trace command 36, response correlation by tag | **Not implemented** | Explicit user-requested trace with airtime/rate limit, bounded TTL, and path validation |
| Meshtastic traceroute | Existing node metrics flow | Already available | Extend feedback and durable last-result interpretation |
| Packet inspector | Already available BLE events, saved messages / RxInfo | Aggregate counts only | Optional bounded opt-in metadata viewer; never claim raw spectrum capture |
| RF congestion/airtime | Only if specific firmware reports a valid value | Unsupported/unknown by default | Capability probe and local-radio-vs-remote distinction |

Protocol references:
- [Official MeshCore companion protocol](https://github.com/meshcore-dev/MeshCore/blob/main/docs/companion_protocol.md)
- [Official companion radio protocol wiki](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol)
- [Official MeshCore CLI commands](https://github.com/meshcore-dev/meshcore-cli/blob/main/README.md)

These links establish potential protocol support, **not evidence that the installed companion firmware implements every optional command**.

## Acceptance / regression

- [ ] JUnit test repeater type+full identity, receipt vs modified timestamps, stale/unknown states, and bounded list behavior.
- [ ] JUnit test per-radio recently stored incoming/outgoing counts, valid/absent RSSI/SNR, path-vs-hop semantics, MQTT ambiguity, and excluded timestamps.
- [ ] GitHub Actions Android unit tests, protocol fixtures, build/lint, API29/API35 emulator smoke tests pass.
- [ ] Real Android phone + two independent radios: 0.4.0 dual-BLE connections remain stable; opening diagnostics generates **no new RF requests**.
- [ ] Confirm repeater list on MeshCore includes full-key type-2 contacts, excludes ordinary clients/rooms/truncated identities, and explains historical membership.
- [ ] Validate message send/receive, channel import/QR, offline maps/tracks, support report sanitation and cloud opt-in unchanged.
- [ ] Keep signed Android 0.4.0 prerelease immutable; patch release only after user acceptance and permanent signature checks.

Next, build **remote status and neighbors** as a separate rate-limited protocol adapter; follow with traceroute and metadata packet inspector in separate PRs. All remote commands must be user-triggered and scoped to a full target key + current radio session. Absolutely no Meshtastic↔MeshCore forwarding.
