# MeshCrossway for the LilyGO T-Deck Plus

**Status: v0.1.0, EXPERIMENTAL / NOT HARDWARE TESTED.** Compiles with ESP-IDF 5.3.2. It is published in the [Web Flasher](https://renbrant.github.io/MeshCrossway/flasher/?board=tdeck-plus) only behind the experimental opt-in.

A compact controller for **two independent Bluetooth radios**: one running Meshtastic and one running MeshCore Companion BLE firmware. It uses the same BLE, protocol and microSD-history code as the Waveshare controller (`../display-prototype`, included unchanged). Only the board layer and the UI are new. MeshCrossway does not forward messages between the networks.

## What v0.1.0 does

- Pair, connect, disconnect and auto-resume a Meshtastic radio and a MeshCore radio (PIN and number confirmation).
- Chats and channels from both networks, Meshtastic nodes and MeshCore contacts, direct messages and channel messages.
- microSD history in `/sdcard/mcx-history`. This is the same format as the Waveshare controller, so a card can move between them.
- Keyboard typing; the trackball moves focus and pressing it selects (Enter also works). Touch is enabled when the GT911 responds.
- Battery percentage from the battery ADC.

**Not in v0.1.0:** maps, GNSS, Wi-Fi and remote browser access, settings pages, emoji sprites. The onboard SX1262 LoRa radio stays idle with its chip select held high. Using it for one of the networks is a later phase; see `docs/alternative-central-lilygo-tdeck-plus.md`.

## First hardware test checklist

1. The screen shows the Home page in landscape with readable colors. If the image is mirrored, rotated or color-inverted, adjust `PANEL_*` in `main/tdeck_board.h`.
2. Rolling the trackball moves the orange focus highlight, and pressing it opens the item.
3. Keyboard text appears in a message field, and Enter sends.
4. Pair the Meshtastic radio, then the MeshCore radio. Both stay connected together.
5. Send and receive one message on each network, then reboot and confirm that both radios reconnect and the history is restored from microSD.
6. Touch hits the right place. If it doesn't, adjust `TOUCH_*` in `main/tdeck_board.h`. Touch is optional.
7. Watch the serial console (USB) for `[MCX-TDECK]` and `[MCX-MEM]` lines and any resets.

When a build passes, record it in `TESTED` in `scripts/write_tdeck_firmware.py`. The Web Flasher then preselects it.

## Build

```sh
cd firmware/tdeck-plus
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_SERIAL_PORT flash monitor   # Download mode: hold the trackball while switching on.
```

`scripts/build_tdeck_firmware.sh` builds with the `espressif/idf:v5.3.2` container and packages the three images (bootloader 0x0, partition table 0x8000, application 0x10000) for the Web Flasher. It refuses any layout that would overwrite the NVS partition holding Bluetooth bonds.

Build footprint (v0.1.0): about 0.98 MB application in a 6 MB factory partition. About 119 KB of static internal RAM is used and about 222 KB is left. There is no Wi-Fi stack, unlike the Waveshare build.

## Hardware notes

ESP32-S3FN16R8 (16 MB flash, 8 MB octal PSRAM), ST7789 320×240, GT911 touch, ESP32-C3 keyboard at I2C 0x55. The TFT, microSD and SX1262 **share one SPI bus**, so the microSD card is mounted before the display starts. GPIO10 powers the peripheral rail.
