# v0.7.7 — Swiping the sidebars (480×320 Waveshare / LVGL 8.4)

**Status:** experimental, pending ESP-IDF CI and real touchscreen testing.

## Gesture contract

| UI | Open | Close | No arrows |
|---|---|---|---|
| Left sidebar (Home, Nodes, Groups, Chats, Map) | Put a finger within ~22px of the **left edge**, then drag right at least 44px | From the inner edge of the **expanded** left rail, drag left at least 44px | Left navigation remains visible as 40px icon strip when collapsed |
| Right sidebar (Chats, Map only) | Put a finger within ~23px of the **right edge**, then drag left at least 44px | From the inner edge of the **expanded** right rail, drag right at least 44px | Right actions remain visible as 40px clickable icons when collapsed |
| Home, Nodes, Groups | — | — | Right rail remains absent, no redundant filters |
| Settings and Radios | Left navigation always present (same swipe contract) | — | Primary navigation always visible; right rail absent |

Swipe interpretation is activated **only** if the finger originates on the relevant edge or the expanded rail's inner 38px. It requires the horizontal travel to be at least twice the vertical travel, lasts at most 1.8 seconds, and is evaluated **once on release**, so the LVGL press handler is never interrupted by deleting/rebuilding widgets. This is a swipe-to-snap interface, **not** a continuously-following drawer.

The gestures are disabled during Wi-Fi editor, BLE PIN keyboard, Quick Settings shade, full-screen BLE scanner and information dialogs, and are disabled on the header. Vertical scrolling of Messages, Nodes, Map panels, and right Map actions remains unchanged. An ordinary tap must keep working as before.

The existing arrow buttons and their 39px reserved row are removed completely. Left six navigation icons are spaced vertically inside the full 286px rail. The right action list begins at the top, retaining scrolling for all Map actions.

No BLE/Wi-Fi/GNSS/LoRa/network-side effect is permitted. Remote web interface remains read-only screenshots and never controls gestures. The official v0.7.6 SVG-derived brand stays in the top header.

## Implementation

- `meshcrossway_edge_swipe.h`: pure state machine, C++11 compatible and unit-testable without a display, with edge zones, horizontal/vertical thresholds, modal suppression, page change cancellation and one-shot trigger.
- `meshcrossway_ui.h`: an LVGL 8.4 **20ms timer** polls the existing pointer only within the LVGL task. The pinned v8.4 exposes `indev->proc.state` via `lv_hal_indev.h`; do not accidentally migrate to the v9-only `lv_indev_get_state`. No new touch driver or hardware gesture sensor is used. Draw only after release, not while touched.
- `apply_to_waveshare.py`: copies the gesture header into the vendor example along with the UI headers.
- `scripts/test_edge_swipe.py` and `tests/test_edge_swipe.cpp`: all four gestures, short taps, long gestures, vertical swipes, modal, header, disabled rail, wrong-page and source-zone tests under C++11 and C++20.
- `scripts/preflight_display.py`: rejects arrow control regressions and ensures release-based edge gestures.
- Web Flasher: v0.7.7 remains **experimental**. Keep **Erase device unchecked** for normal updates (NVS Wi-Fi/Bluetooth bonds remain intact). v0.6.8 is immutable validated rollback.

## Real-device acceptance matrix

1. Boot v0.7.7 with **Erase unchecked**; inspect screen capture. No left or right arrow icons; six left icons visible in Home compact rail; header brand, IP and version visible.
2. At left edge, swipe **right**: expand 79px labeled navigation. Swipe **left starting near the inner edge of expanded rail**: collapse back to 40px. Confirm tabs still select the correct pages with ordinary taps.
3. On Nodes/Groups, verify absence of redundant right rails and vertical scrolling does not open any sidebar.
4. On Chat and Map, swipe **left from rightmost 23px**: expand contextual actions to 125px. Swipe **right from expanded bar's inner edge**: collapse. Every action in both states must work; scroll Map actions to the bottom.
5. On Home, Nodes, Groups and Settings, a right-edge swipe does **nothing**; Settings and Radios always retain the **left primary navigation** and never duplicate Home with a standalone HOME button.
6. Enter Wi-Fi editor and BLE PIN keyboard; drag from either edge. No menu should open or dialog disappear. Test Quick Settings and info dialogs similarly.
7. Scroll chats vertically, drag map content, tap nav and action buttons: no unintended menu toggles. Verify taps near screen edge don't trigger a swipe.
8. Stress at least 30 alternations per side while MeshCore BLE session, Wi-Fi STA/AP, SD and screenshot server run. Verify no LVGL invalid-obj assert, freeze, watchdog, SPI LCD queue exhaustion or heap regression.
9. The firmware's paired radio and Wi-Fi reconnect after power cycle without manual PIN or credentials when bonds are valid. There is no Meshtastic↔MeshCore message bridge.

CI success is necessary but **not** evidence of physical gesture behavior on the board.

## v0.7.7 refinement: navigation and status bar (user screenshot review)

- **Left rail never disappears**, including Settings and its Radios subpage. Left swipe expansion and the six primary nav icons are consistent on every non-modal page. The extra HOME button is removed from the Settings card because Home exists in primary navigation.
- **Right rail uses the same visual colors** as the left primary navigation: orange text/glyphs on dark panels, dark text on orange when selected. Maps and Chat remain the only pages with a contextual right rail.
- Top bar preserves the official 180px logo. It no longer displays Wi-Fi IP. Instead the status area shows: Wi-Fi radio glyph colored only when the station has a real IP, Bluetooth glyph labeled **MT** when the single active BLE session has a Meshtastic identity, Bluetooth glyph labeled **MC** when the BLE MeshCore session is READY and synchronized, and a battery glyph with percentage.
- **Never imply simultaneous dual-BLE functionality:** The current firmware uses exactly one connection handle. The MT/MC icons are independent status *indicators*, not an implementation of two concurrent sessions. Unsupported/inactive icon remains muted.
- The battery is read from the **onboard Waveshare AXP2101** via the existing initialized `i2c_device`. Chip ID `0x03`, presence/VBUS `0x00`, charging status `0x01`, SOC `0xA4`: **READ ONLY** through `i2c_master_transmit_receive`. No new device instance, PMU register writes, rail settings, current limits or eFuse changes.
- The worker runs outside LVGL at 15s intervals with a bounded bus timeout. Unknown/absent battery displays `--` instead of a fabricated `0%`. The lightning icon appears only during active charging; USB when externally powered but not currently charging. The AXP percentage is an approximation, not laboratory-calibrated fuel quantity.
- ESP-IDF host tests include battery state (charging, discharging, full, unknown, battery absent, bad chip ID) and fail closed. If the PMU or I2C read fails, the header keeps working and battery falls back to `--`.

### Additional on-board acceptance

10. On Settings and Radios verify left navigation is always visible, expandable by gesture and clickable. Settings buttons do not overlap at 40px compact and 79px expanded.
11. Right Chat and Map icons remain orange in both states (selected ones orange background/dark glyph). No duplicate filters in Nodes/Groups.
12. Top header shows no IP; Wi-Fi icon goes muted when disconnected, colored only after station gets an IP. MT and MC indicators follow **actual** BLE session identity and never show both connected on the current one-session implementation.
13. Connect a Li-ion cell to the Waveshare battery header and inspect battery percentage. Connect USB: verify charging lightning indicator only while truly charging; at full/standby show USB indication. Disconnect the battery: percentage must display `--`. Compare with the vendor's AXP2101 demo; percentage may fluctuate under load.
14. While exercising BLE pairing, touch and screenshots, verify periodic PMU polling causes no I2C timeouts, LVGL watchdog or touch lag. Check no regulator or charging configuration changes.


## v0.7.7 chat readability: colored sender + receipt time + route hops

A radio screenshot showed `RX / RECEIVED / 1793246599` above every received chat message. This is diagnostic metadata and must **never** appear in the normal chat. Instead:

- First line: **sender display name** in a deterministic color chosen from six accessible colors. The same sender has the same color across messages; colors may repeat for different names. The local account label is **You** in orange.
- Middle: actual message text with read-only UTF-8/emoji rendering. For a channel message matching the conventional `Name: message` prefix, display **Name** only as a visual label and keep just **message** in the body. The source packet and stored UTF-8 bytes are unchanged. A user-supplied channel name is **not cryptographically verified**, so do not imply authentication; if no recognizable prefix, display **Unknown sender** with the original text intact.
- Last line: subtle metadata `HH:MM UTC | N hops`. `observed_at` is recorded when the MeshCrossway client retrieves the message over BLE, using the companion radio's clock. **It is not guaranteed to be the LoRa airtime or original RF reception**, especially for queued messages. If radio time has not been synchronized, display `--:--`, never invent one.
- Decode MeshCore received path according to its encoded `path_len`: bottom six bits are hop/hash count; upper two bits describe path hash size. A received value of `0xFF` means **Direct route**, **not** 255 hops. Invalid/reserved values display `Hops --`. Outgoing messages retain their actual delivery states.
- The protocol's own sender-supplied Unix `timestamp` is preserved for anti-duplication. A separate `observed_at` field is introduced in the existing in-memory message structure. No modifications to radio commands, bonding, packet transport, NVS, SD backups, eFuse or network separation.
- Implement as a small read-only `meshcrossway_chat_presentation.h` helper with independent C++11/C++20 unit tests for colon-prefix parsing, unknown names, emoji-prefix UTF-8, UTC conversion, hop encoding, direct route and invalid routes.

### Chat screenshot acceptance

15. Open a busy MeshCore channel with multiple senders. There is **no** `RX / RECEIVED / [epoch]` label or raw signer ID on message cards. Sender name uses the same color across consecutive messages and different names are visually distinguishable where palette permits.
16. With `CO-PW-HQ-MC01: @[emoji] HYPERSPACE...`, the header says `CO-PW-HQ-MC01` and body begins `@[emoji] HYPERSPACE...`; the original message and emoji bytes remain intact in the in-memory protocol store.
17. Show `HH:MM UTC` only when the companion radio has provided a valid clock, otherwise `--:--`. Compare actual BLE observation against a nearby UTC reference; there is deliberately no unconfigured local time zone.
18. For flooded received routes verify numeric hops from the decoded path count; for direct `0xFF`, show `Direct route` rather than `255 hops`. For invalid path show `Hops --`. Outgoing messages show actual queued/accepted/delivered statuses.
19. Test 10+ messages with long text, multiline text, accent/emoji sender names and both compact/expanded right sidebar. Footer must not overlap text or clip against sidebars; normal scrolling and screen capture stay stable.


## v0.7.7 visual regression — compact emoji + unavailable glyphs

- All **93 existing colored Noto emoji glyphs** are rendered as **14×14px pictures inside a 16px font line box** (previously 20×20px). This uses a bounded, tested 20→14 indexed-4 palette-preserving conversion in PSRAM, without modifying the existing source atlas or its two verified SHA-256 digests.
- Where a Unicode emoji or composed emoji is not in the registered set, render a **single `$`** in its place. A literal `[emoji]` placeholder arriving from another client is also shown as `$` (visual-only). When the LVGL image font cannot load, **all** emoji become `$`, including otherwise recognized ones; never output PUA glyphs without a corresponding image.
- The original text in the radio's in-memory messages, sender names, contact keys and transmitted packets must **not change**. If a remote client has already replaced the actual Unicode with the literal string `[emoji]`, the original drawing **cannot be recovered** and the user sees `$` instead.
- Emoji conversion applies to chat message body, sender titles, Nodes/Groups names, contact details, radio name on Home and dialog titles; all must match normal text scale.
- Verify mixed UTF-8 and emoji on actual Waveshare screen in compact/expanded bars. The replacement must not break text alignment, wrap or scroll and the same sender with unknown emoji still has readable name. Fallback for invalid UTF-8 bytes stays `?` to distinguish malformed encoding from unsupported emoji.

## v0.7.7 T-Deck / MeshCore Companion compatibility and safe diagnostics

- The firmware previously rejected documented `PACKET_OK (0x00)` with an optional 32-bit return value (5 bytes), and a `PACKET_ERROR (0x01)` without its optional error code (1 byte). Accept **only** the documented 1/5-byte and 1/2-byte formats respectively. Channel data packet `0x1B` can have an 8-byte header without payload; accept at least 8 bytes.
- All other frame bounds, timeouts, handshake checks, command matching and radio network separation remain strict. Invalid frames still cause fail-safe disconnect; **do not blindly accept arbitrary unknown frames**, which might mask an incompatible firmware or corrupted payload.
- On a malformed frame, the screen reports **only** `frame 0xXX`, received byte length, pending command number and connection phase. A firmware reporting protocol version older than 3 is explicitly identified as incompatible. Serial diagnostics print only code and length — **no raw packet, text, keys, channel secrets or PINs**.
- The T-Deck must be running **MeshCore BLE Companion** firmware, not standalone T-Deck UI or Meshtastic firmware. As the user's screenshot did not include packet type/size, the exact offending frame is **not yet proven**. If it recurs on v0.7.7, use the new safe code/length to investigate and fix that exact format without changing unrelated radios.
- Test fixtures cover valid short/extended acknowledgments, datagrams with no payload, truncated known frames, protocol v2 rejection, and the existing full bootstrap/contacts/messages corpus.

### Additional physical acceptance
20. Confirm a 14px glyph (approximately the text height) in the Home card, Nodes contact names and received chat. Send an unregistered emoji and an explicit literal `[emoji]`; both show `$` without changing stored/forwarded Unicode.
21. Pair a T-Deck flashed with MeshCore **BLE Companion**: inspect handshake, contact loading and channels; verify no false “malformed” errors for the documented response variants. If rejected, capture ONLY diagnostic `0xXX` code, length, pending command and version (do not share raw packets). No hard-coded acceptance of non-Companion devices.
22. Reboot with normal erase **unchecked**. Confirm saved Wi-Fi and Bluetooth pairings, UI and MeshCore chat work without regression.
