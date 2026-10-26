# MeshCrossway V1 — Seeed XIAO ESP32-S3 + Wio-SX1262 hardware and wiring

[README](../README.md) · [Power design](power-architecture.md) · [Serial APIs](radio-communication-architecture.md) · [Official hardware sources](hardware-sources.md)

> **Current V1 hardware selection (2026-10-26):** two **Seeed Studio XIAO ESP32-S3 & Wio-SX1262 kits**, one running Meshtastic and one MeshCore Companion. No soldered prototype or PCB has been electrically validated.

## Physical assembly concept — one carrier PCB, rear battery

![Hardware and almost-invisible transparent enclosure layers](images/hardware-overview.svg)

![Approved temporary exploded enclosure render with protected 1S LiPo battery mounted behind the carrier PCB](images/architecture/meshcrossway-exploded.webp)

This assembly concept places the two Seeed XIAO/Wio kits and MAX-M10S breakout on **one custom carrier PCB** around the Waveshare display. The protected 1S LiPo pack is **behind the PCB**, not mounted next to the screen. These AI-assisted renders are approximate and **not exact supplier photos or mechanical CAD**. Verify real footprints, B2B stack height, charging safety and antenna clearances before layout approval.

![Revised external UART versus internal B2B wiring](images/architecture/xiao-wiring.svg)

![Rev A controller-radio-GNSS wiring](images/architecture/wiring-diagram.svg)

## 1. Hardware inventory and buying links

| ID | Part | Quantity | Purpose |
|---|---|---:|---|
| U1 | [Waveshare ESP32-S3-Touch-LCD-3.5](https://www.amazon.com/dp/B0F3WS2W5R) | 1 | 480 × 320 display, touchscreen, ESP32-S3 host, microSD and PMIC |
| A | [Seeed XIAO ESP32-S3 & Wio-SX1262 kit](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html) | 1 | dedicated Meshtastic radio |
| B | [Seeed XIAO ESP32-S3 & Wio-SX1262 kit](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html) | 1 | dedicated MeshCore Companion radio |
| G1 | [Uputronics MAX-M10S breakout, 3 V variant](https://shop.wegmatt.com/products/uputronics-ublox-max-breakout-for-active-antennas?variant=44060220489751) | 1 | host-side u-blox GNSS over I²C |
| ANT-G | [Bingfu active GNSS antenna](https://www.amazon.com/dp/B0FL7PVF16) | 1 | candidate active GNSS antenna, bias/connector validation required |
| S1 | microSD for Waveshare onboard slot | 1 | storage, logs and future map assets |
| BAT | protected 1S LiPo compatible with Waveshare requirements | 1 | proposed portable power; verify protection, connector, current, enclosure |

The XIAO ESP32-S3 kit contains a 2.4 GHz Wi-Fi/BLE MCU with **8 MB flash and 8 MB PSRAM** plus a SX1262 operating across supported sub-GHz bands. It uses a **board-to-board (B2B)** connection between the XIAO and Wio daughterboard. A Wio module supplied for a **XIAO nRF52840** or an alternative **header-board variant** may use *different* wiring; do not substitute it based on its name. The SX1262 antenna uses a U.FL/IPEX connector; confirm antenna/pigtail suitability for the selected regional frequency and final enclosure.

**Cost planning:** Calculate the complete BOM using two Seeed kits, applicable taxes/shipping, RF pigtails and antennas, carrier PCB fabrication, battery and enclosure. Confirm purchase prices before publishing a total.

Manufacturer references used for Rev A: [Seeed Wio-SX1262 + XIAO product](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html) · [official Wio-SX1262 schematic](https://files.seeedstudio.com/products/SenseCAP/Wio_SX1262/Schematic_Diagram_Wio-SX1262_for_XIAO.pdf) · [XIAO ESP32-S3 getting started](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) · [XIAO resources](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#resources). See the [official hardware source registry](hardware-sources.md) for the full list.

## 2. Interfaces: external UART versus internal SX1262 B2B bus

Each kit contains two internally connected devices: a **XIAO ESP32-S3 MCU** and a **Wio SX1262 radio module**. The MCU controls the SX1262 via **internal SPI and GPIOs on the B2B connector**. The Waveshare communicates only with the **XIAO MCU** via a *separate* external UART API. Do not wire Waveshare GPIOs to the SX1262 SPI signals or DIO1/BUSY lines.

### Rev A host-to-XIAO serial connectivity

![UART crossing on two identical XIAO pin pairs](images/radio-wiring.svg)

![Host GPIO identities and XIAO connector candidates](images/connector-reference.svg)

| Function | Waveshare U1 | XIAO A (Meshtastic) | XIAO B (MeshCore) | Status |
|---|---|---|---|---|
| Host UART1 TX → radio A RX | GPIO39 | **D7 / GPIO44 (RX)** | — | Rev A selected; firmware/bench validation required |
| Radio A TX → host UART1 RX | GPIO40 | **D6 / GPIO43 (TX)** | — | Rev A selected; firmware/bench validation required |
| Host UART2 TX → radio B RX | GPIO17 | — | **D7 / GPIO44 (RX)** | Rev A selected; firmware/bench validation required |
| Radio B TX → host UART2 RX | GPIO18 | — | **D6 / GPIO43 (TX)** | Rev A selected; firmware/bench validation required |
| Common logic reference | GND | GND | GND | mandatory |
| Central GNSS SDA / SCL | **SDA GPIO8 / SCL GPIO7** | — | — | J8 pin 28 SDA / pin 26 SCL; verify actual board continuity before fabrication |
| Central GNSS PPS | **GPIO21** | — | — | J8 pin 5 <- MAX-M10S TP/PPS through R5 0R |
| Radio power | **J8 pin 2 / 5V** or alternate `LOCAL_5V` | **pin 14 VBUS/5V** | **pin 14 VBUS/5V** | independent 1x3 selector -> Schottky -> current link; bench validate |
| GNSS power | **J8 pins 31/32 / 3V3** | — | — | MAX-M10S 3 V breakout VCC; 100 nF + 10 uF local decoupling |

**GPIO43/44** are the Rev A XIAO external UART TX/RX signals. They may also be used for serial boot diagnostics and USB/debug workflows; verify that the exact Meshtastic serial module and MeshCore Companion build can bind the API to these pins without startup logging corrupting the binary stream. Wire **TX ↔ RX**, never TX ↔ TX. Baseline link target: **115200 baud, 8N1**, explicitly configured (the Meshtastic serial default is not necessarily 115200).

XIAO pins labeled **D6/D7** are board-pin names; the numbers **GPIO43/44** are ESP32-S3 native GPIO numbers. Do not confuse either with pins on the Wio SX1262 radio module.

### SX1262 internal B2B hardware (reference, NOT external host wiring)

For the current **XIAO ESP32-S3 + Wio-SX1262 B2B kit**, the Seeed schematic and current upstream Meshtastic/MeshCore board definitions agree on the radio wiring: **SCK=GPIO7, MISO=GPIO8, MOSI=GPIO9, NSS=GPIO41, RESET=GPIO42, DIO1=GPIO39, BUSY=GPIO40 and RX-enable/RF switch=GPIO38**; TCXO is configured at 1.8 V. These are internal radio resources and must not be reused by the carrier. **D6/GPIO43 and D7/GPIO44 remain available for the external host UART.** Older pin lists using GPIO35/36/37 or GPIO1/2/4/5 do not describe the current selected B2B kit and must not be used for this design.

Sources: [Seeed kit schematic/resources](https://wiki.seeedstudio.com/xiao_esp32s3_%26_wio_SX1262_kit_for_meshtastic/), [Meshtastic board definition](https://github.com/meshtastic/firmware/blob/master/variants/esp32s3/seeed_xiao_s3/variant.h), and [MeshCore Xiao_S3_WIO variant](https://github.com/meshcore-dev/MeshCore/blob/main/variants/xiao_s3_wio/platformio.ini).

## 3. Bench wiring

```text
                  Waveshare ESP32-S3-Touch-LCD-3.5
                    UART1 TX GPIO39  ----->  A XIAO D7 / GPIO44 RX
                    UART1 RX GPIO40  <-----  A XIAO D6 / GPIO43 TX
                    UART2 TX GPIO17  ----->  B XIAO D7 / GPIO44 RX
                    UART2 RX GPIO18  <-----  B XIAO D6 / GPIO43 TX
                    GND              ------- A GND / B GND / GNSS GND

                    I2C SDA GPIO8 / J8-28 <-----> MAX-M10S SDA
                    I2C SCL GPIO7 / J8-26 <-----> MAX-M10S SCL
                    GNSS PPS GPIO21 / J8-5 <----- MAX-M10S TP/PPS (candidate)

        XIAO A --(internal B2B SPI)--> Wio SX1262 A -- antenna
        XIAO B --(internal B2B SPI)--> Wio SX1262 B -- antenna
```

The Waveshare GPIO39/40 and GPIO17/18 assignments assume the **camera is absent and disabled**. The supplied Waveshare pin-definition drawing identifies J8 pin 9=GPIO39, 11=GPIO40, 16=GPIO17, 18=GPIO18, 26=SCL(GPIO7), 28=SDA(GPIO8), 25=RXD(GPIO43), 27=TXD(GPIO44), 31/32=3V3, 1=BAT, 2=5V and multiple GND pins. Use GPIO7/8 for MAX-M10S I2C and reserve GPIO43/44 for service/debug where practical. Verify continuity on the actual board before fabrication.

## 4. Firmware bring-up

1. Identify the exact Wio B2B kit and XIAO PCB revision. Test each separately via its own USB-C interface and an antenna connected.
2. Flash a **Seeed XIAO ESP32-S3 + Wio SX1262** Meshtastic image for A; enable the Serial module in `PROTO` mode using XIAO RX=44, TX=43 if that release supports that mapping. Validate actual framing, reboot and incoming messages; use the firmware target specifically built for the Seeed XIAO/Wio kit.
3. Flash/build **`Xiao_S3_WIO_companion_radio_serial`** for B, not repeater, USB, BLE or Wi-Fi Companion firmware. Current upstream source already sets `SERIAL_TX=D6` and `SERIAL_RX=D7`, corresponding to GPIO43/GPIO44 on XIAO ESP32-S3. The source path is now confirmed; bench operation, startup behavior and persistence still require validation.
4. MeshCore's release/partition layout may require a **complete merged firmware image** on first flash to retain configuration. Check release instructions and verify region/name/settings survive a cold power cycle; do not blindly flash an app-only image to a blank target.
5. Capture UART framing with each kit alone; then connect both to Waveshare and test concurrency, reconnect and isolation. Preserve both separate USB ports for maintenance.

Upstream MeshCore includes a dedicated **Xiao_S3_WIO serial Companion** environment; [Seeed's MeshCore guide](https://wiki.seeedstudio.com/pt-br/get_started_with_esp32s3_meshcore/) also discusses merged images and persistent settings. The UART source configuration is confirmed, while the physical link and cold-boot behavior remain bench-validation gates.

## 5. Power / RF / mechanical gates

![GNSS and battery at the rear of the carrier](images/gnss-and-power.svg)

![Conditional AXP2101 and J8 power distribution](images/architecture/power-architecture.svg)

Rev A no longer proposes feeding the radios through XIAO `3V3`. Each radio is fed at **pin 14 / VBUS / 5V** from an independent source selector choosing `J8_5V` or the still-TBD `LOCAL_5V`, followed by Schottky isolation and a removable current-measurement link. This follows Seeed's external-5V guidance and keeps `3V3_OUT` as an output/test node. The exact J8 5 V current headroom, Schottky drop, local-regulator choice, USB/VBUS backfeed behavior and simultaneous TX transients still require bench measurement. The GNSS remains on J8 3V3. Never tie chargers or battery pads together. See [power study](power-architecture.md).

The XIAO's smaller footprint does not reduce the electrical separation needed by two simultaneous nearby RF transmitters. Mount the antennas apart, verify suitable 915 MHz antennas for US deployment, and characterize desensitization during simultaneous TX/RX.

For carrier PCB: use footprints matching the **specific XIAO + Wio B2B kit stacking geometry**; leave USB-C accessible for firmware service, reserve UART test pads, and consider controlled reset and switched power **only after verifying available pins and safe shutdown behavior**. No final mechanical clearance, stack height, antenna pigtail path or footprint is approved yet.

## 6. Required validations

- Board ID and pinout with actual hardware (all external UART and internal SX1262 pin reservations)
- Meshtastic PROTO on external XIAO UART at configured baud, without debug contamination
- MeshCore Companion on external XIAO UART with persisted settings
- Two simultaneous independent UARTs, power-cycle and queue recovery
- J8 5 V source capability, `LOCAL_5V` regulator choice, Schottky drop, USB/VBUS backfeed, brownout and RF TX-current measurement
- RF coexistence and antenna placement
- GNSS antenna bias/current and I²C compatibility
- Prove route behavior separately for both firmware releases; zero-hop remains unverified

**Design decision:** two XIAO ESP32-S3 + Wio SX1262 kits are selected for V1 documentation and bench evaluation; PCB release awaits the gates above.


## 7. Official hardware sources used for Rev A

The complete source registry is maintained in [hardware-sources.md](hardware-sources.md). The primary manufacturer references supplied for this design review are:

- [Waveshare ESP32-S3-Touch-LCD-3.5 GitHub repository](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/main)
- [Seeed Wio-SX1262 with XIAO ESP32-S3 product page](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html)
- [Seeed Wio-SX1262 official schematic PDF](https://files.seeedstudio.com/products/SenseCAP/Wio_SX1262/Schematic_Diagram_Wio-SX1262_for_XIAO.pdf)
- [Seeed XIAO ESP32-S3 getting started](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Seeed XIAO ESP32-S3 resources](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#resources)
- [Seeed XIAO ESP32-S3 Sense V1.51 schematic PDF](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/new-res/202003753_XIAO%20ESP32S3%20Sense_V1.51_SCH_260611.pdf)

The Sense schematic is a supporting ESP32-S3/XIAO power and pin-reference document; the exact XIAO PCB revision shipped with the Wio kit must still be confirmed before fabrication.


## MAX-M10S breakout-specific electrical reference

**Manufacturer breakout datasheet:** [Uputronics uBLOX MAX Breakout for Active Antennas, Rev 2.0 (25 April 2025)](https://cdn.shopify.com/s/files/1/0835/7707/8094/files/uBLOX_MAX_Breakout_for_Active_Antennas_Datasheet.pdf?v=1745569009). The 3 V board is selected; never apply 5 V to its VCC (**2.7–3.6 V operating, 3.6 V absolute maximum**). The separately sold 5 V version has different supply and level-converter circuitry.

| Breakout signal | Carrier Rev A connection | Explanation |
|---|---|---|
| VCC | Waveshare J8-31/32, 3V3 | Verify loaded rail ≤3.6 V and no backfeed |
| GND | Common GND | Required return |
| SDA | J8-28 / GPIO8 | I2C data; verify pull-ups |
| SCL | J8-26 / GPIO7 | I2C clock; verify pull-ups |
| TP | J8-5 / GPIO21 via R5 0 Ω | Optional 1 Hz time pulse after fix |
| TX, RX | Not connected | Breakout UART is an alternative interface; default 9600 baud |
| EN | Not present for selected 3 V variant | Only the 5 V breakout uses EN for its level converter |

**Measured values are still required:** breakout datasheet lists **25 mA acquisition, 21 mA tracking and 6 mA cyclic power-save**; do not use the chip-only low-power figure as the complete breakout budget. Add active-antenna current separately; nominal antenna bias is **2.7 V** through the breakout's **SMA female** port, so verify antenna compatibility. Board footprint is approximately **22 × 22 mm**, 2.54 mm header pitch, 1.6 mm PCB; check orientation, height and clearance against the received part. A **0.2 F supercapacitor** preserves backup state and an LED indicates PPS. These features do not eliminate the need for continuous regulated supply and local decoupling.

