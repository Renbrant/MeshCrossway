> **Historical UI research for v0.6.x.** The `main` firmware has progressed to v0.7.6 with MeshCore live Companion screens; some six-tab display/chevron and DEMO wording below describes earlier versions. Proposed v0.7.7 interaction changes are still in [open PR #54](https://github.com/Renbrant/MeshCrossway/pull/54). See [current status](current-status.md) and [v0.7.6 UI](contextual-ui-branding-v076.md).

# Field Console navigation — Meshtastic + MeshCore workflow research

**Reference date:** 2026-10-28  
**Target:** MeshCrossway Core (Waveshare ESP32-S3 Touch LCD 3.5, 480 × 320).  
**Current status:** UI/demo only; the two BLE radio pods and GNSS have not been integrated.

## Six-tab information architecture

| Tab | Purpose | Initial state and navigation |
|---|---|---|
| Home | At-a-glance radio/GNSS/bridge diagnostics | Auto-hide the global left bar; keep its chevron so the six tabs are reachable |
| Nodes | People, companion nodes, repeaters, sensors, room servers when appropriate | Network filter All / MT / MC; inspect role, heard time and messaging capability |
| Groups | Meshtastic channels, MeshCore channels and room-server conversations | Network filter All / MT / MC; unread counts per thread when real messages are integrated |
| Chats | Open **the currently selected** individual/group conversation | Opened from a Node (DM) or Group (channel/room); preserve selection when navigating away |
| Map | Positions, tracks, POIs, GNSS and sharing | Never imply position accuracy without a fix |
| More | Wi-Fi, device, read-only viewer, radio pod diagnostics | Put detailed radio diagnostics behind More > Radios |

This is an information-architecture design, **not** a claim that the two protocols have the same packet format, routing, chat security or device feature set. Every displayed network record must keep its MT/MC origin and native identifier. Do not merge nodes just because visible names match.

## Source-based behavior

### Meshtastic

- Official Android user guide: [Messages & Channels](https://meshtastic.org/docs/software/android/user/messages-and-channels/) documents channels and DMs, unread indicators and selecting an individual chat from a node list.
- Official device UI reference: [Meshtastic UI](https://meshtastic.org/docs/configuration/device-uis/meshtasticui/) has a node list, filter options, channels, chats and per-chat message status.
- [Joining your local mesh](https://meshtastic.org/docs/getting-started/joining-your-local-mesh/) explains that previously heard nodes remain in the database after configuration changes. **Do not label the whole list "online".**
- Meshtastic channels are configured by channel slot, PSK and radio settings, not equivalent to arbitrary MeshCore channel names.

### MeshCore

- The [official repository](https://github.com/meshcore-dev/MeshCore) distinguishes Companion, Repeater and Room Server firmware. A discovered repeater is not automatically a direct-message recipient.
- The [companion protocol](https://github.com/meshcore-dev/MeshCore/blob/main/docs/companion_protocol.md) includes contacts, channel slots, message synchronization and advertisements. Plan BLE adapters independently.
- [QR format](https://docs.meshcore.io/qr_codes/) distinguishes contacts by Companion (1), Repeater (2), Room Server (3) and Sensor (4).
- The [official FAQ](https://docs.meshcore.io/faq/) describes channels as live communication and room servers as store-and-forward conversations. Show the difference between a channel and a room-server thread in Groups.
- Do not equate contact discovery with availability, do not imply a repeater is a messaging target, and do not promise reliable delivery or a unified cross-network channel without a separately designed gateway.

## State model for a future live implementation

- `nodeKey = (network, nativeNodeId)`: network is MT or MC, never just display name.
- `threadKey = (network, kind, nativeId)`: kind is direct, channel or room.
- `selectedThread`: persistent in UI state; clicking a node or group opens Chats and sets this key.
- `nodesFilter` and `groupsFilter` are separate All/MT/MC choices.
- `unreadCount`: derive from actual received messages, not channel membership; indicate zero/unknown distinctly.
- `lastHeard`: timestamp of observed advertisement or packet, not guaranteed reachability.
- `canMessage`: distinguish a user/companion from a repeater, sensor or server; include protocol-specific prerequisites.
- `txStatus`: never label a message delivered without protocol-specific confirmation.
- Room server history and real-time channel history have different expectations.

## v0.6.2 scoped implementation

Only sample rows and demonstrative unread counts are shown. Selecting a sample user or channel changes the active preview thread; composing/sending is explicitly disabled. MeshCore room servers are labeled separately. The design preserves working Wi-Fi/SD and manual read-only remote capture. A physical hardware test is required to validate text, glyphs, scrolling, touch targets and memory.

## Later milestones

1. BLE connection state and read-only node/contact discovery for both independent pods.
2. Independent native channel/group lists and message synchronization with source-aware keys.
3. Unread counts, last-heard indicators, pinning/favorites and detailed repeater screens.
4. Safe protocol-aware transmit flows (separate permissions, channel settings, rate limits and delivery state).
5. SD persistence for conversations and GPX/POI data once data formats have been defined and migration tested.
