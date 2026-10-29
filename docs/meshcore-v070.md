# MeshCore Companion — v0.7.0 experimental

v0.7.0 adds a real Companion client to the Waveshare display. It has not been validated on physical hardware. The owner confirmed v0.6.8 UI and manual PIN-free web screenshots; those exact bytes remain the recommended default and rollback. This does not claim that every radio feature has been hardware tested.

## Authoritative references

- [Official Companion guide](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/companion_protocol.md). The guide calls itself incomplete; the actual command dispatcher is the authority for layouts and responses.
- [Pinned official firmware dispatcher](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/MyMesh.cpp), protocol level 14, all 59 defined command opcodes (1–43, 50–52, 54–66; other codes are reserved).
- [ESP32 BLE frame transport](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/src/helpers/esp32/SerialBLEInterface.cpp) and [176-byte frame bound](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/src/helpers/BaseSerialInterface.h).
- [Official JavaScript client reference](https://github.com/meshcore-dev/meshcore.js/tree/26193c059fcd1d0524f1ad5b19fd88287ecc5c61), used to cross-check command encoding. Wire facts were independently implemented; neither upstream library nor firmware source is linked or vendored into this application.

## Connection and ownership

The radio must run **MeshCore BLE Companion**, not repeater-only or serial-only firmware. Service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`, write RX `...0002...`, notify TX `...0003...`. Discover actual handles, authenticate the six-digit Bluetooth PIN, exchange MTU (preferred 247, minimum 155), discover the TX CCCD and enable notifications before any application command. Every write/notification is one complete binary frame; there is no UART framing or arbitrary BLE chunk concatenation. Writes must fit the negotiated `MTU - 3` and the 176-byte radio limit; oversized commands are rejected, not split.

`APP_START` → `DEVICE_QUERY(14)` → read radio clock → all contacts (START / records / END) → all reported channel slots → battery/storage → drain offline messages. Channel count comes from the radio, never an assumed eight slots. Protocols below level 3 are rejected explicitly. Newer feature gates follow known introduction levels where documented; otherwise the radio's unsupported/disabled error is authoritative.

NimBLE callbacks only copy into a bounded, 16-frame PSRAM mailbox. A 50 ms LVGL timer owns the client model and processes at most four queued frames per tick. One command is in flight until both its ATT write completion and expected application response arrive. Async events cannot acknowledge unrelated commands. Remote requests also wait for their peer/tag-correlated result before another remote request is started. Local protocol timeout, malformed frame or mailbox overflow closes the connection; there is no automatic retry of potentially transmitted data. Manual reconnect creates a clean radio session.

## Display workflows

- **Nodes**: real contacts, contact type, unread counts, public key, advertised coordinates and paths. Details provide chat, repeater/room login, status, telemetry, path discovery/reset and removal.
- **Groups**: real channel slots and unread counts. Tap a configured slot to chat or an empty slot to configure it. Channel setup supports existing keys, hashtag keys (first 16 bytes of SHA-256 of the exact hashtag name) and random private keys when the key is empty. Clear a slot with empty name and an all-zero key.
- **Chat**: compose direct/channel text with the working full-screen keyboard. Input is limited to 150 UTF-8 bytes, also bounded by MTU. Direct messages reject ambiguous six-byte key prefixes. A valid radio Unix clock is required; clock setting is explicit and does not use uptime as a fabricated date. Messages show QUEUED, RADIO ACCEPTED, DELIVERED, REJECTED or UNCONFIRMED. Only a matching direct-message ACK yields DELIVERED; channel acceptance never implies delivery to every member.
- **More → Radios → MeshCore Tools**: typed command forms for every catalog entry below. Numeric fields are encoded in the documented units; keys/binary expert payloads are hex. Mutating or secret-related operations require an in-app confirmation. Firmware-dependent tools are disabled below their known minimum protocol.
- Results decode identity, firmware, battery/storage, clock, radio statistics and common Cayenne LPP telemetry. Variant-specific/binary data remains available through **SHOW / HIDE DATA**. Bluetooth PIN bytes from device-info are redacted; private keys/channel keys are not printed to serial or added to HTTP health. An explicit export can be revealed locally, so protect screenshots of that view.
- The web view remains manual, read-only and PIN-free, with its five-second capture limit. BLE startup remains opt-in; LCD DMA buffers, pixel alignment and CPU configuration from the working baseline remain intact.

## Scope and practical limits

This release exposes the complete **59-opcode Companion command catalog**, not every future feature from every fork. Dedicated everyday screens cover contacts, groups and chat; expert functions use typed parameter forms, and arbitrary vendor/binary payloads require knowledge of the radio's schema. Optional key export/import can be disabled in upstream builds; sensors/GPS and repeating depend on board and firmware. Other transports, phone-app account features, maps, dual-radio Meshtastic operation, LoRa firmware OTA and continuous navigation are not implemented by this adapter.

The model has 512 contact records, 256 channel slots and the most recent 96 messages in PSRAM. History is temporary; ring eviction, new radio sessions and restart discard it. Reconnect is manual and never replays queued outgoing messages. A channel's visible text may contain a sender prefix supplied by the radio. Contact positions are advertised coordinates, not a claim of a current GNSS fix. A future SD message store and automatic reconnect can build on this protocol layer.

Some command families are explicitly multi-step: signature start/data/finish, room login/session checks, and regional flood overrides. Tool availability does not bypass the radio's permission, capability, airtime or region checks. No radio reconfiguration, automatic advert, location broadcast, login or factory reset occurs during initialization. Factory reset/reboot intentionally disconnect the radio; the client does not replay them.

## Command coverage

Every row has a typed encoder and touchscreen form. Numeric opcodes refer to the pinned official dispatcher. Minimum levels marked 1 are baseline commands; the client as a whole requires level 3+.

| Opcode | Tool | Minimum level | Confirmation |
| --- | --- | --- | --- |
| 1 | Refresh identity | 1 | No |
| 2 | Send direct text | 1 | No |
| 3 | Send channel text | 1 | No |
| 4 | Refresh contacts | 1 | No |
| 5 | Read clock | 1 | No |
| 6 | Set clock | 1 | Yes |
| 7 | Advertise identity | 1 | Yes |
| 8 | Rename radio | 1 | Yes |
| 9 | Add or update contact | 1 | Yes |
| 10 | Fetch waiting messages | 1 | No |
| 11 | Radio parameters | 1 | Yes |
| 12 | Transmit power | 1 | Yes |
| 13 | Reset contact path | 1 | Yes |
| 14 | Set advertised location | 1 | Yes |
| 15 | Remove contact | 1 | Yes |
| 16 | Share contact nearby | 1 | Yes |
| 17 | Export contact | 1 | No |
| 18 | Import contact advert | 1 | Yes |
| 19 | Restart radio | 1 | Yes |
| 20 | Battery and storage | 1 | No |
| 21 | Set radio tuning | 1 | Yes |
| 22 | Firmware information | 1 | No |
| 23 | Export private identity | 1 | Yes |
| 24 | Import private identity | 1 | Yes |
| 25 | Send raw data | 1 | Yes |
| 26 | Repeater or room login | 1 | Yes |
| 27 | Request remote status | 1 | No |
| 28 | Check room connection | 1 | No |
| 29 | Leave room | 1 | Yes |
| 30 | Find contact by key | 1 | No |
| 31 | Read channel slot | 1 | No |
| 32 | Create or update channel | 1 | Yes |
| 33 | Begin signature | 1 | Yes |
| 34 | Append signature data | 1 | Yes |
| 35 | Finish signature | 1 | Yes |
| 36 | Trace route | 1 | Yes |
| 37 | Change Bluetooth PIN | 1 | Yes |
| 38 | Contact and privacy policies | 1 | Yes |
| 39 | Read telemetry | 1 | No |
| 40 | Read sensor settings | 1 | No |
| 41 | Set sensor setting | 1 | Yes |
| 42 | Last advert route | 1 | No |
| 43 | Read radio tuning | 1 | No |
| 50 | Request remote binary data | 1 | Yes |
| 51 | Factory reset radio | 1 | Yes |
| 52 | Discover contact path | 1 | Yes |
| 54 | Override flood scope | 8 | Yes |
| 55 | Send control data nearby | 8 | Yes |
| 56 | Read statistics | 8 | No |
| 57 | Anonymous remote request | 8 | Yes |
| 58 | Automatic contact policy | 8 | Yes |
| 59 | Read automatic contact policy | 8 | No |
| 60 | Allowed repeat frequencies | 9 | No |
| 61 | Path hash size | 10 | Yes |
| 62 | Send channel data | 11 | Yes |
| 63 | Set default flood scope | 12 | Yes |
| 64 | Read default flood scope | 12 | No |
| 65 | Send raw LoRa packet | 13 | Yes |
| 66 | Local radio console | 14 | Yes |

## Validation

- Host C++ fixtures use the real protocol engine with AddressSanitizer and UndefinedBehaviorSanitizer: boot sequence, multiple channel slots, legacy/V3/signed messages, async pushes interleaved with commands, notification-before-write ordering, direct ACKs, channel acceptance, timeouts, prefix collisions, queue bounds and every opcode catalog entry.
- A deterministic corpus exercises every response byte at every length from zero through oversize; malformed packets cannot index beyond the frame. Readable LPP conversion is checked independently from its little-endian envelope.
- Native LVGL 8.4 tests compile the actual UI helpers and MeshCore workspace: touchscreen editor bounds, two-step mutation confirmation, channel wire command, compose/send, lists/history, repeated open/close, firmware-gated tools and the existing Bluetooth PIN keyboard regression.
- Patcher idempotence, DMA alignment, JavaScript syntax, effective sdkconfig, immutable stable SHA, catalog/manifest identity and full ESP-IDF 5.3.2 build gate publication. ESP-IDF and hardware results must be recorded separately; no successful build implies successful radio operation.

## First physical test

1. Select **v0.7.0 experimental**, leave **Erase** unchecked. Keep v0.6.8 available for rollback.
2. Check touch, Wi-Fi and one manual web screenshot before enabling BLE.
3. More → Radios → Enable BLE → Scan → choose the MeshCore Companion Heltec → enter its Bluetooth PIN if requested.
4. Wait for synchronization to finish. Check real radio name, firmware, contacts and channel slots. There must be no sample MeshCore entries. Radio Tools → Read clock / Battery / Firmware should return current values.
5. With a second radio, receive a channel message and a direct message. Reply through the touchscreen; distinguish radio acceptance from the direct ACK. Check unread counts and touchscreen/web capture responsiveness.
6. Add a spare hashtag/private channel slot and verify the same name/key on another client. Avoid overwriting the radio's existing slots during this test.
7. On an owned repeater/room, test login/status/telemetry/path functions only with credentials and regional radio settings you already use.
8. Disconnect/reconnect and confirm there is no automatic duplicate transmission. Report the device log and the stage/status shown if initialization fails. Return to v0.6.8 if responsiveness regresses.
