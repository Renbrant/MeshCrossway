# Waveshare firmware status — October 28, 2026

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
