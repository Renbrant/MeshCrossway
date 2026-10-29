# MeshCrossway display-only firmware prototype

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
