# Meshtastic / T-Deck radio — v0.7.12

Implemented for the Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 controller. The
T-Deck / T-Deck Plus runs its own Meshtastic firmware and acts as the external
BLE radio. This is not a MeshCrossway firmware image for the T-Deck.

## Setup

1. On the T-Deck, use official Meshtastic firmware for **T Deck**, with an
   antenna and frequency variant appropriate to your existing network. The
   ordinary LCD T-Deck / Plus is not the T-Deck Pro firmware target.
2. For MUI, open its shutdown/reset menu and choose the Bluetooth icon, or
   hold the Meshtastic logo during boot to enter **Bluetooth Programming Mode**.
   **BaseUI** also releases the Client API. On unified 2.7+ firmware, switching
   UI does not require reflashing. MUI normally owns the sole Client API session.
3. Turn **Wi-Fi OFF on the T-Deck** and enable Bluetooth. Close another phone,
   desktop or serial client that might hold the API. The Waveshare's own Wi-Fi
   may stay on; its ESP-IDF coexistence configuration is preserved.
4. Use RANDOM_PIN or FIXED_PIN. MeshCrossway requires an encrypted,
   authenticated link and does not silently downgrade to NO_PIN.
5. Install **v0.7.12 experimental** on the Waveshare using the
   [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/). Leave
   **Erase unchecked**. v0.7.8 remains the immutable owner-tested recovery.
6. Open More > Radios > Enable BLE > Scan, select the T-Deck and enter the
   six-digit PIN displayed by the radio. Wait for configuration, NodeDB and SD
   restore. A BLE connection alone does not enable the MT online indicator.
7. Select MT or ALL in Nodes / Groups / Chats. Choose a node for a direct
   message, or a synchronized channel for a channel message. SEND is a separate
   action from keyboard DONE. Region and TX must already be configured on the
   radio. View native configuration and compatibility in MESHTASTIC STATUS.

## Implemented behavior and boundaries

| Area | Implementation / check |
|---|---|
| Identity | Discover the actual Meshtastic GATT service; advertised names are only hints. Refuse a device exposing both native protocol services ambiguously. Validate characteristic properties and FromNum CCCD. |
| GATT | Service `6ba1b218-15a8-461f-9fa8-5dcae273eafd`; acknowledged ToRadio writes; FromNum notifications are four-byte counters; read FromRadio until empty. Never parse FromNum as a message. Standard asynchronous long reads assemble up to 512 bytes with contiguous offsets. Request MTU 517 and require at least 512; smaller MTUs fail with an actionable message. |
| Session | Full configuration request with a random, echoed nonce; avoid reserved config-only/node-only nonces. Accept completion only for this request, with MyNodeInfo, metadata and LoRa config present. Firmware must identify as Meshtastic 2.5+ within major 2. Configuration has a 90-second deadline; heartbeat every 30 seconds; fallback reads every two seconds recover missed notifications. |
| Version reporting | Firmware version, hardware model, negotiated MTU and min_app_version are recorded. min_app_version is a phone-platform build code, not a MeshCrossway semver value; no invented numerical equivalence is used. Unknown protobuf fields are skipped, but malformed lengths/wire types fail. New firmware major versions are blocked for review. |
| Nodes/channels | Up to 128 nodes and eight native channel slots. Active primary/secondary channels, names and settings fingerprints come from the radio. Preserve live channel configuration during history restore. Full node table retains an explicit dropped-node counter instead of evicting unpredictably. |
| Messaging | TEXT_MESSAGE_APP UTF-8, up to 237 bytes, direct and channel messages. Native sender/destination/channel/packet IDs, duplicate suppression, queue status, matched ACK/NAK and timeout. Destination node availability, radio identity and channel fingerprint are rechecked at SEND. No automatic application retry. |
| Encryption | Request native PKI for direct messages when metadata advertises support and the destination public key is known. The radio performs cryptography. Other DMs use the selected channel's native encryption. This is not a promise that every DM is PKI encrypted. Private keys and raw channel PSKs are not retained by the new MT store. |
| Delivery | Bluetooth accepted, radio accepted, remote ACK, rejected and unconfirmed are distinct. A channel's local ACK never claims delivery to everybody. A remote ACK does not mean read or authenticated human identity. A failed write/reconnect/timeout never silently resends. |
| Position/metrics | Decode NodeInfo and live node/user, position and device-metrics application packets. Show last heard, coordinates, altitude, battery, voltage, utilization and key availability in node details. Zero coordinates remain valid when both fields are present. This does not implement the future navigation/basemap engine. |
| UI | Native MT nodes, groups, conversations, composer, reusable custom keyboard, per-conversation drafts, radio status and compatibility. ALL includes MC and MT rows with network labels; native callbacks select the corresponding protocol. There is one active BLE radio session in this release. Concurrent MC+MT sessions remain deferred. |
| Local history | MT-specific `mt-<bonded-identity>.bin`, `.bak`, `.tmp` under `/sdcard/mcx-history`, separate from existing MC journals. Version/size/identity/checksum checks, full flushed snapshots, backup recovery, bounded RAM model and no-op suppression. Up to 96 messages and 16 drafts; up to about 150 KiB across three MT images per radio. Plaintext as previously authorized. |
| SD ownership | The existing single low-priority SD worker handles both protocol stores. No file operations or blocking BLE reads run on LVGL. State/mailbox/snapshots use explicit PSRAM with no internal-RAM fallback. Normal snapshots coalesce changes for three seconds; failed writes retry after at least three seconds. |
| Recovery | Restore before radio message backlog; keep newer same-radio RAM across reconnect. Outgoing saved states become uncertain, never a reconstructed TX queue. Block switching MT radios while the previous radio's data is unsaved. Last bonded MT history can load with its radio offline. Invalid/I/O-failed archives are not authorized for destructive replacement. |
| Exclusions | No bridge, conversion, forwarding, shared radio keys or cross-network sending. No MQTT client proxy, raw packet retransmission, remote administration/reset, channel-key editing, region changes or automatic position sharing. Configure those radio settings with the official Meshtastic client. |

## Verification and physical acceptance

`python3 scripts/test_meshtastic.py` compiles the production codec/session/store
under ASan/UBSan and the production mailbox/SD handoff with a 4096-byte stack-frame
guard. It checks golden wire bytes, completion nonces, destination/channel/radio
isolation, PKI fields, channel versus direct ACK, NAK, UTF-8 byte limits, zero
coordinates, duplicate suppression, timeouts, interrupted SD images, drafts,
reconnect and 20,000 malformed frames. The parser performs no heap allocations.
On the host ABI the Client is 51,448 bytes and Archive is 50,712 bytes; device
sizes are checked by the build/ELF evidence rather than assumed identical.

The existing real-LVGL pairing/UI fixture also exercises MT node/channel
selection, composer/keyboard geometry, native send, disconnected SEND gating,
draft preservation and compatibility dialog. Existing MC, SD, battery, emoji,
gesture, configuration and installer regression checks remain mandatory.
The authoritative ESP-IDF 5.3.2 compile, generated sdkconfig, ELF/static-memory
check and embedded-version checks run in PR preflight before publication.

**Hardware qualification is pending.** In particular, test T-Deck PIN/bond
reconnect, actual MTU and multi-part GATT read behavior, channel and direct
send/receive/ACK with another Meshtastic peer, at least 30 minutes of traffic
with the Waveshare Wi-Fi on, reboot with radio off, channel configuration change,
SD absent startup and restoration. Wait for SD saved before power removal.
Historical messages must never transmit automatically. Host tests cannot
certify RF compatibility, power integrity, firmware forks or absence of resets.

## Primary references and provenance

Research combines the official protocol schema, firmware, Python client and
device documentation. All new adapter/codec/UI/storage code is original;
no GPL source, `.proto` file or generated binding is distributed or linked.
Meshtastic firmware remains separately installed on the user's radio.

- [Official T-Deck hardware and firmware targets](https://meshtastic.org/docs/hardware/devices/lilygo/tdeck/).
- [Official MUI / Client API ownership and Bluetooth/BaseUI modes](https://meshtastic.org/docs/configuration/device-uis/meshtasticui/).
- [Official Bluetooth settings / Wi-Fi precedence](https://meshtastic.org/docs/configuration/radio/bluetooth/).
- [PhoneAPI schema, reviewed `ffd5ad428da6db4a7ddfe62e06548718cc2603e0`](https://github.com/meshtastic/protobufs/blob/ffd5ad428da6db4a7ddfe62e06548718cc2603e0/meshtastic/mesh.proto), plus channel/config/telemetry definitions at that revision (GPL-3.0 research only).
- [Firmware PhoneAPI, reviewed `6d41e279f1f51bd59f687b9d441c1bf47b1594fc`](https://github.com/meshtastic/firmware/blob/6d41e279f1f51bd59f687b9d441c1bf47b1594fc/src/mesh/PhoneAPI.cpp) and [NimBLE service behavior](https://github.com/meshtastic/firmware/blob/6d41e279f1f51bd59f687b9d441c1bf47b1594fc/src/nimble/NimbleBluetooth.cpp) (GPL-3.0 research only).
- [Official Python BLE API behavior](https://github.com/meshtastic/python/blob/master/meshtastic/ble_interface.py) and [packet/PKI behavior](https://github.com/meshtastic/python/blob/master/meshtastic/mesh_interface.py), research only; moving references should be rechecked when updating the adapter.

See the [dependency review](third-party-licenses.md) and [release policy](licensing.md).
