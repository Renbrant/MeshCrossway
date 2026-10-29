# v0.7.7 — Swiping the sidebars (480×320 Waveshare / LVGL 8.4)

**Status:** experimental, pending ESP-IDF CI and real touchscreen testing.

## Gesture contract

| UI | Open | Close | No arrows |
|---|---|---|---|
| Left sidebar (Home, Nodes, Groups, Chats, Map) | Put a finger within ~22px of the **left edge**, then drag right at least 44px | From the inner edge of the **expanded** left rail, drag left at least 44px | Left navigation remains visible as 40px icon strip when collapsed |
| Right sidebar (Chats, Map only) | Put a finger within ~23px of the **right edge**, then drag left at least 44px | From the inner edge of the **expanded** right rail, drag right at least 44px | Right actions remain visible as 40px clickable icons when collapsed |
| Home, Nodes, Groups | — | — | Right rail remains absent, no redundant filters |
| Settings and Radios | — | — | Both rails remain absent, all Settings workspace usable |

Swipe interpretation is activated **only** if the finger originates on the relevant edge or the expanded rail's inner 38px. It requires the horizontal travel to be at least twice the vertical travel, lasts at most 1.8 seconds, and is evaluated **once on release**, so the LVGL press handler is never interrupted by deleting/rebuilding widgets. This is a swipe-to-snap interface, **not** a continuously-following drawer.

The gestures are disabled during Wi-Fi editor, BLE PIN keyboard, Quick Settings shade, full-screen BLE scanner and information dialogs, and are disabled on the header. Vertical scrolling of Messages, Nodes, Map panels, and right Map actions remains unchanged. An ordinary tap must keep working as before.

The existing arrow buttons and their 39px reserved row are removed completely. Left six navigation icons are spaced vertically inside the full 286px rail. The right action list begins at the top, retaining scrolling for all Map actions.

No BLE/Wi-Fi/GNSS/LoRa/network-side effect is permitted. Remote web interface remains read-only screenshots and never controls gestures. The official v0.7.6 SVG-derived brand stays in the top header.

## Implementation

- `meshcrossway_edge_swipe.h`: pure state machine, C++11 compatible and unit-testable without a display, with edge zones, horizontal/vertical thresholds, modal suppression, page change cancellation and one-shot trigger.
- `meshcrossway_ui.h`: an LVGL 8.4 **20ms timer** polls the existing pointer only within the LVGL task. The pinned v8.4 exposes `indev->proc.state` via `lv_hal_indev.h`; do not accidentally migrate to the v9-only `lv_indev_get_state`. No new touch driver or hardware gesture sensor is used. Draw only after release, not while touched.
- `apply_to_waveshare.py`: copies the gesture header into the vendor example along with the UI headers.
- `scripts/test_edge_swipe.py` and `tests/test_edge_swipe.cpp`: all four gestures, short taps, long gestures, vertical swipes, modal, header, disabled rail, wrong-page and source-zone tests under C++11 and C++20.
- `scripts/preflight_display.py`: rejects arrow control regressions and ensures release-based edge gestures.
- Web Flasher: v0.7.7 remains **experimental**. Keep **Erase device unchecked** for normal updates (NVS Wi-Fi/Bluetooth bonds remain intact). v0.6.8 is immutable validated rollback.

## Real-device acceptance matrix

1. Boot v0.7.7 with **Erase unchecked**; inspect screen capture. No left or right arrow icons; six left icons visible in Home compact rail; header brand, IP and version visible.
2. At left edge, swipe **right**: expand 79px labeled navigation. Swipe **left starting near the inner edge of expanded rail**: collapse back to 40px. Confirm tabs still select the correct pages with ordinary taps.
3. On Nodes/Groups, verify absence of redundant right rails and vertical scrolling does not open any sidebar.
4. On Chat and Map, swipe **left from rightmost 23px**: expand contextual actions to 125px. Swipe **right from expanded bar's inner edge**: collapse. Every action in both states must work; scroll Map actions to the bottom.
5. On Home, Nodes, Groups and Settings, a right-edge swipe does **nothing**; Settings keeps the full-width screen with HOME/RADIOS back.
6. Enter Wi-Fi editor and BLE PIN keyboard; drag from either edge. No menu should open or dialog disappear. Test Quick Settings and info dialogs similarly.
7. Scroll chats vertically, drag map content, tap nav and action buttons: no unintended menu toggles. Verify taps near screen edge don't trigger a swipe.
8. Stress at least 30 alternations per side while MeshCore BLE session, Wi-Fi STA/AP, SD and screenshot server run. Verify no LVGL invalid-obj assert, freeze, watchdog, SPI LCD queue exhaustion or heap regression.
9. The firmware's paired radio and Wi-Fi reconnect after power cycle without manual PIN or credentials when bonds are valid. There is no Meshtastic↔MeshCore message bridge.

CI success is necessary but **not** evidence of physical gesture behavior on the board.
