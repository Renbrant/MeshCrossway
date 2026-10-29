# Waveshare firmware status — October 28, 2026

## Current candidate — v0.7.13 (October 29, 2026)
v0.7.13 integrates PR #63 on top of v0.7.12: a dedicated touch sampling task with bounded queue, PSRAM chat-card reuse, preserved reading position on radio updates, and offline history restore without waiting for the delayed BLE auto-resume. Preflight and full ESP-IDF build must pass before Pages publication. **Experimental, not yet hardware tested:** tap and rail scrolling during BLE traffic, absent/slow SD, internal memory and repeated cold starts must be checked. Retains one active Bluetooth radio at a time, independent MeshCore and Meshtastic network stores, and the unchanged tested v0.7.8 recovery. Install without Erase.

## Previous candidate — October 29, 2026

v0.7.12 implements the independent Meshtastic PhoneAPI client using an external T-Deck / T-Deck Plus BLE radio. See [setup, compatibility gates, tests and hardware acceptance](meshtastic-v0712.md). Only one BLE radio session is active at a time. Physical T-Deck qualification is pending; v0.7.8 remains the immutable owner-tested recovery. Earlier sections below are historical release records.


Target: Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0, non-B, 480 × 320. ESP-IDF 5.3.2; LVGL 8.4.0; Waveshare BSP `d9ff7837148b47b33edadc4b87775c341cf35bbe`.

| Firmware | Physical evidence | Use |
|---|---|---|
| v0.4.0 | Screen/touch repeatedly tested | Recommended historical recovery |
| v0.3.0 | Screen/touch baseline tested | Older recovery |
| v0.5.0 / v0.5.1 | Display blackouts/flickering reported | Do not reinstall for this test |
| v0.5.2 | Owner reported stable screen and accessible menus on October 28 | Display-tested rollback; Wi-Fi provisioning unverified |
| v0.5.3 | Hardware test pending | Experimental touchscreen Wi-Fi provisioning |

## v0.5.3 behavior

Settings → CONNECT / CHANGE HOME WI-FI opens the touchscreen provisioning dialog. It starts an asynchronous 2.4 GHz scan; SCAN repeats it. Select a discovered SSID or tap the SSID field to enter a hidden network manually. Tap the password field to direct the on-screen keyboard there. Password entry is masked. WPA personal passwords must contain 8–63 bytes; open and enterprise networks are unsupported in this build.

Wait for the scan to finish, then press CONNECT. The dialog closes and Settings displays connection progress, failure or the assigned local IP. After a failure, reopen the dialog and retry with the password. CANCEL closes the dialog without saving the entered credentials. A scan already running finishes in the background. ESP-IDF stores submitted station credentials in NVS and attempts reconnection on reboot; passwords are not logged. Reboot persistence and the complete touch workflow still need hardware confirmation.

HTTP screenshots, the browser remote-control server and the secondary LVGL pointer remain disabled. A local IP does **not** imply browser control is available. The display regression’s precise root cause remains unknown. Radio, BLE, GNSS and map panels still contain simulated data.

## Release checks

PR #14 must pass offline checks and the full ESP-IDF archive preflight before merge. Both preflight and Pages publication check the version label embedded in the compiled v0.5.3 image, in addition to manifests and SHA-256 checksums. These gates incorporate the approach proposed in draft PR #9. Successful compilation does not establish hardware stability. Wait for successful Pages deployment before installing.

The archive retains the exact physically tested v0.5.2 image from commit `ce221fcf3d0ba1524ab2cea7e6296b584a97ff71`, SHA-256 `293c3a363b4aa686ce7e31d1fcb41576f2cd412501fade09c25b4b6287619bbf`. Publication fails if this recovery image cannot be restored and verified. v0.4.0 remains the recommended default independently of the latest experimental version.

## Focused hardware test

1. In the [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/), select **v0.5.3** explicitly and verify **DEV 0.5.3** on the screen after boot.
2. Navigate through all menus and open Settings → Wi-Fi setup.
3. Confirm the scan lists nearby networks and SSID selection works.
4. Confirm the virtual keyboard edits both SSID and masked password fields; try CANCEL and reopening.
5. Select your 2.4 GHz network, enter its password, wait for scan completion and press CONNECT.
6. Confirm Settings shows a local IP, then navigate for several minutes and watch for blackouts/freezes.
7. Restart once to check credential persistence and reconnection. If connection fails, retry through the dialog.

Report the visible firmware version, scan/keyboard result, connection/IP result and whether blackouts occurred. Roll back directly to v0.5.2 or v0.4.0 if necessary. Do not mark v0.5.3 hardware-tested until the owner confirms the results.

## v0.5.4 keyboard correction

The owner confirmed v0.5.3 installation, network scan and SSID selection. Password input was blocked because the keyboard was not visible. LVGL 8.4 creates keyboards with bottom-center alignment; applying the dialog coordinates without resetting that anchor places the keyboard below the screen. v0.5.4 sets top-left alignment before positioning it at (4, 185), size 472 × 131, inside the 480 × 320 dialog. The Wi-Fi logic and disabled remote subsystem remain unchanged. Keyboard visibility/typing, connection and display stability require a new hardware test.

## v0.5.4 physical result and next layout

The owner confirmed successful Wi-Fi connection and normal operation on v0.5.4. Preserve it as the landscape reference while developing portrait separately. Portrait (320 × 480) is the requested direction, but simply compressing the current ten-column keyboard would reduce key width from roughly 47 to 31 pixels. Use a dedicated text-entry view; consider a reduced-column keyboard or optional landscape entry rather than shrinking existing keys. Portrait firmware is not yet implemented or tested.

## Build once, publish verified bytes

Pages first looks for an artifact from successful same-repository PR preflight. It checks the GitHub artifact digest and compares all firmware files, build script, metadata writer and preflight workflow against the compiled commit. Only identical inputs may reuse that binary; current manifest/checksum/embedded-version checks run again. Missing, expired or mismatched artifacts fall back to compilation. Compiled commit provenance is retained. Superseded preflight runs are cancelled automatically; active Pages deployments are not cancelled.

## v0.5.5 full-screen landscape QWERTY

The agreed orientation remains landscape for every screen; portrait and ABC keyboard proposals are superseded. Tap the SSID/password field to open an editor with text at the top and the native QWERTY keyboard below. Keyboard area grows from 472 × 131 to 472 × 218 pixels. Native uppercase/numeric/symbol modes remain available. DONE (or keyboard checkmark) applies the edit; CANCEL (or keyboard close key) discards it. Password is masked on opening, with a SHOW/HIDE control. Connection still requires CONNECT on the provisioning screen. v0.5.5 requires hardware testing; exact owner-tested v0.5.4 bytes remain available as rollback.

## v0.5.5 typing freeze / v0.5.6 controlled input change

The owner reported one freeze **during typing** on v0.5.5, requiring restart; typing worked after reboot. Ergonomics improved but letter keys remained too narrow. Root cause is unconfirmed; do not mark v0.5.5 stable.

v0.5.6 keeps the same editor, Wi-Fi driver, landscape orientation and disabled remote features. QWERTY letter rows give their width to letters; mode/backspace commands move to other rows. Lowercase, uppercase and all printable ASCII symbols remain available. Every key uses CLICK_TRIG and NO_REPEAT: insert on completed release, never repeat while held. This reduces repeated textarea updates but is **not a proven freeze fix**. Existing periodic diagnostics add key event count and last/maximum processing duration, with no key labels or input contents logged and no serial printing in the key callback. Test touch-down/hold/release, mode switching, long credentials, backspace, confirm/cancel and stability on hardware. Exact tested v0.5.4 remains rollback.

## v0.5.6 confirmed baseline / v0.5.7 remote experiment B

The owner confirmed on October 28 that v0.5.6 keyboard sizing is usable and the version functions normally. This confirms the tested keyboard/Wi-Fi scope, not BLE, radios, GNSS or mapping. Preserve the exact v0.5.6 binary SHA-256 `a52490616a960709b3f476fe1fae7b2d98e68a0d447c7980f10efe60d098967d`, compiled source `0d1193d427926529615e1bd24f60d5d34d267fa4`. The earlier typing-freeze root cause is still unproven.

v0.5.7 introduces only a bounded HTTP server: GET `/` shows status; GET `/health` returns version, uptime, free heap, IP and screen/touch capability flags. No LVGL calls, screenshots, framebuffer allocations, pointer devices or credential-changing endpoints are present in this service. Status is readable by clients on the local network without a PIN; it exposes no SSID/password. Two client sockets, two routes and short socket timeouts bound resource use. Server startup/route failure is logged and the server stops on partial registration failure rather than restarting the UI.

After successful deployment, connect to home Wi-Fi as usual, note Settings' IP, then open `http://DEVICE_IP/` from the same LAN. Verify the v0.5.7 status page and advancing uptime while navigating and typing on the physical device for several minutes. This is **not yet screen viewing or remote touch**. Validate HTTP-only stability on hardware before adding screenshot capture (experiment C), then remote touch separately (experiment D). Roll back to exact tested v0.5.6 if needed.

## v0.5.8 Wi-Fi persistence and optional plaintext SD backup

The owner explicitly authorized unencrypted SD storage. Insert a FAT-compatible SD card **before boot**. No formatting is performed; no card, mount failure or invalid backup must prevent internal Wi-Fi provisioning. After successful DHCP, a lower-priority worker saves `/sdcard/meshcrossway-wifi.json` with format=1, ssid and password in plaintext. It never logs their contents. Unchanged credentials skip rewriting. A temporary file is flushed/synced before replacement; the previous valid file is retained as `.bak`. This reduces interruption risk but is not a guarantee against FAT/card damage or removal during writing.

At startup, internal NVS takes precedence. Only when no SSID exists internally does the firmware validate and restore the SD JSON (or its `.bak` fallback) into NVS. Invalid/oversized/nested data is rejected. There is no hot-insert support in this version; restart with the card inserted. The Wi-Fi setup dialog displays SD backup status. To remove a saved SD configuration, delete both JSON and BAK files; erasing internal flash alone deliberately allows SD restore. Nothing is encrypted.

From v0.5.8, ESP Web Tools installs separate bootloader (0), partition table (0x8000) and application (0x10000) files, leaving NVS 0x9000–0xefff and PHY data untouched when Erase is unchecked. The merged image remains an archive/checksum artifact, not the installer input. Components have individual SHA-256 values and must equal their corresponding merged-image slices. Historical version manifests/images stay unchanged; installing older merged images can still overwrite NVS. A full Erase still clears internal settings; SD restore is available only if a valid backup already exists.

Physical tests pending: first boot/card mount and backup creation, reboot without Erase, reinstall v0.5.8 without Erase to verify NVS preservation, then (only after saved backup is confirmed) full Erase and SD recovery. Keep keyboard/Wi-Fi/status HTTP testing; capture and remote touch remain disabled. Do not mark SD/preservation tested solely from CI.


## v0.5.8 SD write failure on physical FAT32 / v0.5.9 fix candidate

On October 28 the owner first attempted a 64 GB exFAT card (not mountable by the current FatFs configuration). Switching to 32 GB FAT32 changed the device's SD backup status to **ready**, confirming mount success. After entering home Wi-Fi credentials, Settings reported **write failed**. The expected root-level file `meshcrossway-wifi.json` was absent, and owner previously saw only unrelated files on the card.

**Root-cause candidate strongly supported by source:** The pinned ESP-IDF v5.3.2 FatFs Kconfig defaults to `CONFIG_FATFS_LFN_NONE=y`; the original Waveshare `sdkconfig.defaults` does not override it. Yet the backup opens `/sdcard/meshcrossway-wifi.tmp`, renames to `meshcrossway-wifi.json`, and may create `meshcrossway-wifi.bak` — all filenames longer than FAT 8.3. The card can mount successfully while these operations fail (likely an invalid filename error). Actual failed syscall and errno in the owner's v0.5.8 binary are not available because that build logs only a generic status.

v0.5.9 (not physically tested) explicitly enables `CONFIG_FATFS_LFN_HEAP=y` in the patched ESP-IDF build. It also reports the exact failing stage/errno (open, write, flush, sync, close, rotate, rename, etc.) in serial diagnostics, on the main Settings overview, in the Wi-Fi setup dialog, and via `/health`; never logs SSID/password. HTTP remains **status only**, with no LVGL screenshot or remote input.

Physical test: insert FAT32 card before boot; install **without Erase**; verify Settings' SD state; reconnect Wi-Fi (or await DHCP); confirm state **saved** and a valid `meshcrossway-wifi.json` at SD root. Remove/read the card only while device is powered down, and treat the plaintext password as sensitive. Only after verifying the backup should full Erase/recovery be tested. NVS-preservation on firmware updates is an independent issue; continue investigating if credentials still disappear despite installing without Erase. Preserve v0.5.7 and v0.5.8 binary rollbacks.

**Release numbering:** v0.5.9 is reserved for the SD bug fix. The separate experimental read-only screen viewer PR #21 must be renumbered/rebased before future merge; do not publish two different firmwares with the same version.

## v0.5.9 physical verification — FAT32 SD write and no-Erase Wi-Fi update

On October 28, the owner reported **SD backup: saved** on v0.5.9 with a **32 GB FAT32 microSD**. The same device was then reinstalled with v0.5.9 and **automatically reconnected to the home Wi-Fi** after restarting. This physically validates successful backup-write reporting and Wi-Fi credential persistence across that reinstall. The v0.5.8 FAT32 write failure is no longer present in this test. Source-level cause was likely ESP-IDF FatFs long-filename support being disabled; a hardware errno trace from the failing v0.5.8 image was not captured, so avoid claiming conclusive syscall-level proof.

**Confirmed additionally on October 28:** the owner powered off the unit, removed the 32 GB FAT32 card and verified on Windows that root-level `meshcrossway-wifi.json` exists and contains a syntactically valid JSON object with keys `format` (value `1`), `ssid`, and `password`; the password was hidden in the shared evidence. **Still not validated:** a complete **Erase → SD restore with NVS empty** was not performed. Reconnection after normal reflashing can come from preserved internal NVS, not necessarily SD restoration. Do not mark full SD recovery as tested or suggest Erase until a valid external backup has been confirmed. The plaintext JSON holds the Wi-Fi password; do not request screenshots or contents.

The v0.5.9 firmware remains HTTP status-only. The draft screen-viewer experiment PR #21 must be rebased and renumbered to **v0.5.10 or later** before merging to avoid version collisions.


## v0.5.10 experiment C — manual read-only remote screen (hardware test pending)

The v0.5.9 binary was physically validated with a 32 GB FAT32 microSD: the owner saw `SD backup: saved`, inspected the real `meshcrossway-wifi.json` (credentials hidden in shared evidence), then reinstalled without Erase and saw automatic home-Wi-Fi reconnection. This is **not** proof of full Erase → SD recovery; leave that as a separate test. The exact v0.5.9 split installer and merged firmware are archived for recovery.

v0.5.10 introduces a deliberately narrow version of remote display viewing:

- The original local `http://DEVICE_IP/` status page now has an optional **Capture screen** action. A manual click requests one 480 × 320 RGB565 frame; it is rendered in a browser canvas. There is no continuous screenshot polling or video stream.
- The user must type the **8-digit View PIN** shown in **Settings** on the physical Waveshare. The PIN is regenerated at boot, passed only in a request header, and never sent via `/health`.
- An HTTP request queues a one-shot capture. An LVGL timer—not the HTTP task—renders `lv_scr_act()` into a single allocated 307,200-byte PSRAM buffer using `lv_snapshot_take_to_buf`. No additional touch-input device or touch endpoint is registered.
- Captures are serialized, limited to one every five seconds, and the HTTP request times out after four seconds. If a draw takes more than two seconds, capture is disabled for the remainder of that boot. Memory stats and capture timings remain available in `/health`.
- The existing SD-backup reporting, FAT long-file-name configuration, current Wi-Fi NVS data, and 0.5.9 split manifest behavior remain intact.
- **Security:** HTTP traffic is not encrypted. The screenshot can expose private information displayed physically. Use only on a trusted private LAN; never expose device port 80 to the Internet. The PIN is only a limited development access control.

**Focused hardware test, only after automated CI + GitHub Pages deployment:** install v0.5.10 **without Erase** and keep microSD FAT32 inserted. Confirm `DEV 0.5.10` and Wi-Fi auto-connection. Check `SD backup: saved` and screen/touch stability before any capture. Open the local browser page via the device's assigned IP. Enter the PIN from the device's Settings and press **Capture screen** once, then repeat after five seconds while changing pages on the physical Waveshare. Confirm color/orientation, screenshot updating, responsive touch and uptime/heap. Test a wrong PIN (403) without changing the device. If there is any flicker, black screen, lockup or failed capture, **do not advance to remote touch**; roll back to the exact tested v0.5.9. A successful build is not physical proof of stability.

The future step (experiment D) may consider bidirectional touch only after experiment C passes the physical test.


### October 28, 2026 — v0.5.10 manual screenshot physically verified (first capture)

The owner installed v0.5.10 and provided a browser screenshot showing **Device online / manual view available** plus **Read-only snapshot received** and a correctly rendered **480 × 320** Settings screenshot of the Waveshare UI. The browser status reported one completed manual capture, **58 ms** last-capture latency, 42 seconds uptime, **7,869 KB free heap**, and **SD backup: saved**. The local HTTP status, Wi-Fi connection, correct firmware label `DEV 0.5.10`, physical View PIN display, browser PIN entry and one screenshot transfer are therefore hardware-observed as working. The View PIN was visible in the shared screenshot and should be rotated by rebooting the device; never copy actual PINs, passwords or SSIDs into public logs.

**Not yet verified:** repeated captures over extended uptime, physical touch responsiveness while capturing, stability during menu changes, zero blackouts under stress, PIN failure behavior, or any remote touch/input capability. Therefore label this as **first read-only capture confirmed**, not yet long-term hardware-stable. Keep v0.5.9 available as known-good SD backup/Wi-Fi rollback and do not enable remote touch until a separate stability test passes.


### October 28 — repeated read-only capture and default Web Flasher selection

Following the first successful v0.5.10 remote screenshot, the owner physically tested **multiple captures of different interface pages** and reported all captures working normally with no black-screen or touch-interaction regressions observed during this test. The v0.5.10 manual **read-only viewer** is therefore hardware-tested for multiple captures. This is not a multi-hour stability/soak test; **remote touch remains disabled** and requires its own isolated experiment.

The Web Flasher now **preselects `catalog.latest`** on load (v0.5.10 at this change), while the existing drop-down remains able to select any archived firmware. The catalog's `recommended` entry remains `v0.4.0` for historical recovery; do not remove or replace it. Subsequent releases will open on their newest published version automatically, without hardcoding the version number.


## v0.6.0 — native Waveshare LVGL Field Console (physical test pending)

This is a **major UI navigation milestone** based on the approved browser UI v5 in `docs/ui-prototype/` (480 × 320 landscape). It **does not** add GNSS hardware, map tiles, Meshtastic/MeshCore BLE, radio packet transmission, or remote touch.

- **Left global sidebar** with Home, Chats, Map, Radios and More, each with icon and label. It can collapse to an icon-only rail.
- **Right context sidebar** with tools based on the active screen. It independently collapses for maximum workspace, and the list scrolls for long Map action lists.
- **Top quick-settings shade** opens from the status bar. Displays preview options for GPS, BLE, sound and brightness, clearly labeled as non-operational; the Wi-Fi action opens the **actual working** local touchscreen provisioning workflow.
- **Home:** separate MX-M/Meshtastic and MX-C/MeshCore status cards, GNSS and cross-network indicators, all explicitly **offline/demo**, not invented live counts.
- **Chats:** demo inbox with All / Meshtastic / MeshCore filters and contextual actions. Compose remains demo-only and does not send messages.
- **Navigation:** an illustrative topographic-style map/grid (not a basemap) plus separate POI, Tracks, GNSS, Team and Live Share panels. Track recording and live position sharing are **OFF**, with clear physical requirements. No file or radio transmissions occur.
- **Radios:** independent BLE status cards explicitly marked not connected; tools remain demonstrations until the radio pods are integrated.
- **More:** real ESP32 uptime, free heap, current Wi-Fi connection, SD backup status and physical View PIN; the same tested full-screen QWERTY Wi-Fi keyboard, NVS saving, and FAT32 backup are retained.
- **Remote viewing:** exactly the existing v0.5.10 PIN-protected, manual, read-only RGB565 screenshot. No HTTP remote click/touch endpoint and no additional LVGL pointer.
- **Rendering safety:** live Settings and top Wi-Fi status labels are updated **in place** every 3 seconds; switching pages or changing sidebar size can rebuild that page only on a user interaction. A potentially destabilizing wholesale timer-driven UI recreation is intentionally avoided.

**Hardware test plan:** Select v0.6.0 in the Web Flasher (without Erase), boot with the tested FAT32 SD, verify Wi-Fi auto-reconnect and `SD backup: saved`, navigate each page, collapse/expand both rails, scroll the Map context tools, open POI/Tracks/GNSS/Share subviews and top shade, use the original Wi-Fi keyboard, and capture multiple pages through the unchanged read-only HTTP interface. Observe the display for blackouts/flicker and memory stability. If anything regresses, recover using the **unchanged v0.5.10 archived image**. Automated build success does not establish hardware stability.

The approved visual reference has dark graphite and orange/amber as its primary palette; color-coded functional accents may indicate map/track/share/radio tools without implying actual live data.


## v0.6.1 — adaptive Field Console layout (physical validation pending)

- Compact left navigation centers each glyph inside its 34px button and hides captions without losing the icon.
- Body scrolls vertically for overflowing Chats and Radio status; headings/cards use single-line ellipsis.
- Right contextual tools use short one-line headers, a fixed 32px header gap, and active-filter highlighting.
- Home status cards use shorter text on narrow widths; map sample contours scale with available canvas width.
- Quick Settings is a bounded modal with a dim backdrop and explicit demo-only indicators.
- Wi-Fi touchscreen provisioning, SD plaintext backup, manual PIN-protected view-only capture and disabled remote touch are unchanged.
- Check all four independent rail combinations, 5 pages, map subviews and modal behavior on physical 480x320 LCD. Do not mark hardware-tested before owner confirmation.

## v0.6.2 — six-tab field workflow (hardware test pending)

- Navigation: Home, Nodes, Groups, Chats, Map, More (Radios has moved into More > Radios).
- On Home, the left rail is hidden except for a 29px handle that reopens the six navigation buttons.
- Nodes has independent All/MT/MC filters, with sample contacts and repeaters labeled **DEMO**, not live.
- Groups has All/MT/MC filters, sample channels and a MeshCore room server. Unread counts are **DEMO**.
- Selecting a sample contact/group opens Chats; the chosen conversation stays selected when navigating elsewhere.
- Custom vector radio, two-person and conversation icons are rendered with LVGL shapes, not font emojis.
- Wi-Fi settings, SD backup and PIN-protected read-only capture are unchanged. No BLE transmission or GNSS is implemented.

**Hardware acceptance:** flash without Erase, inspect all six nav entries and Home's reopen handle, collapse/expand the rail, filter Nodes/Groups independently, open each sample thread then return to Chats, check More > Radios and More > Wi-Fi, and capture screenshots with the existing remote viewer. Verify no lockups after repeated navigation. Build success is not proof of hardware stability.

See [workflow research](field-console-workflow.md).

