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
