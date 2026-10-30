**v0.8.4 experimental:** local tracks/GPX and POIs, map presets and revised controls. [Guide](../../docs/gps-navigation-v084.md). Hardware acceptance pending; v0.8.2 is the owner-tested recovery.

**v0.8.1 candidate (experimental):** GPS header status, persistent local GPS settings and real XYZ PNG maps. [Configuration, SD tiles and limits](../../docs/maps-gps-v081.md). Map hardware acceptance pending; v0.7.8 recovery unchanged.

**v0.7.14 experimental:** PR #65 improves Meshtastic PhoneAPI GATT compatibility and RX/send diagnostics. Physical Heltec V3 RX/TX validation is pending; do not treat missing RX as confirmed fixed. Install without Erase. v0.7.8 remains the unchanged recovery.

# MeshCrossway display-only firmware prototype

**Current v0.7.13 (experimental):** PR #63 adds queued touchscreen sampling, retained PSRAM chat cards, scroll-position preservation, and earlier offline history restore. Physical touch/SD/BLE testing is pending. Inherits v0.7.12 protocol support.  independent MeshCore Companion and Meshtastic PhoneAPI clients, one active BLE radio at a time. T-Deck / Plus is supported as an external Meshtastic radio with compatibility gates; physical qualification remains pending. The display-only descriptions below record earlier milestones. [Meshtastic setup, scope and acceptance](../../docs/meshtastic-v0712.md). v0.7.8 stays the owner-tested recovery image.


> **Modular BLE software-first development (2026-10-28):** This firmware remains a **display-only / Wi-Fi screen-viewer prototype**, not a finished BLE Central implementation. The final MeshCrossway Core must connect to both autonomous Meshtastic and MeshCore Radio Pods over BLE. Retain this firmware's current behavior for UI tests, but evaluate Wi-Fi AP and BLE radio coexistence/memory before simultaneous operation. See [approved architecture](../../docs/modular-architecture.md). No PCB or wiring changes are required.

This is an **early UI-only hardware test**, using the official Waveshare `ESP32-S3-Touch-LCD-3.5` ESP-IDF 8.4 LVGL example as its board-support baseline. No XIAO modules, GNSS receiver, antennas, or radio firmware are required. No network traffic is transmitted and all signal/satellite data is fictional.

## Windows / ESP-IDF quick start

1. Install Espressif ESP-IDF (with the ESP-IDF PowerShell or Command Prompt). The upstream example specifies IDF >=5.1; use a release supported by the Waveshare project.
2. In your MeshCrossway working copy, run:
   ```powershell
   git clone https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5.git vendor/ESP32-S3-Touch-LCD-3.5
   python firmware/display-prototype/apply_to_waveshare.py
   cd vendor/ESP32-S3-Touch-LCD-3.5/examples/esp-idf/02_lvgl_example
   idf.py set-target esp32s3
   idf.py build
   idf.py -p COM5 flash monitor
   ```
   Replace **COM5** with the serial port shown in Windows Device Manager. Use a data-capable USB cable. Do not connect the unfinished carrier PCB.
3. Verify startup screen, landscape orientation, all five menu buttons, and touch alignment.

The script patches the cloned upstream **example only**, sets rotation 90 degrees for 480x320, replaces `lv_demo_widgets()` with `meshcrossway::init()`, and copies the UI header. Re-running it refreshes the UI header. The upstream Waveshare code owns I2C, expander, LCD, touch, backlight, PSRAM buffer, and LVGL port.

### Important compatibility notes
- This is designed for the non-B **Waveshare ESP32-S3-Touch-LCD-3.5 / -C** example and **LVGL 8.x**. Before flashing, confirm the product silkscreen and manufacturer example match the actual purchased board. Do **not** use a 3.5B pinout/example interchangeably.
- The exact font scaling, alignment, and touch rotation must be verified on the physical screen; no hardware flash or successful build has been claimed yet.
- The UI is a first-pass navigation/bring-up shell, **not yet an exact LVGL port** of the existing browser simulator. Future work is to progressively match the browser UI and implement screen-level state/providers.

### Acceptance checklist
- [ ] Firmware builds using vendor example and selected ESP-IDF toolchain
- [ ] Device boots with MeshCrossway header and DEMO footer
- [ ] Landscape rendering fills 480x320
- [ ] All five navigation buttons work, with correct touch coordinates
- [ ] No restarts, freezes, or memory errors in 10 minutes
- [ ] No unexpected UART/radio traffic (no modules connected)

Browser UI reference: https://renbrant.github.io/MeshCrossway/
Vendor source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/main/examples/esp-idf/02_lvgl_example


## UI v0.2 — graphite and orange (October 28, 2026)

The LVGL UI now uses the MeshCrossway graphite/orange visual language with a compact left navigation rail, 43px top status header, 35px bottom demo status, and clipped-safe cards. The reference is WADAMESH's field-focused touchscreen information hierarchy, but the UI is original and does not copy WADAMESH code/assets. All displayed radio/position values are clearly demo data. Hardware verified for v0.1 on Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0; **v0.2 still requires physical testing** after web-flashing. The GitHub Pages flasher build pipeline triggers automatically from changes in this folder. Before installing, confirm the Actions run succeeded and the displayed manifest version matches the latest commit.

## UI v0.3 — real host diagnostics

The Settings screen now displays actual ESP32-S3 uptime and free heap (refreshed every 5 seconds), while all radio and GNSS displays remain explicitly simulated. Also shortens the overview Crossway card text to prevent overlap. Palette and navigation remain unchanged. **Physical validation still required on Rev2.0**, including Settings update cadence, heap behavior and navigation stability. Screen brightness, SD and UART remain future milestones; no hardware driver or radio changes in this build.

## v0.4 — Wi-Fi development screenshot server (experimental)

This build adds a local Wi-Fi SoftAP (`MeshCrossway-Dev`) with WPA2 password `meshcrossway-dev`. Connect a laptop to that network and browse to `http://192.168.4.1/` to view real LVGL screen snapshots, refreshed every two seconds, and save a PNG screenshot. The board is not connected to the home router in this first iteration; Internet access on the laptop may switch off while connected to the AP. The browser page is **read-only** — touch interaction stays on the physical display.

Security: this is a temporary development-only AP with a known documented password. Do not leave the device powered up in public spaces or use this fixed credential for final production. The AP is isolated from the public Internet by design. Future work: secure on-device Wi-Fi provisioning (STA mode), changeable credentials, authentication and remote touch control.

Before flashing, wait for GitHub Actions to report a successful build and confirm the Web Flasher announces v0.4.0. Verify AP connection, screenshot colors, rendering, touch responsiveness and memory after repeated captures. The previously hardware-tested v0.3 remains the recovery reference.

## v0.5.0 — Join home Wi-Fi and operate the screen remotely (experimental)

On the physical **Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0**, open **Settings > CONNECT / CHANGE HOME WI-FI**, enter the exact 2.4 GHz network SSID and WPA2/WPA3-Personal password using the touchscreen keyboard, then press **CONNECT**. The ESP-IDF Wi-Fi station configuration is stored in device NVS, not committed to GitHub. The device retries its saved Wi-Fi automatically on reboot. When DHCP completes, Settings displays its local IP address. Keep your computer on the **same Wi-Fi/LAN** and navigate to **http://DEVICE_IP/**.

For remote access in **v0.6.8**, open `http://DEVICE_IP/` and press **Capture screen**. No View PIN is required. Captures remain manual, limited to one every five seconds, and read only. Remote touch is disabled; use the physical touchscreen. The six-digit **Bluetooth pairing PIN** is separate and remains required by radios that request it.

Development-only **AP+STA**: Once connected, use the home LAN IP. The MeshCrossway-Dev AP at 192.168.4.1 remains as a recovery fallback. HTTP is unencrypted; use trusted local Wi-Fi only and never expose TCP port 80 to the Internet. Anyone with access to the device's HTTP server can request a screenshot. Standard ESP-IDF NVS is not encrypted unless separately configured.

ESP32-S3 supports 2.4 GHz Wi-Fi. Guest network client isolation may prevent browser access even with Wi-Fi connected. **Physical testing required:** open the web page without a PIN, request a screenshot, verify the five-second limit, screenshot colors and touchscreen responsiveness with BLE enabled.

## v0.5.1 — settings redraw regression candidate

The v0.5 Settings refresh destroyed/recreated every Settings card every 3 seconds. v0.5.1 keeps LVGL cards and touch targets alive, updating only their existing uptime/heap/IP label text when values change. This avoids whole-subtree invalidation and preserves the LAN Wi-Fi and remote touch features. **This is a candidate, not a confirmed physical fix**: repeated reboot, Wi-Fi AP+STA power or memory pressure may also produce apparent screen flashing. Keep v0.4.0 as the tested recovery version. Check v0.5.1 by opening Settings for at least 5 minutes, observing other screens, and testing Wi-Fi credentials, DHCP IP, remote clicks, physical touchscreen, and PNG. If the device still flashes, gather serial boot/restart logs before further changes.

## v0.6.3 — critical boot regression (DO NOT INSTALL)

On Oct 28, 2026 the owner installed v0.6.3 twice: boot logo/UI `DEV 0.6.3` displayed, but touch froze and Wi-Fi stayed `waiting`. Reflashing v0.6.2 restored normal operation. The underlying cause has **not been established** (serial logs were not captured); starting NimBLE unconditionally at boot is a leading hypothesis. The Web Flasher must block this known-broken image. [Issue #30](https://github.com/Renbrant/MeshCrossway/issues/30).

## v0.6.4 — staged BLE initialization (experimental, NOT hardware tested)

The default boot **does not call NimBLE initialization at all**. Wi-Fi AP/STA, LVGL, SD backup, and manual read-only screen viewer should start unchanged. Use **More > Radios > ENABLE BLE / TEST** only *after* confirming that touch and Wi-Fi are functional. NimBLE initialization runs in a separate FreeRTOS worker; the radio page reports startup stages and GATT discovery. Startup on-demand is a diagnostic isolation measure, **not a proven fix**: enabling BLE may reproduce the v0.6.3 freeze. Do not use this image for critical communications.

Test sequence:
1. Flash v0.6.4 without Erase; boot normally with Heltec initially off or unconnected; test all tabs, Wi-Fi IP, manual viewer and Settings.
2. Open Radios, verify the state is `DISABLED`. Confirm it remains responsive for several minutes **before pressing ENABLE BLE**.
3. Enable BLE and note whether UI stops responding or Wi-Fi disconnects. A serial monitor at 115200 baud (and boot reset reason) is essential if it freezes.
4. If the link initializes successfully, scan and select the Heltec V3 **MeshCore Companion Bluetooth**, enter PIN shown by the device. No messages or MeshCore commands are sent by this candidate.
5. If any freeze occurs, reinstall the tested v0.6.2 image; do not erase unless necessary.

## v0.6.5 — LVGL / SPI coexistence diagnostic (EXPERIMENTAL / not tested)

The owner provided detailed v0.6.4 Web Flasher serial logs. BLE startup **completed** (`nimble_port_init result=0`, host created), immediately followed by `lcd_panel.io.spi: spi transmit (queue) color failed`. The task watchdog repeatedly reported `IDLE0` starved with `taskLVGL` running on CPU0; CPU1 was idle. The triggering issue is in LCD SPI transfers / LVGL progress, not a failed BLE connection or MeshCore pairing. Root cause of the SPI queue error is not yet proven.

This candidate stays on the pinned Waveshare BSP and ESP-IDF 5.3.2, and tests only a targeted resource adjustment:
- Pins LVGL task to CPU1 rather than an unpinned task that was observed on CPU0.
- Decreases LCD SPI PCLK 80 MHz to 40 MHz and transaction depth 10 to 6.
- Keeps Wi-Fi CPU0 and pins NimBLE host/controller CPU1 (coexistence enabled).
- Prints `[MCX-MEM]` with free internal 8-bit SRAM, largest internal block and DMA-capable SRAM before/after NimBLE startup, separately from PSRAM-inclusive total heap.

The documented tested rollback is **v0.6.2**. Versions **v0.6.3** and **v0.6.4** are blocked from the Web Flasher selector. After flashing v0.6.5, check touch/Wi-Fi before choosing **More → Radios → ENABLE BLE / TEST**, and capture the **Web Flasher Logs**. Do not assume the new candidate fixes the display without hardware testing.

Issue tracker: https://github.com/Renbrant/MeshCrossway/issues/30

## v0.7.0 — MeshCore Companion (experimental)

Implements the official Nordic-UART Companion protocol over authenticated BLE: negotiated MTU, discovered RX/TX/CCCD, notification subscription, serialized commands, automatic identity/contact/channel synchronization and message draining. Nodes, Groups and Chat use live MeshCore data. More → Radios → MeshCore Tools exposes typed forms for all 59 current official Companion command opcodes, with firmware-version gates and explicit confirmation for mutations. See [coverage, sources, limits and hardware test](../../docs/meshcore-v070.md).

The owner-confirmed **v0.6.8** UI/manual-screenshot image is now the immutable default and rollback. **v0.7.0 has not been validated on hardware**. Message history is bounded in RAM; reconnect discards the prior radio session. No automatic resend or automatic radio configuration/location broadcast occurs.

## v0.7.1 — Bluetooth pairing remembered (experimental)

NimBLE stores authenticated Bluetooth **bond keys in ESP32 NVS**, not the six-digit PIN in plain text. Once a radio successfully bonds, manual disconnect/reconnect and restart of the Core should reuse those keys, skipping PIN entry. The Radios page indicates whether a bond was actually established. A radio factory reset, deleted bond, disabled bonding, or erased NVS requires pairing again. Web Flasher **Erase must stay unchecked** to preserve saved pairing. v0.6.8 remains the physical-hardware validated rollback; v0.7.1 still requires real-device verification. This development build does not enable NVS flash encryption.

## v0.7.2 — Automatic Wi-Fi / last bonded BLE radio (experimental)

On boot, the Core first attempts the most recent successfully connected Wi-Fi network, then up to four other remembered WPA2/WPA3-Personal networks, without asking for a password again. Successful networks are stored in a versioned NVS list; a bad password does not evict them. The recovery Wi-Fi AP remains available. It periodically retries when the known networks are out of range. In Wi-Fi settings, selecting a known SSID with an empty password reuses the saved password. The existing plaintext SD Wi-Fi backup is unchanged and is **not** device-bound encryption.

For Bluetooth, after the UI and Wi-Fi server have initialized, a deferred worker checks the last authenticated **bonded** radio identity stored in NVS, starts NimBLE only if such a record exists, scans for that identity and attempts a secure reconnection. It never matches by advertised name alone. A user-requested Disconnect pauses auto-scanning for that boot; a reboot re-enables automatic resume. On first updating from v0.7.1, connect the radio **manually once** to establish the saved last-radio reference, then test auto resume after restarting. Peer-side bonding keys must remain intact. A rotating private BLE address may require additional identity resolution, and no automatic replay of messages occurs. The adapter currently has only **one active BLE connection**, with Meshtastic transport not yet implemented; this does not bridge the two mesh networks.

**Not yet hardware tested.** Keep `Erase device` unchecked to preserve NVS and use owner-verified v0.6.8 as recovery. [Implementation limitations and physical test checklist](../../docs/auto-reconnect-v072.md). The separately planned encrypted microSD security backup is tracked for **v0.7.3** and is not part of this release.

## v0.7.4 — checkered flag glyph and name rendering (experimental)

The color emoji renderer now recognizes the checkered flag (`U+1F3C1`, with or without optional `U+FE0F`) and draws its real 20×20 Noto Emoji bitmap rather than the `[emoji]` fallback in MeshCore messages. The same display-only mapping also applies to contact names in node lists/details, chat headings, tool dialogs, and the Home radio name. The underlying transmitted text, keys and contacts remain unmodified. The original 92-emoji atlas checksum is unchanged; a separately hashed 264-byte indexed-4 image supplies glyph #93. C++11/20 tests include the exact reported sender-name pattern.

**Physical hardware verification is still required** for color/spacing, power-cycle and Bluetooth coexistence. v0.6.8 remains the immutable owner-tested rollback, and normal updates must leave Web Flasher **Erase device unchecked**. [Regression and acceptance steps](../../docs/checkered-flag-emoji-v074.md).

## v0.7.7 — swipe-to-expand sidebars (experimental)

The left navigation and the Chat/Map contextual menu now open/close with a horizontal finger gesture from the relevant screen edge or expanded rail. Arrow buttons were removed; compact rails keep clickable icons. This is triggered after releasing the finger and does not interfere with the header, vertical scrolling or PIN/Wi-Fi keyboards. [Swipe UX and on-device acceptance matrix](../../docs/edge-swipe-sidebars-v077.md). **Not yet tested on Waveshare hardware**; keep Web Flasher Erase unchecked to preserve Wi-Fi and BLE bonds.

### v0.7.7 status bar / AXP2101 (experimental)

Primary left navigation now stays present across **all** main pages including Settings and Radios. The Chat/Map right context action glyphs use the same orange/charcoal palette as left navigation. The upper bar replaces the textual Wi-Fi IP with a real Wi-Fi station status icon and separate MT / MC BLE session status glyphs. **Only one BLE session is implemented** today; do not interpret both icons as simultaneous connections. Battery state comes from a **read-only AXP2101 I2C telemetry worker** (0x03, 0x00, 0x01, 0xA4); it shows charge percentage and lightning during actual charging, USB status otherwise, and `--` if the battery or sensor is unavailable. Never writes to PMU registers or changes charger settings. Testing on the physical Waveshare Rev2.0 is required; AXP2101 percentage is an approximation. See [UI/gesture test plan](../../docs/edge-swipe-sidebars-v077.md).

### v0.7.7 chat readability (experimental)

MeshCore chat cards now display a colored sender name, read-only message text and `HH:MM UTC | N hops` metadata instead of `RX / RECEIVED / Unix-timestamp`. Names are resolved from authenticated contact keys for direct messages; channel `Name: message` labels are explicitly unverified sender claims. `observed_at` reflects when the MeshCrossway BLE client sees the message, not guaranteed original RF receive time. MeshCore `path_len` is decoded using the low 6 bits for hops; received `0xFF` is displayed as `Direct route`, not 255 hops. Original UTF-8, message timestamp and radio packets remain untouched. Hardware UI acceptance and screenshot tests are still required. [Checklist](../../docs/edge-swipe-sidebars-v077.md).

### v0.7.7 emoji sizing and T-Deck response compatibility (experimental)

The 93 image-font emoji are reduced from 20px to **14px** with a 16px line box for more natural text alignment. Unsupported Unicode or literal `[emoji]` placeholders render as `$` on screen, including if the color font is unavailable, while the original bytes remain unchanged. The updated MeshCore Companion decoder accepts documented optional 32-bit OK response values, optional error codes and empty channel datagrams. For genuinely malformed radio frames, it reports only type, length, command and phase to diagnose T-Deck firmware compatibility safely, without printing payloads, keys or PINs. A T-Deck running non-Companion MeshCore or Meshtastic cannot be assumed compatible. The root cause of the reported hardware failure still requires reproducing on the device. See [physical acceptance](../../docs/edge-swipe-sidebars-v077.md).

## v0.7.8 — left drag and status bar polish (experimental)

A touch PRESSING handler on each left nav tab detects a horizontal swipe before LVGL sends CLICKED, suppresses accidental navigation and defers redraw until release. Right Map action scrollbar graphics are hidden so the scroll remains touch-draggable without covering the icons. Header moves just `0.7.8` beside the original logo, fixes MT/MC on a single line, and places battery percentage inside its rightmost battery silhouette. The battery placeholder is `--` for missing/invalid pack. No Wi-Fi/BLE protocol changes. **Real-device touch and layout acceptance remains necessary**, and Web Flasher updates must leave Erase unchecked.


## v0.7.9 — messaging phase 1 (experimental)

MeshCore conversation lists, native room session flow, local search/aliases/favorites, notification and mute controls, editable quick replies, draft persistence and manually confirmed resend now use a bounded transaction journal on microSD. Filesystem work runs outside LVGL. The SD is plaintext by owner choice. Meshtastic remains unavailable pending its adapter. **v0.7.8 is the recommended recovery image**; v0.7.9 still requires physical RX/TX/ACK, SD failure and coexistence acceptance. See [implementation, resource limits and verification](../../docs/messaging-phase-1-v079.md). Earlier release sections describe historical behavior.


## v0.7.11 — Application stability audit (experimental)

LVGL allocation and reallocation explicitly require PSRAM, preserving internal
heap for BLE/stacks/DMA. BLE uses a bounded 128-frame burst mailbox; automatic
list/chat reconstruction is throttled. Enabled Bluetooth intent survives reboot
independently of bonding. Offline/synchronizing composers keep drafts, disable
SEND and explain the actual blocker. Reset breadcrumbs and heap/UI diagnostics
are available remotely. See [audit, sources and tests](../../docs/stability-audit-v0711.md).

Cached incremental saves avoid replaying the entire journal for every update.
Snapshots coalesce, continuous traffic respects retry backoff, and BLE reconnect
preserves a same-radio unsaved RAM snapshot. Read errors cannot trigger archive
truncation. Chat storage has separate error/errno/timing diagnostics in the UI,
remote page and `/health`. Existing journals remain readable. Physical retest is
pending; v0.7.8 remains recovery. See [evidence and retest](../../docs/sd-history-reliability-v0711.md).

## v0.7.10 — Bluetooth startup memory fix (experimental)

v0.7.9 was withdrawn after the owner reproduced NimBLE error 257 (out of memory). Chat preferences and list/sort workspaces now use explicit PSRAM instead of 45,604 bytes of static internal RAM. CI audits the actual ELF to prevent a repeat. Phase-1 history and NVS pairing are preserved; v0.7.8 remains recommended recovery. [Diagnosis and retest](../../docs/ble-memory-fix-v0710.md).

