# v0.8.16: independent dual BLE radios and radio configuration

Status: experimental candidate. On 2026-11-01, the owner supplied v0.8.16 screenshots showing both radios connected concurrently on the device and showing radio settings entry points plus the local browser companion working. These captures confirm those functions in the demonstrated setup; they are not exhaustive hardware acceptance. The exact owner-tested v0.8.14 image remains the recommended stable release. This change does not replace its immutable source reference or binary checksum. v0.8.15 message visibility and device-local time fixes remain included.

## Connection behavior

The Core can maintain one Meshtastic PhoneAPI connection and one MeshCore Companion connection concurrently. The Heltec V3 is the owner's initial test hardware, not an allowlisted model. The ESP32-S3/NimBLE configuration already allowed two connections; the old application had one shared connection handle and reset both transports when selecting a peer.

`meshcrossway_ble_link.inc` is instantiated in two separate namespaces. Each owns its connection handle, discovery handles, pairing state, callback epoch and saved peer. Notifications, writes, errors and disconnects stay inside their instance. One controller and host task serve both. Scanning and new connection setup are serialized; an established connection continues carrying traffic while the other is being paired. A peripheral must expose the protocol chosen for its slot; unknown/wrong or ambiguous GATT services are rejected without disconnecting the other slot.

Select **Meshtastic** or **MeshCore** in the device's Radios page, or select that network and **Network** in the browser. Pair each radio separately. Both Home cards and top-bar indicators can be connected together. Disconnect applies only to the selected radio. A controller reset necessarily affects both links.

Authenticated identities are saved separately as `peer_mt` and `peer_mc` in the existing NVS namespace. The previous `last_peer` record is migrated only into its matching network. Bonds and the legacy record are retained. After safe boot, a round-robin worker alternates direct reconnect and identity-matched scanning, with a per-link retry interval. An absent remembered device cannot permanently starve the other. A deliberate disconnect pauses retries for that slot until restart or successful manual reconnect. Pairing dialogs and queued selections bind to the intended network.

No bridging, relaying, message conversion, cross-network sending or shared protocol model is introduced.

## Settings available

Both the device and browser expose network-specific settings. Touch either radio card on Home to open that network's settings directly; the gear icon marks the shortcut. Closing settings returns to Home. Disconnected radios keep the same connection/synchronization checks as the Radios-page settings. Browser: **Messages → network → Network → Radio settings**. Settings remain local to the selected attached radio; Meshtastic admin packets explicitly target that radio's own node with zero mesh hops.

Meshtastic provides 37 groups:

- Core: device, position, power, network, display, LoRa (including region), Bluetooth and security.
- Device UI, identity, eight channels, canned-message text and notification ringtone.
- Modules: MQTT, serial, external notification, store-and-forward, range test, telemetry, canned message, audio, remote hardware, neighbor info, ambient lighting, detection sensor, paxcounter, status message, traffic management, TAK and mesh beacon.

Fields use typed controls, enum choices and hidden secret inputs. Repeated scalar values are comma-separated (strings use one entry per line). Repeated embedded protobuf records are explicitly advanced hexadecimal inputs. Hardware-derived identity fields and deprecated fields are read-only. This is radio configuration, not a firmware updater or a replacement for every administrative action in the official apps; for example, remote-node administration, fixed-position coordinate provisioning and radio firmware flashing are not added here.

The editor first reads a group. Before saving one field it rereads the group and obtains a fresh admin session passkey when the firmware supplies one. A changed baseline or stale editor revision rejects the write. Original unknown protobuf fields and unrelated values are retained. The writer sends one change, waits for the radio's acknowledgement, then reads back that field. BLE write acceptance alone is not reported as saved. NAK, timeout, normalized/different values and disconnection are reported explicitly; writes are never replayed automatically. Changing Bluetooth, radio or power settings can reboot/disconnect the radio before verification; reconnect and read again in that case.

MeshCore reuses the existing typed Companion command catalog on both surfaces, including radio frequency, bandwidth, spreading factor, coding rate, power, identity, channels, Bluetooth PIN, privacy/contact policies, sensors, tuning, flood scopes and supported local console. Commands are gated by the radio's reported protocol revision and by the existing frame/range/MTU validation. MeshCore reports command responses/ACKs; it does not claim universal readback verification for commands without a read operation. Sensitive result frames are hidden in the browser and can be inspected deliberately on the device.

Community **LoRa region presets** fill frequency/BW/SF/CR before confirmation. Optional path-hash recommendations are displayed and configured separately; these are not country-wide automatic operating settings or automatic power changes. Presets are an offline snapshot of the MeshCore API, not a startup network dependency.

## Compatibility and boundaries

Compatibility is based on GATT services, valid protocol framing, synchronization responses and supported commands, not board name. Meshtastic's former hardcoded `2.5+ / major 2` string gate is removed. Required PhoneAPI synchronization fields and the echoed configuration nonce remain mandatory. This does **not** guarantee every historical or future firmware: unknown breaking wire changes, missing settings, disabled/managed administration or incompatible BLE security must still fail clearly. Missing fields retain protobuf defaults; unsupported requested changes cannot be advertised as verified unless the returned field matches.

The Meshtastic wire metadata reference is `meshtastic/protobufs` commit `ffd5ad428da6db4a7ddfe62e06548718cc2603e0`. The implementation uses independent bounded Reader/Writer and patching code; no upstream generated protobuf binding is linked. Additional primary references:

- https://github.com/meshtastic/protobufs/blob/ffd5ad428da6db4a7ddfe62e06548718cc2603e0/meshtastic/admin.proto
- https://github.com/meshtastic/protobufs/blob/ffd5ad428da6db4a7ddfe62e06548718cc2603e0/meshtastic/config.proto
- https://github.com/meshtastic/protobufs/blob/ffd5ad428da6db4a7ddfe62e06548718cc2603e0/meshtastic/module_config.proto
- https://github.com/meshtastic/firmware/blob/master/src/modules/AdminModule.cpp
- https://github.com/meshcore-dev/MeshCore/blob/main/docs/radio_presets.md
- https://api.meshcore.nz/api/v1/config

## Resource and validation design

The settings state and work frames live inside the PSRAM Meshtastic client (about 59 KB total on the host). The HTTP settings mailbox uses a bounded 32 KiB response plus bounded request buffers in PSRAM. HTTP only submits commands and returns completed snapshots; protocol edits and JSON formatting run on the LVGL owner. No settings storage is added to the SD message archive. Secret inputs/passkeys are not written into chat history or diagnostics. SRAM checking now distinguishes writable allocated ELF sections from constant schema tables in flash, retaining the existing 2 KiB chat/history SRAM budget. Existing HTTP, resume and navigation stack guards remain required.

Automated validation:

- Actual two-instance BLE code against an event-driven NimBLE test port: simultaneous destinations, pairing isolation, separate NVS identities, legacy migration, 30 reconnects, stale GATT callbacks and wrong-protocol isolation.
- Settings engine with ASan/UBSan: local-only packets, nested and packed fields, unknown-field preservation, numeric/hex/UTF-8 bounds, stale edits, fresh passkey, readback, NAK, timeout and disconnect.
- Actual HTTP handlers: protocol identity, request validation, bounded allocation failure, firmware gating, sensitive-result redaction and cancellation before owner execution.
- Actual browser functions: enum selection, confirmation, per-network destinations, preserved edits and stale-radio replacement.
- Real LVGL forms: group selection, settings read, enum editor, confirmation, keypad and existing message/map/Home regressions.

Remaining hardware acceptance includes bidirectional receive/send traffic on both radios at once; reconnect each while the other exchanges messages; reboot the Core with either/both radios missing; PIN pairing for each; repeat while refreshing remote tracks/settings; verify region/channel changes and observe minimum task stack/internal heap. Record both radio firmware versions and exact candidate binary identity. The owner screenshots document connected state, device views and browser access, not this full test matrix. Automated tests cannot establish RF coexistence, long-duration stability or compatibility with every radio firmware.
