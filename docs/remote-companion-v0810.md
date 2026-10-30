# Remote companion and last-radio resume — v0.8.10 candidate

v0.8.9 is the owner-confirmed stable baseline, preserved as the exact published binary. See [stable record](stable-v089.md). This candidate requires physical acceptance before replacing it.

## On the device

After safe startup (about 15 seconds), Bluetooth attempts the last successfully authenticated radio directly by its saved identity, for either MeshCore or Meshtastic. A failed attempt is followed by a scan on the next retry, alternating at 30-second intervals. Automatic discovery reserves a slot for the saved radio even when eight nearby peers have already filled the list. Advertised names are never used as identities. A deliberate disconnect stops retries until reboot. A new device without Bluetooth intent remains off.

Secure, authenticated peers are remembered even if the radio does not retain a bond. Native PIN authentication still applies; if keys have been erased, the radio changed its identity, or it requests a new PIN, finish pairing on the device. No password or PIN is guessed. Existing NVS data is read without a format migration.

## Remote access

Connect to the device's local Wi-Fi address. All assets, including the exact official SVG logo reused from the landing page, are served from firmware. No external fonts, scripts or libraries are required at runtime.

- **Messages:** select MeshCore or Meshtastic, then a contact/channel. Read the newest 40 messages in the loaded conversation, with sender, time and native delivery status. A notice counts older messages. Conversation lists have 32 entries per page; Meshtastic DMs retain their native channel. Local drafts stay in the browser's memory while switching conversations; page reload discards unsent drafts. Sends require an explicit button press, use the native byte limit, and are never automatically retried after an ambiguous connection failure.
- **Tracks & recording:** existing visibility, color, names, deletion, GPX downloads and preset editing remain available. Start chooses a preset/color; controls follow recording state and Finish accepts a name.
- **Device screen:** manual capture with a five-second cooldown, plus PNG download. Physical touch remains available. No automatic screenshots or remote touch are introduced.
- **Service & data:** diagnostic fields and a JSON download, separated from normal use. Diagnostic downloads do not contain message text or radio/channel keys.

Only **one BLE radio session is active at a time**. The web page can display the loaded MC and MT data independently; sending requires the selected network's connected, synchronized radio. It cannot connect a second radio, bridge, forward, convert or broadcast across networks. Older radio archives are not searched or replayed by HTTP.

## Concurrency and memory

The existing LVGL task owns both message clients. HTTP submits one bounded request through an atomic mailbox; a 50 ms owner timer reads models or invokes their existing send guards. HTTP never calls LVGL or mutates a protocol client. The single 96 KiB response buffer and request workspace are allocated explicitly in PSRAM during UI initialization. Responses are bounded, JSON-escaped and written via textContent by the page. Opaque per-boot salted SHA-256 conversation handles bind network, radio and destination/channel identity without exposing channel secrets. Reboot, channel changes, blocked contacts or wrong-network handles prevent sending.

An unconsumed mailbox request is cancelled after 1.5 seconds. A consumed operation completes before HTTP releases its buffer, preventing a timeout from silently queuing a later send. HTTP POST requires the same-origin application header and emits no CORS permissions. Access remains the existing PIN-free local HTTP model: use a trusted network and do not expose port 80 to the Internet.

## Validation and physical acceptance

Host checks exercise actual MC/MT models and the real mailbox/HTTP parsers with ASan/UBSan: correct native queues, isolation, stale identities, channel changes, blocked/offline/oversize rejection, UTF-8, JSON escaping, paging, memory failure and timeout cancellation. BLE fixtures execute real resume selection for both protocols and identity address types, including an already-full scan list. Existing real-LVGL tests cover display, map, recording and message UI regressions.

On hardware: restart with each radio, try radio-off-at-boot then power it on, retry after link loss, manually disconnect, and verify PIN recovery after erased bonds. Test Wi-Fi/BLE coexistence, MC and MT receive/reply, native ACK state, loaded offline history, tracks/presets/GPX, and stack/heap metrics in Service & data. Host tests and CI do not establish RF behavior or physical boot stability.

Browser checks: `node scripts/test_remote_companion.cjs` with Playwright available, optionally `MCX_CHROMIUM_EXECUTABLE=/path/to/chromium`. The test serves mocked local HTTP responses (no radio transmissions), checks separate replies, recording state, captures, diagnostics and mobile overflow, and saves inspection screenshots under `/tmp`. It passed locally with Chromium 153.
