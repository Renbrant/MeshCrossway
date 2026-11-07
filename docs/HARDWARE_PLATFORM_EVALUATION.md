# MeshCrossway — Hardware Platform Evaluation

**Research date:** 2026-11-07  
**Status:** Research / candidate assessment; no migration approved  
**Primary requirement:** Keep Meshtastic and MeshCore connected to **two independent external BLE-capable LoRa devices**, with Wi-Fi remote UI operating concurrently. No protocol bridging.

## Executive decision

Retain the existing Waveshare ESP32-S3 3.5-inch controller as the tested baseline. Evaluate the **Elecrow CrowPanel Advanced ESP32-P4 5-inch** as the leading performance-upgrade prototype, with **Waveshare ESP32-P4 4.3-inch** and **M5Stack Tab5** as alternatives. Consider the **LILYGO T-Deck / T-Deck Plus** as compact ready-made controller variants, **not** performance upgrades. The referenced **Cduino** hardware family requires exact model/part-number identification before a reliable specification-level comparison; do not assume a common specification for all Cduino boards.

**Critical caveat:** ESP32-C6 hardware and ESP-IDF support BLE + Wi-Fi coexistence, but this is **not** evidence that the Elecrow's shipped P4/C6 ESP-Hosted firmware has been demonstrated with two simultaneous BLE central links, Wi-Fi STA and an active SoftAP client. This remains a release-blocking test.

## Functional acceptance requirements

- Two BLE central/client connections at once: one Meshtastic radio and one MeshCore radio, with separate pairing, security/PIN, GATT notifications, reconnect and persistence.
- Wi-Fi STA and SoftAP, ideally simultaneously, including remote web access while both BLE sessions remain active.
- Offline maps with responsive pan/zoom and readable labels; SD storage; GNSS; track recording; LVGL.
- Stable memory headroom and no watchdog resets, with practical battery life.
- Keep user choice of BLE-capable external radio; do not require XIAO, Heltec, or a particular LoRa board.
- Embedded LoRa, where present, is optional hardware and must not silently replace the two independent external radios.

## Candidate comparison

| Platform | Processor / wireless | Display | PSRAM / flash | Strength | Main concern |
|---|---|---|---|---|---|
| Current Waveshare ESP32-S3 3.5-inch | ESP32-S3 integrated Wi-Fi/BLE | 480×320 SPI | 8 / 16 MB | Existing working firmware, GNSS and battery integration | Graphics throughput, RAM pressure |
| Elecrow CrowPanel Advanced P4 5-inch | ESP32-P4 + ESP32-C6 via ESP-Hosted SDIO | 800×480 IPS, **parallel RGB** on this 5-inch variant | 32 / 16 MB | More RAM, larger map, faster graphics pipeline | Hosted BLE + Wi-Fi integration; power and enclosure |
| Waveshare ESP32-P4 WiFi6 Touch LCD 4.3 | P4 + wireless coprocessor (check exact revision) | 800×480-class | Verify per SKU | More compact P4 candidate | Exact board revision, hosted stack, battery/GNSS pins |
| M5Stack Tab5 | ESP32-P4 + ESP32-C6 | 5-inch 1280×720 | 32 / 16 MB | Integrated tablet, removable battery, SD, sensors | High framebuffer cost; porting; BLE/Hosted validation |
| LILYGO T-Deck Plus | ESP32-S3, integrated Wi-Fi/BLE, SX1262 | 2.8-inch 320×240 | 8 / 16 MB | Keyboard, trackball, touch, GNSS, battery, SD; compact | Smaller map and same memory class as current |
| LILYGO T-Deck | ESP32-S3, Wi-Fi/BLE, SX1262 | 2.8-inch 320×240 | Check SKU | Ready-made compact terminal | GNSS not standard; not a CPU/RAM upgrade |
| Guition ESP32-S3 display boards | ESP32-S3 integrated Wi-Fi/BLE | Varies by SKU | Varies | Lower-cost porting experiments | Often no meaningful performance uplift |
| Cduino devices (model TBD) | **Unverified** | **Unverified** | **Unverified** | Potential ready-made alternative | Need exact vendor URL/model, BLE central support, SDK and schematics |

Specifications are SKU/revision-dependent. Verify official schematics and firmware packages before purchase. Do not conflate the 5-inch Elecrow RGB panel with larger P4 boards using MIPI-DSI.

## Detailed P4 + C6 connectivity analysis

### Architectural split

The ESP32-P4 runs the UI, maps, storage, GNSS and application logic; it has **no integrated Wi-Fi/BLE radio**. The ESP32-C6 coprocessor provides 2.4 GHz connectivity through **ESP-Hosted**, commonly over SDIO. The Elecrow reference configuration specifies 4-bit SDIO at 40 MHz, an ESP32-C6 target and a Bluedroid Bluetooth option:

```ini
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_ENABLED=y
CONFIG_ESP_HOSTED_PRIV_SDIO_OPTION=y
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID=y
```

The current MeshCrossway ESP32-S3 uses local NimBLE. Migrating to P4 requires checking whether the **host-side BLE HCI interface, NimBLE host, controller firmware, transport version and security callbacks** all work together. Do not assume the existing NimBLE code can be recompiled unchanged. Bluedroid is a possible fallback, with additional porting and memory implications.

### Two BLE connections

ESP32-C6 BLE controller/stack supports multiple connections in principle. However, controller limits depend on ESP-IDF version, configured maximum connections, available memory and host stack. Published maximum-connection numbers are **not** an Elecrow performance guarantee. Verify two concurrent BLE **central** GATT clients with both target radio firmwares, including pairing and notifications.

### BLE + Wi-Fi coexistence

ESP32-C6 has a **single shared 2.4 GHz RF path**, time-multiplexed between Wi-Fi and BLE; logical concurrency is not simultaneous RF transmission.

Espressif's coexistence matrix marks **Wi-Fi STA connected + BLE connected** as supported/stable (**Y**). SoftAP beacon transmission with BLE is supported, but **SoftAP with a client connected + BLE** is marked **C1**, meaning supported but performance may be unstable. This distinction is decisive: MeshCrossway requires a usable remote browser client, not merely beaconing an AP. The published matrix also does not independently certify **two BLE clients plus AP+STA under real load**.

Mitigations to investigate: modest BLE notification throughput, tuned connection intervals, bounded BLE scanning, avoiding repeated scans during active sessions, Wi-Fi throughput caps, reconnection backoff, and testing STA-only vs AP+STA. None substitutes for real tests.

### Hosted firmware risks

Check exact Elecrow PCB revision, ESP-Hosted host/slave compatibility, C6 firmware version, Bluetooth mode, SDIO wiring and C6 recovery/flashing access. A preflashed C6 may require a separate programming procedure. Confirm whether firmware updates are possible before selecting the board.

### Display and memory

800×480 RGB565 full framebuffer = **768,000 bytes**; two full buffers ≈ **1.54 MB** (decimal). 1280×720 RGB565 full framebuffer = **1,843,200 bytes**; double buffering ≈ **3.69 MB**. These exclude map tiles, decoding, LVGL objects, DMA-capable internal buffers, stacks, Wi-Fi and BLE allocations. PSRAM capacity alone does not guarantee DMA compatibility or sufficient internal RAM.

Evaluate RGB565 preconverted offline tiles, bounded LRU caches, background FreeRTOS tile loading, reduced redraw regions and map label legibility. Vector rendering is a separate research track, not a prerequisite for a hardware upgrade.

### Power and enclosure

Measure active/idle current with Wi-Fi AP+STA, both BLE links, GNSS, SD and varying backlight. Validate charging circuitry and cell compatibility before reusing the existing 3.7 V 10,000 mAh battery. A 5-inch HMI PCB may be substantially larger than its active display area; check mechanical dimensions, antenna placement and heat dissipation.

## Ready-made terminals

### LILYGO T-Deck Plus

Official documentation lists ESP32-S3FN16R8 (240 MHz), 8 MB PSRAM, 16 MB flash, 2.8-inch 320×240 capacitive touch, physical keyboard, trackball, SX1262, microSD, u-blox MIA-M10Q GNSS and 2,000 mAh battery. This is attractive for a pocketable keyboard-centric MeshCrossway edition. It does **not** solve the current 8 MB PSRAM limit and its smaller screen makes map legibility harder. Its onboard LoRa radio cannot implement both independent networks simultaneously by itself; continue using two BLE peripherals unless a separately designed radio architecture is approved.

### Cduino candidates

The term “Cduino” is not sufficiently specific to identify a verified product. Add exact product links, MCU, RAM, display controller/bus, SD, GNSS, battery/charger, firmware support, BLE central capabilities, AP+STA support and schematics before ranking. A complete enclosure/keyboard is useful, but the connectivity requirement remains a hard gate.

## Verification matrix (must pass on real hardware)

| Test | Procedure | Pass criterion |
|---|---|---|
| BLE-A | Connect Heltec V3 Meshtastic, pair/PIN, exchange messages | Stable send/receive |
| BLE-B | Add Heltec V3 MeshCore concurrently | Both GATT sessions remain alive |
| Wi-Fi STA | Join router while both BLE links operate | No sustained disconnects |
| SoftAP | Connect phone/browser to device AP | Remote UI usable |
| AP+STA | Enable both Wi-Fi roles with two BLE links | Usable throughput and bounded latency |
| Reconnect | Power-cycle each radio separately, router drop, AP client reconnect | Automatic recovery |
| Stress | Map pan/zoom, track recording, SD I/O, chat via both networks, remote page | No crash/watchdog |
| Soak | Run at least 24 hours with periodic bidirectional traffic | No persistent failures |
| Memory | Log internal heap, PSRAM, largest free block, stack high-water, DMA allocations | Defined headroom; no leak trend |
| Power | Measure brightness levels and all radios active | Meets target runtime |

**Release gate:** do not label any candidate “compatible” until the two-BLE + Wi-Fi STA + SoftAP-client test passes with the actual Meshtastic/MeshCore firmware and an appropriate long-duration soak test.

## Recommended phased plan

1. **No firmware migration yet:** keep current S3 baseline intact.
2. **Connectivity-only proof of concept on P4/C6:** minimal ESP-IDF/ESP-Hosted app, two BLE central GATT clients, Wi-Fi STA, SoftAP, logs.
3. **UI BSP port:** display, touch, SD, GNSS and power management.
4. **Map benchmark:** frame time, tile latency, memory and text legibility versus existing board.
5. **Full MeshCrossway integration:** independent MT/MC managers, remote UI, track logging, recovery and power tests.
6. **Only then** consider a hardware-specific release and web flasher target.

## Sources and manufacturer references

- Espressif ESP32-C6 RF coexistence matrix: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/coexist.html
- Espressif ESP-Hosted MCU: https://github.com/espressif/esp-hosted-mcu
- Elecrow P4 5-inch ESP-Hosted configuration: https://www.elecrow.com/wiki/CrowPanel-Advanced-5inch-ESP32-P4-HMI-AI-Voice-Chat-Robot.html
- Elecrow 5-inch hardware overview (third-party; cross-check with schematic): https://lcdultrapro.com/boards/elecrow-crowpanel-advanced-p4-5-0
- M5Stack Tab5 manufacturer specifications: https://docs.m5stack.com/en/core/Tab5
- LILYGO T-Deck Plus manufacturer documentation: https://wiki.lilygo.cc/products/t-deck-series/t-deck-plus/
- LILYGO T-Deck manufacturer documentation: https://wiki.lilygo.cc/products/t-deck-series/t-deck/
- Elecrow Amazon candidate: https://www.amazon.com/dp/B0GLGWJP8B

## Research limitations

This is a documentation-based assessment, not a compiled or bench-tested validation. Exact Cduino models, some Waveshare variants, firmware versions, power figures and Elecrow C6 update procedures remain to be verified. Any earlier statement that P4/C6 automatically guarantees the complete MeshCrossway BLE+AP+STA scenario should be read with these limitations.
