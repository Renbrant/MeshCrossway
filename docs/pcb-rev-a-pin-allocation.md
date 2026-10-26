# MeshCrossway PCB Rev A — pin allocation and schematic plan

> Status: high-level schematic freeze candidate, 2026-10-26. This document is intended to drive the first KiCad 10 single-sheet A3 schematic. It is not fabrication approval.

## 1. Schematic format

- One schematic sheet only.
- Paper: **A3 landscape**.
- No hierarchical sheets.
- Arrange left-to-right by functional flow: **power -> host/controller -> radios/RF**.
- Put GNSS and service/debug blocks below the host.
- Use short local wires and net labels for long-distance signals.
- Keep all unresolved power paths explicitly marked **TBD / DNP / VALIDATE BEFORE POPULATING**.

## 2. Major assemblies

| Ref | Assembly | Role |
|---|---|---|
| U1 | Waveshare ESP32-S3-Touch-LCD-3.5 | central UI/application host, display/touch, microSD, PMIC |
| U2 | Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | Meshtastic radio |
| U3 | Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | MeshCore Companion radio |
| U4 | Uputronics u-blox MAX-M10S 3 V breakout | GNSS receiver |
| BAT1 | Protected 1S LiPo | portable power source; exact cell still candidate |

The Wio-SX1262 is treated as part of each XIAO/Wio assembly. The carrier does **not** route the SX1262 SPI bus.

## 3. Host-to-radio UART allocation

| Net | Waveshare host | Radio endpoint | Direction | Status |
|---|---|---|---|---|
| MT_UART_TX | GPIO39 | U2 D7 / GPIO44 RX | host -> Meshtastic | candidate; bench validate |
| MT_UART_RX | GPIO40 | U2 D6 / GPIO43 TX | Meshtastic -> host | candidate; bench validate |
| MC_UART_TX | GPIO17 | U3 D7 / GPIO44 RX | host -> MeshCore | candidate; bench validate |
| MC_UART_RX | GPIO18 | U3 D6 / GPIO43 TX | MeshCore -> host | candidate; bench validate |
| GND | GND | U2/U3 GND | reference | required |

Target serial configuration for both links: **115200 8N1, no hardware flow control**, explicitly configured by firmware.

### XIAO/Wio internal radio resources — do not reuse on the carrier

Current upstream Meshtastic and MeshCore XIAO_S3_WIO definitions agree on:

- SX1262 SCK = GPIO7
- SX1262 MISO = GPIO8
- SX1262 MOSI = GPIO9
- SX1262 NSS = GPIO41
- SX1262 RESET = GPIO42
- SX1262 DIO1 = GPIO39
- SX1262 BUSY = GPIO40
- RF RX enable / switch = GPIO38
- TCXO = 1.8 V configuration

Therefore D6/GPIO43 and D7/GPIO44 remain the intended external UART pins.

## 4. GNSS allocation

Use the Waveshare expansion I2C interface exposed at J8 for the MAX-M10S breakout. The Waveshare pin-definition drawing identifies **J8 pin 26 = SCL/GPIO7** and **J8 pin 28 = SDA/GPIO8**. Treat those as the Rev A signal allocation, with continuity verification on the actual board before fabrication.

Required GNSS nets:

- GNSS_SDA
- GNSS_SCL
- GNSS_GND
- GNSS_3V3
- GNSS_PPS — optional/candidate; route to **GPIO21 / J8 pin 5** through a removable link/test point if the chosen breakout exposes PPS
- GNSS_RF stays on the breakout/SMA path; no RF trace on the carrier in Rev A

Add footprints for optional I2C pull-ups but mark DNP until the breakout and host pull-ups are confirmed.

## 5. UART protection / debug provisions

For each UART signal:

- reserve 22–100 ohm series resistor footprint near the transmitting source;
- default value TBD after bench testing;
- add test point for MT_UART_TX, MT_UART_RX, MC_UART_TX, MC_UART_RX;
- keep traces short and away from RF antenna zones;
- do not add level shifting: all endpoints are intended to use 3.3 V logic.

Reserve independent radio reset/recovery control only where a safe accessible XIAO reset/power-control node is confirmed.

## 6. Local decoupling and bulk capacitance

The Rev A carrier shall include local decoupling footprints close to each powered module.

### Radio A and Radio B

For each XIAO + Wio assembly, reserve:

- **100 nF ceramic** directly at the radio supply entry;
- **10 uF ceramic** directly at the radio supply entry;
- **47 uF bulk** footprint, initially DNP;
- **100 uF bulk** footprint, initially DNP;
- test point on the radio-side 3V3 rail and a nearby GND test point.

The 100 nF and 10 uF parts are intended as normal local decoupling. The 47 uF and 100 uF footprints are experimental bulk-capacitance options for Rev A so TX/load-step behavior can be characterized without redesigning the PCB.

Do not populate every bulk option blindly. Validate regulator stability, inrush current, startup behavior and effective capacitance under DC bias before choosing the production configuration.

### GNSS

Reserve near the MAX-M10S breakout supply entry:

- **100 nF ceramic**;
- **10 uF ceramic**;
- optional additional bulk footprint if bench measurements show a need.

### Carrier 3V3 entry

Reserve a larger carrier-entry bulk capacitor footprint in the **220–470 uF** range, initially DNP, to support power-distribution testing. This capacitor is not a substitute for adequate regulator current capability.

### Placement rule

Place decoupling capacitors electrically and physically close to the corresponding module power/GND pins with short, wide connections and a direct low-impedance ground return. Do not place the capacitors far away merely for schematic neatness.

## 6. Power architecture — intentionally not frozen

Current proposal:

Waveshare AXP2101 / system 3V3 -> carrier distribution -> optional radio A branch + optional radio B branch + GNSS branch.

This is **not yet approved electrically**.

Rev A schematic shall therefore include:

- one carrier 3V3 entry net from Waveshare J8;
- bulk capacitor footprint at carrier entry: 220–470 uF, DNP initially;
- each radio branch through a **3-pin 2.54 mm power-source selector jumper**: center pin to RADIO_x_3V3, one side to Waveshare J8 3V3, the other side to the local regulated 3V3 source;
- per-radio 100 nF + 10 uF local decoupling, plus optional 47 uF and 100 uF bulk footprints;
- GNSS 10 uF + 100 nF local decoupling footprints;
- test points on source 3V3 and each branch, with nearby GND points;
- clear warning note: **DO NOT POPULATE SHARED RADIO POWER UNTIL J8 CURRENT BUDGET AND XIAO 3V3 REVERSE-FEED PATH ARE VERIFIED**.

Initial firmware/UART bring-up remains with each XIAO powered from its own USB-C and only logic GND shared.

## 7. RF / antenna handling

- No SX1262 RF traces on the carrier.
- Preserve each Wio-SX1262 U.FL/IPEX connector.
- Keep antenna pigtail keep-out zones around both radios.
- Place the two radio modules on opposite sides of the display/carrier where practical.
- Keep GNSS antenna path physically separated from both LoRa antennas.
- Validate 915 MHz antenna selection for US use and region-appropriate antennas elsewhere.

## 8. Suggested schematic placement on A3

```text
+-----------------------------------------------------------------------------------+
| POWER / BATTERY          HOST / UI                         RADIO A — MESHTASTIC     |
| Waveshare PMIC           Waveshare ESP32-S3                XIAO + Wio-SX1262       |
| shared-rail TBD          Touch / display / SD              UART + power isolation  |
|                                                                                   |
| TEST / POWER             GNSS                               RADIO B — MESHCORE      |
| branch TPs / DNP         MAX-M10S + active antenna         XIAO + Wio-SX1262       |
| options                  I2C / PPS candidate               UART + power isolation  |
+-----------------------------------------------------------------------------------+
```

## 9. Rev A schematic design notes

Place these notes directly on the schematic:

1. **SINGLE-SHEET A3 — MESHCROSSWAY REV A**
2. **POWER DISTRIBUTION TO XIAO MODULES IS A VALIDATION GATE, NOT YET APPROVED**
3. **DO NOT CONNECT WAVESHARE DIRECTLY TO SX1262 SPI; HOST TALKS TO EACH XIAO OVER UART**
4. **D6/GPIO43 = XIAO TX; D7/GPIO44 = XIAO RX**
5. **VERIFY ACTUAL WAVESHARE J8 PINOUT AND PCB REVISION BY CONTINUITY BEFORE FABRICATION**
6. **RADIO ANTENNAS MUST BE CONNECTED BEFORE RF TRANSMIT TESTS**

## 10. Validation gates before layout/fabrication

- actual Waveshare V1/V2 board revision identified;
- J8 signal/power pinout continuity verified;
- Meshtastic PROTO proven on XIAO D6/D7;
- MeshCore `Xiao_S3_WIO_companion_radio_serial` proven on D6/D7;
- simultaneous two-UART operation proven;
- XIAO external power/reverse-current behavior measured;
- Waveshare J8 3V3 load headroom measured under LCD/SD/radio load;
- GNSS breakout supply, active-antenna bias and PPS availability confirmed;
- RF coexistence and antenna placement tested.



## 11. Waveshare J8 reference pinout

Based on the supplied Waveshare pin-definition drawing:

| J8 pin | Signal | J8 pin | Signal |
|---:|---|---:|---|
| 1 | BAT | 2 | 5V |
| 3 | GND | 4 | GND |
| 5 | GPIO21 | 6 | DN / GPIO19 |
| 7 | GPIO38 | 8 | DP / GPIO20 |
| 9 | GPIO39 | 10 | GPIO11 |
| 11 | GPIO40 | 12 | GPIO10 |
| 13 | GPIO41 | 14 | GPIO9 |
| 15 | GPIO42 | 16 | GPIO17 |
| 17 | GPIO45 | 18 | GPIO18 |
| 19 | GPIO46 | 20 | BOOT / GPIO0 |
| 21 | GPIO47 | 22 | RST |
| 23 | GPIO48 | 24 | PWR |
| 25 | RXD / GPIO43 | 26 | SCL / GPIO7 |
| 27 | TXD / GPIO44 | 28 | SDA / GPIO8 |
| 29 | GND | 30 | GND |
| 31 | 3V3 | 32 | 3V3 |

Mechanical reference from the supplied Waveshare drawing: display outline approximately **92.44 mm x 61.00 mm**, PCB mounting-hole span shown as **72.00 mm** horizontally and **48.50 mm** vertically; use the official mechanical drawing/actual board for footprint generation rather than these summary values alone.
