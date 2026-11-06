# v0.9.29 emoji test candidate — issue #48

Status: experimental; physical acceptance pending. The byte-identical v0.9.28 image remains recommended stable. Current recoveries are v0.9.25 and v0.9.16; older recovery references in issue #48 are historical.

## Change and limits

The candidate covers 750 explicitly cataloged Unicode sequences, including all 272 visible reference mappings. It preserves all 93 original PUA assignments and exact 14px legacy pixels. See the [generated coverage audit](emoji-coverage-v0929.md), `scripts/emoji/coverage.json` and append-only registry for precise coverage. This is not full Unicode emoji support and does not introduce an emoji keyboard.

Names in node/channel/conversation lists use the same display-only renderer as message bodies, sender names and headings. Fixed row geometry, manual refresh, node retention, map behavior, BLE workers and native protocols are preserved. Temporary PUA markers are never stored or transmitted. Unknown clusters and text-presentation variants use the readable `[emoji]` fallback; malformed UTF-8 is replaced safely in presentation only. If a fixed scratch buffer cannot hold the expanded display text, the complete original text is retained with the default font instead of discarding the message.

## Memory strategy

- All 750 14×14, 16-color sprites and lookup tables stay in read-only flash. Palette pointers have four-byte alignment.
- Pixel/palette bytes: 121,500; aligned sprite storage: 123,000 plus 12 string terminators. These counts are not total firmware growth.
- Remove the former 24,552-byte PSRAM atlas and 93-entry mutable image-descriptor table. Image descriptors are constructed individually on the stack by the LVGL callback.
- Use one shared 1,200-byte PSRAM text scratch buffer owned exclusively by LVGL; font and scratch initialization are attempted once. Allocation failure falls back without retry storms. The fallback-only stack buffer is 512 bytes rather than the old 1,200-byte buffer on every label call.
- LVGL still owns label text and its normal small image decoding allocations. Total runtime RAM depends on visible labels and message lengths; hardware measurements remain required.

## Automated procedure

1. `python3 scripts/test_emoji.py`: offline hashes, full catalog consistency, all 272 visible mappings, exact first-93 pixels/PUA, all 750 sequence transformations, UTF-8 bounds, VS16/VS15, long ZWJ chains, regional/tag flags, modifiers, keycaps and readable unknown fallback. Compile on C++11 and C++20.
2. `python3 scripts/test_ble_pairing_ui.py --lvgl /path/to/lvgl`: real LVGL 8.4 node/channel/chat integration with emoji enabled; render all 750 image descriptors; repeated label changes; actual allocation failure for shared scratch and image font; long-message preservation. Reuse the production PSRAM-only allocator and verify bounded live bytes. Retain existing touch, node-list/history, map and Home regression checks.
3. Run preflight, independent Meshtastic/MeshCore protocol tests, conversation/history tests and Web Flasher preservation checks. Inspect the ESP-IDF 5.3.2 CI build and memory/stack reports against stable v0.9.28. The installer must continue to default to stable and require experimental opt-in, with `Erase device` unchecked for a normal update.
4. Regeneration is build-time only: `python3 scripts/build_emoji_catalog.py --cache /path/to/pinned-inputs`. Noto/Unicode input hashes and rasterizer versions are recorded in `scripts/emoji/coverage.json`; CI verifies committed assets offline. No asset downloads occur on the device.

## Measured build comparison

ESP-IDF 5.3.2 preflight [run 42250260865](https://github.com/Renbrant/MeshCrossway/actions/runs/42250260865) passed both jobs, including real LVGL and independent radio/history regression tests with normal CI sanitizers. Comparison uses its debug artifact `13519754274` and stable v0.9.28 debug artifact `13471837771` (run `42150205062`). The recorded merged build source is `33e4245024aeb1e4e2d6853c81055e15d82aeb72`; later documentation/notice packaging does not change the firmware inputs.

| Measurement | Stable v0.9.28 | Candidate v0.9.29 | Change |
|---|---:|---:|---:|
| Merged image bytes | 2,219,232 | 2,345,376 | +126,144 |
| Flash code bytes | 1,420,482 | 1,420,330 | -152 |
| Flash data bytes | 589,652 | 715,940 | +126,288 |
| Static DIRAM bytes | 162,204 | 161,092 | -1,112 |
| Static BSS bytes | 35,008 | 33,896 | -1,112 |
| `set_message_text` static compiler stack frame | 2,480 | 592 | -1,888 |
| Explicit renderer backing buffer in PSRAM | 24,552 | 1,200 | -23,352 |

The last row compares explicit atlas/scratch allocation requests after initialization, not whole-device heap measurements. Static compiler frames are individual functions, not complete call-chain stack usage. Runtime free heap, largest block, UI high-water mark and behavior under simultaneous maps/BLE require the physical checklist below. The effective LVGL image cache remains disabled (`CONFIG_LV_IMG_CACHE_DEF_SIZE=0`), matching the stable build and avoiding mutable imagefont descriptor cache reuse.

Candidate merged image SHA-256: `fff99d7c1e5d7bc373f4d72b1511d06cb92ef8f89b5d01f9bf3de940466261c3`.
Stable v0.9.28 remains byte-identical: `a36970339c00cc327515eac2648c5421e1b9aab460484919a6ed8f57cc040c61`.

## Physical acceptance checklist

- [ ] Install v0.9.29 using Web Flasher experimental opt-in without erase. Confirm Wi-Fi networks and both independent BLE bonds remain saved.
- [ ] On each network separately, inspect node names, channel/group names, conversation headers, senders and received message bodies containing `🏁`, `❤️`, `🇧🇷`, `🇺🇸`, `👍🏿`, `👩‍💻`, `🏳️‍🌈` and a family sequence. Compare with the native radio app; verify alignment, colors, clipping and fixed row spacing.
- [ ] Show an unsupported ZWJ sequence (for example woman + ZWJ + grinning face), unknown flag pair and long message with a final `END`. Confirm a readable fallback and preserved suffix. Verify original source names/message bytes remain unchanged on the radio and after SD history reload/reboot.
- [ ] Repeatedly open/scroll/close node lists, channels and at least 100 messages. Confirm scroll position and no overlapping rows, watchdog, reset, or growing memory trend.
- [ ] Collect serial `[MCX-MEMLOG]`, `[MCX-ALLOC]` and stack diagnostics before opening chats, after 10 minutes of switching pages, after map dragging and after reconnection of each radio. Compare internal free/largest block, PSRAM free and minimum UI stack headroom with v0.9.28 under the same workload; inspect allocation failures individually.
- [ ] Test map dragging/tile loading, touch response, Wi-Fi hotspot switching, independent BLE reconnects and orientation lock. Emoji coverage alone does not establish whole-device stability.
- [ ] Confirm exact v0.9.28 rollback remains installable. Attach screenshots and logs to issue #48 before considering promotion or closure.

## Attribution

New artwork is rasterized from official Noto Color Emoji v2.047 (OFL-1.1). The original license and copyright remain in `LICENSES/NotoEmoji-OFL-1.1.txt` and `THIRD_PARTY_NOTICES.md`, copied into the published firmware notice bundle. Only mapping facts were extracted from the comparison repository; no third-party application implementation or bitmap bytes are reused.
