# MeshCrossway display-only firmware prototype

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
