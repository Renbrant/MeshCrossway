> **Version-scoped UI contract for merged experimental v0.7.6.** This document is accurate for **v0.7.6 only**: Settings has no main left sidebar and the top bar can show IP. **Open v0.7.7 PR #54** proposes the opposite: global left navigation **always** present (including Settings/Radios), no IP in the top bar, swipe-expanded rails and matched icons, along with battery/MT/MC indicators and refined chat formatting. Do not silently apply PR #54's contract to a v0.7.6 device. [Current status](current-status.md).

# MeshCrossway v0.7.6 — contextual navigation and official branding

**Status:** experimental; compile, visual and Waveshare touch tests are required before promotion.

## UI rules (480×320, LVGL 8.4)

| Screen | Left navigation | Right contextual actions | Main area |
| --- | --- | --- | --- |
| Home | Visible, icons when compact | **Absent** (navigation would duplicate left) | Dashboard cards, maximum usable width |
| Nodes | Visible, icons when compact | **Absent** (All/MT/MC is already in content) | Discovery/filters and contact list |
| Groups | Visible, icons when compact | **Absent** (All/MT/MC is already in content) | Filters, channel list and tools |
| Chats | Visible, icons when compact | **Present**: Compose, Chat tools, Chat info | Actual conversation and composer |
| Map | Visible, icons when compact | **Present**: Map, POIs, Tracks, Record, Share, GNSS, Team | Navigation workspace (demo still labeled) |
| Settings | **Absent** | **Absent** | Full-width two-column device, Wi-Fi/SD status and local actions |
| Settings / Radios | **Absent** | **Absent** | Full-width BLE controls; SETTINGS button returns to settings |

### Interaction requirements

- Left rail toggles 40px icon-only ↔ 79px labeled mode. Home no longer hides icons and the compact setting is consistent when changing pages.
- Right rail toggles 40px icon-only ↔ 125px labeled mode; action buttons **remain visible and clickable** when minimized. All seven Map actions can be reached with vertical scrolling without obstructing the display. No empty right handle/strip on pages without contextual actions.
- Chat Compose and Tools are **only** on the right rail, not duplicated above each conversation. The message list begins higher up. Chat info opens actual contact details (or safe channel metadata for group chats), not another copy of Tools.
- Settings has a clearly visible **HOME** exit button; its Radios detail has a **SETTINGS** return. Main actions: Wi-Fi Networks, Bluetooth / Radios, Remote View (read-only), Device Diagnostics (including read-only eFuse audit), Privacy, About.
- The upper application bar uses the **actual official mountain logo and traced wordmark**, rasterized from `docs/site/assets/meshcrossway-logo.svg` to a 180×24 indexed-4 LVGL image. The letterforms are not approximated by a generic font. The existing Wi-Fi status and version remain visible and the bar still opens Quick Settings.
- Rendering-only/layout change: does not change mesh messages, MeshCore pairing, Meshtastic independence, Wi-Fi profiles, flash partitions, eFuses, SD data or Web Flasher erase handling.

## Hardware acceptance test

1. Boot v0.7.6 without checking Erase; verify both the mountain symbol and custom MeshCrossway wordmark at the top, with Wi-Fi IP and DEV 0.7.6 visible and no overlap.
2. On Home confirm the compact **left rail displays all six icons**. Open and collapse it; all icons remain visible and clickable, including More.
3. On Nodes and Groups check All / MT / MC filters work and **there is no duplicate right sidebar**. The main list grows to fill the space.
4. On Chat: right bar starts compact with **Compose, Chat tools and Info icons** visible. The center no longer repeats Compose/Tools. Toggle expanded/collapsed, open contact details and a channel's info; verify actions match the selected conversation and messages are not hidden, sent, converted or replayed unexpectedly.
5. On Map: verify all 7 actions show accessible icons when compact (scroll if needed); expanded labels match actions; content changes modes while GNSS / map remain clearly demo-only.
6. On Settings: no left or right rails. Verify two balanced status cards and six touch actions, HOME exit works, Bluetooth / Radios opens full-width with SETTINGS return and BLE scanning/pairing remains functional.
7. Open and close Quick Settings from the header, Wi-Fi password editor, BLE PIN keyboard, diagnostics modal and read-only remote screenshot. Verify no clipping, scroll corruption, missing touch targets or watchdog stalls.
8. Reboot with previously bonded MeshCore radio and known Wi-Fi: both recover normally; check there was **no eFuse write**, no SD/NVS erase and no cross-protocol routing.

## Build/asset checks

- `meshcrossway_brand.h` holds an independently rasterized (from the repository's official SVG) 165×22 BGRA-palette LVGL image, 2,224 bytes. No image download, SVG parser, or large runtime decoder is needed on the Core.
- `apply_to_waveshare.py` copies this header with the existing UI headers; preflight asserts inclusion, compact rails and no duplicated Nodes/Groups context arrays.
- The firmware remains experimental until physical touchscreen screenshots validate precise branding, contrast and sizing. **v0.6.8** remains the verified owner rollback.
