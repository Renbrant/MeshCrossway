# Messaging phase 1 — v0.7.9 experimental

Related to #37. Original implementation based on comparative research across several communication systems and the public Companion protocol. Target remains the Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 (non-B), 480×320.

## Available software

- Chats opens independent MC conversations, sorted by last local activity, with sender/name, last text, time, unread counter and connection state. All / MT / MC filters exist in Nodes, Groups and Chats. MT explicitly reports unavailable: no Meshtastic adapter or simultaneous BLE-session support is claimed.
- Open Nodes to start a direct conversation. Groups separates native MeshCore channels and room contacts. Rooms have a dedicated login/check/leave screen with waiting, authenticated, rejected, disconnected and timeout states. Room sends require a successful native session; login passwords are not persisted.
- Compose keeps a local draft. The SEND action selects the conversation's original network, full radio identity and native recipient. Channels additionally retain their channel identity so changing a slot's secret cannot send an archived conversation to a different group.
- Message lifecycle: draft → queued locally → radio accepted → delivered only after an unambiguous correlated direct-message ACK, or rejected/uncertain. A channel acceptance never implies delivery to its subscribers or reading. Disconnect/timeouts do not automatically retry.
- Tap a message for origin/time/state and available route/SNR metadata. Rejected, uncertain or accepted outgoing messages can be resent manually only after a separate duplicate-risk confirmation. Recognized HTTP(S) links are offered as text to open on another device; there is no browser.
- Chat tools provide local search, mark all read, favorites, local aliases, four editable quick replies, notification previews, All/Mentions/Muted controls, local blocking and confirmed clear history. Blocking/mute affect the local UI, not reception at the radio. Blocked conversations can be revealed from tools. Mentions use `@` followed by the current radio's name. Opening a thread marks its retained incoming messages read.

## Storage and durability

The SD contains plaintext, including message text and channel data. Encryption/eFuse provisioning remain deferred by the owner's request. No new libraries were adopted.

The journal is stored under `/sdcard/mcx-history/mc-<full-32-byte-public-radio-id>.log`. Every transaction has a schema/size/sequence envelope, individually checked records and a commit checksum of the resulting state. Recovery applies only fully committed transactions, stops at a corrupt/incomplete tail and truncates that tail before a subsequent append. Checksums detect accidental corruption; they are not authentication.

A low-priority FreeRTOS worker performs all reads, writes, recovery, compaction and `flush/fsync`, away from LVGL/BLE callbacks. The UI supplies immutable bounded snapshots at most once per second. Journal deltas include changed messages, contacts, channels and local preferences. Reading, clearing and editing while offline also schedule persistence. A pending/unsaved indicator is distinct from saved state. Power loss before a transaction commits can lose recent uncommitted work; FAT/SD controller behavior still needs power-cut validation on hardware.

The active journal compacts at the 1 MiB build-time quota, retaining a previous `.bak2` journal. One transaction may temporarily exceed that threshold; three files can exist during compaction. Retention is explicitly bounded to the newest 96 message slots per radio, 512 contacts, 256 channel slots and 128 local conversation preferences. This is not an unlimited archive. Preferences contain drafts, aliases, favorites, notification settings, edited quick replies and the last selected context. Journal schema 2 rejects unknown/ABI-incompatible layouts; the unreleased PR #56 binary-v1 snapshot is not silently reinterpreted or deleted.

The last-radio pointer has a previous copy for offline startup. All archives validate the full public identity. Live radio contact/channel synchronization remains authoritative; old history stays local. Restore runs before draining the radio backlog, merges messages collected during temporary SD absence, and reconstructs no transmit queue. Restored queued/accepted messages become uncertain; old ACK tokens cannot confirm a new session's message. SD failures leave bounded RAM operation and visible error status; reinsert/load and writes are retried.

## Resource limits

Large journal snapshots and recovery buffers are allocated in PSRAM; the worker has a 6 KiB stack. The request queue holds at most four snapshots, with a single failed-write retry snapshot. Storage and conversation arrays have compile-time limits. Actual heap/PSRAM high-water marks, SD throughput and UI frame rate are not yet measured on hardware. Do not promote v0.7.9 to the recommended image on host tests alone.

## Verification

Production-code host tests cover explicit network/radio send fences, native destinations, channel identity changes, UTF-8, room send gating, unread state, search, mute/mentions, drafts/preferences, wrong-radio archives, every truncation point of a message delta transaction, checksum corruption, retry after a bad tail, quota compaction, previous-journal fallback, and zero transmissions after restoration. The LVGL fixture compiles and exercises the actual workspace, forms, touchscreen keyboard and send path. CI additionally builds the complete ESP-IDF 5.3.2 firmware using the pinned Waveshare BSP.

Hardware acceptance remains open: real MeshCore RX/TX and correlated ACK, room authentication against an actual server, SD absent/full/removal and abrupt power cuts, reboot/reconnect without replay, BLE+Wi-Fi+LVGL+SD under load, unread restoration, touch usability and memory measurements. Meshtastic acceptance applies when its independent adapter is implemented. The parent roadmap issue remains open for these acceptance checks and phases 2–5.

## v0.7.8 recovery provenance

The existing published v0.7.8 bytes are retained, with full merged-image SHA-256 `483b7da13ddb8c394ecb20bd92d312cb707c59a5ed538016fb7be10399432d45` and source commit `40788ccc2e1ebec1b79e0183f42bdb793d7f0a74`.

Evidence: successful Pages run [40824001151](https://github.com/Renbrant/MeshCrossway/actions/runs/40824001151), deployment job `123282726371`, records `Manifest ready: v0.7.8 / 483b7da13ddb` and the matching verification. Published build-info and the binary agree. Separate PR/cache builds had different hashes. The previous `f437f38d325c` constant did not identify the deployed image; it is replaced by the verified full published hash, not by rebuilding the approved version. Pages validation now checks v0.7.8 instead of the retired v0.6.8 path. v0.7.8 remains recommended; v0.7.9 is experimental.
