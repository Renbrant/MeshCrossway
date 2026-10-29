# Read-only color emoji rendering — MeshCrossway v0.7.3 candidate

**Implemented in branch `feature/color-emoji-rendering`, PR #41.** This is a **candidate**, not yet hardware-tested, merged or published. It adds 92 colored emoji images to the conversation view; it does **not** add an emoji picker or alter the keyboard, message payloads, BLE security or native radio protocols.

## Rendering architecture

1. `emoji_presentation.h` validates bounded UTF-8 and tokenizes supported emoji sequences, including basic smileys, flags, ZWJ, variants, skin tones and keycaps. The input bytes remain unmodified.
2. The 92-entry `emoji_registry.inc` maps complete emoji sequences to internal private-use Unicode markers U+E000..U+E05B. Unsupported emoji sequences become `[emoji]` in the **temporary presentation string only**.
3. `emoji_assets_data.h` contains 92 color Noto Color Emoji glyphs (20×20, 16-color palettes), pre-rasterized and bundled as an offline-only Base64 blob. There is **no network, font download or runtime PNG decoding**. Unpacked image data is 24,288 bytes in PSRAM; string source occupies approximately 32 KB in firmware flash.
4. `emoji_display.h` decodes sprites into a shared PSRAM block once, configures each `lv_img_dsc_t` as `LV_IMG_CF_INDEXED_4BIT`, then creates one `lv_imgfont_create(20, image_cb)` font using LVGL 8.4 (not LVGL 9). Standard text uses the default font fallback. If initialization fails, individual emoji clusters fall back to `[emoji]` without hiding surrounding text.
5. `meshcrossway_meshcore_ui.h` calls `set_message_text` only on the visible chat message body and uses each label’s calculated height for card sizing. The `m->text` field, identities, timestamps, queueing, delivery status and composer remain untouched. The future Meshtastic chat view can use the exact same display-only function with no inter-network traffic.

## Source and licensing

Color artwork is derived from **Noto Color Emoji**, copyright Google Inc. and contributors, from the SIL Open Font License 1.1 font. The source font itself is **not included** in MeshCrossway: only pre-rasterized 20×20 bitmaps with 16-color palette reduction are bundled. See `THIRD_PARTY_NOTICES.md` and `LICENSES/NotoEmoji-OFL-1.1.txt`. The implementation is original MeshCrossway display logic and does not copy third-party messaging-app source code.

## Validation and deployment gates

- Run `python3 scripts/test_emoji.py`: verifies all 92 image palettes, decoded byte count, pinned SHA-256 and the independent C++11 Unicode parser tests.
- `apply_to_waveshare.py` copies the four emoji files into the pinned vendor example. ESP-IDF `sdkconfig.defaults` requests `CONFIG_LV_USE_IMGFONT=y`; `scripts/check_ble_sdkconfig.py` requires this option in the **generated**, not merely requested, configuration.
- GitHub Actions `Firmware preflight (no website deployment)` performs host validation and ESP32-S3 compilation. A successful CI compile is **not** physical validation.
- On hardware, install only after CI succeeds, with **Erase unchecked** to preserve NVS Wi-Fi and Bluetooth bonds. Capture screenshots of mixed emoji text, multi-line and long chat; verify colored images, layout/scroll, BLE, Wi-Fi, touch and memory stability.
- Do not merge or publish this PR until the user has inspected and approved the behavior on hardware.

**Scope excludes** emoji keyboard or picker, relaying between Meshtastic and MeshCore, automatic radio message conversion, global fonts and external downloads. The v0.7.2 automatic reconnect firmware remains on `main` until approval.

## References

- LVGL 8.4 [imgfont API](https://lvgl.io/docs/open/8.4/others/imgfont)
- [Unicode Emoji Technical Standard](https://www.unicode.org/reports/tr51/)
- Noto Color Emoji [source](https://github.com/googlefonts/noto-emoji)
- Tracking [Issue #40](https://github.com/Renbrant/MeshCrossway/issues/40), [PR #41](https://github.com/Renbrant/MeshCrossway/pull/41)
