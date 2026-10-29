# Color emoji in read-only message view - isolated preparation

Status: **DRAFT / NOT ACTIVATED IN FIRMWARE**. See issue #40.

This PR carries only a host-testable UTF-8 presentation transformer and a
versioned registry of 92 candidate color glyph codes. It intentionally does
NOT hook the firmware, change LVGL configuration, add assets, release firmware,
change Meshtastic/MeshCore adapters, or activate emoji display yet. Do not merge
until a subsequent hardware validation and explicit owner approval.

## Current firmware seam

- Main UI helper `txt()` uses LVGL labels in
  `firmware/display-prototype/meshcrossway_ui.h`.
- MeshCore chat in `firmware/display-prototype/meshcrossway_meshcore_ui.h`
  currently renders `m->text` via `txt(card,m->text,...)`, with fixed-height
  cards. Future integration must adjust card height and safe scrolling.
- This preparation does not touch the original message, Bluetooth frames,
  delivery status or the compose keyboard. Meshtastic and MeshCore NEVER bridge.

## Byte-safe display-only sequence

`raw UTF-8 message` -> `mcxemoji::transform(src,len,dst,cap)` -> `temporary display
UTF-8 string with U+E000.. glyphs` -> `LVGL 8.4 imgfont` -> `color assets`.

`emoji_presentation.h` has no side effects or heap allocation; it accepts
explicit byte lengths, validates UTF-8, recognizes composed clusters and uses
whole-glyph truncation. Unknown pictographs produce `[emoji]` in the display
copy, not in the original message. Every supported glyph must acquire a
corresponding color asset before this transform can be wired into firmware.
The registry is append-only to preserve stable codepoint assignments.

## To complete later (explicitly NOT part of this preparation PR)

1. Review image-source licensing, attribution and asset adaptation, including
   `CONTRIBUTING.md` and `THIRD_PARTY_NOTICES.md`; generate matching color
   assets at 20-24px as LVGL 8.4 compatible `lv_img_dsc_t` objects in flash.
   Do not bundle new third-party fonts/assets in this PR.
2. Enable `LV_USE_IMGFONT` on the exact pinned Waveshare/LVGL 8.4 build.
3. Create one shared `lv_font_t` via `lv_imgfont_create(height,callback)`.
   The callback handles U+E000.. glyph IDs with static image descriptors;
   set its fallback to the existing text font; use `lv_imgfont_destroy` only
   after every dependent label has been destroyed.
4. Apply to read-only message labels (and message previews), NOT global font
   or keyboard; generate bounded temporary presentation copies and carefully
   manage their lifetime. If assets/font init fail, show legible placeholders.
5. Recalculate message-card heights/wrap/scroll and handle repeated redraws
   without LVGL object leaks or UI stalls. Keep MT and MC source labels.
6. Compile against the actual LVGL 8.4 build, run device-memory and pixel/
   touchscreen tests with BLE and Wi-Fi concurrently, and obtain owner approval.

## Host-only testing

From repo root (without ESP-IDF or LVGL):

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic -O2 tests/test_emoji_presentation.cpp -o /tmp/mcx_emoji_test
/tmp/mcx_emoji_test
```

Tests cover accented text, common and multi-codepoint emoji, skin tones, ZWJ,
regional flags, keycaps, unsupported sequences, strict malformed UTF-8,
byte-exact message immutability and bounded output/truncation. No device tests
were run; successful host tests do not validate LCD performance or emoji color.

## References

- LVGL 8.4 imgfont: https://lvgl.io/docs/open/8.4/others/imgfont
- Unicode emoji: https://www.unicode.org/reports/tr51/
- MeshCrossway issue: https://github.com/Renbrant/MeshCrossway/issues/40
