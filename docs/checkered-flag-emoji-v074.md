# v0.7.4 — MeshCore checkered flag emoji regression

## Reproduction (reported on actual v0.7.3 hardware)

Incoming MeshCore chat showed `[emoji]PU2OOM RenBrant 02: test` when the sender's name contains the checkered flag `🏁` (Unicode `U+1F3C1`). The previous color atlas contained only 92 entries and did not include that code point. The message was not corrupted; the read-only presentation layer substituted a fallback.

## Fix

- Append registry entry `U+1F3C1 → U+E05C`; the 92 existing PUA glyph IDs and original atlas bytes/hash never change.
- Add one 20×20 color Noto Emoji indexed-4 bitmap, verified with a separate SHA-256 and loaded into the existing single PSRAM atlas at offset 92×264. This uses 264 extra PSRAM bytes plus static descriptor and source header. No image downloads.
- Recognize the optional `U+FE0F` presentation selector after an existing known emoji without allocating an additional bitmap.
- Apply the same read-only LVGL transform to MeshCore contact lists, contact-details title, chat header and radio Home name, without changing the underlying packets, contact keys, bytes, routing, stored messages or screen-capture behavior.
- Keep Wi-Fi network profile fallback, persistent BLE bonding and auto-reconnect from v0.7.2-v0.7.3 untouched. No Meshtastic↔MeshCore bridging.
- Verify C++11 and C++20 host parser tests and two independent asset checksums (original atlas and checkered flag).

## Acceptance on Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0

1. Install v0.7.4 **experimental** with Erase device unchecked; verify Wi-Fi autoconnect and MeshCore BLE connection still work.
2. Read a new or existing message including `🏁PU2OOM RenBrant 02: test`. Expect an actual small black-and-white checkered flag graphic **followed by** the unchanged name and message, with no `[emoji]`.
3. Verify the same name in MeshCore Nodes list, contact details, chat heading and Home connected radio label where applicable.
4. Test both `🏁` and `🏁️` (with U+FE0F), other previous emojis, accented UTF-8, and truly unknown emoji fallback.
5. Inspect the remote screenshot: 20×20 glyph, no clipping, corrupt pixels, extra placeholders or broken line wrapping; scroll several messages.
6. Verify normal touchscreen, screenshot endpoint, Wi-Fi STA/AP, SD card and BLE during repeated redraws and power cycles; no regressions, stack overflow or PSRAM leak.
7. If unavailable/invalid image assets, show a readable fallback and maintain the original message bytes. No new PIN prompt unless the peer itself lost its BLE bond.

**Release status:** CI and physical acceptance are independent; completing CI is not proof that the glyph was viewed correctly on real hardware. The v0.6.8 owner-tested rollback remains available.
