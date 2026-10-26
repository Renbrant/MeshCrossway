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

Use the Waveshare expansion I2C interface exposed at J8 for the MAX-M10S breakout. The exact host GPIO mapping must be copied from the **actual shipped Waveshare revision / verified J8 pinout** before the PCB is frozen; do not infer header position from photographs.

Required GNSS nets:

- GNSS_SDA
- GNSS_SCL
- GNSS_GND
- GNSS_3V3
- GNSS_PPS — optional/candidate, routed to an otherwise-free host GPIO/test point if supported by the chosen breakout
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

## 6. Power architecture — intentionally not frozen

Current proposal:

Waveshare AXP2101 / system 3V3 -> carrier distribution -> optional radio A branch + optional radio B branch + GNSS branch.

This is **not yet approved electrically**.

Rev A schematic shall therefore include:

- one carrier 3V3 entry net from Waveshare J8;
- bulk capacitor footprint at carrier entry: 220–470 uF, DNP initially;
- each radio branch through an isolatable link / 0 ohm or load-switch placeholder;
- per-radio optional 47–100 uF bulk + 10 uF + 100 nF footprints;
- GNSS 10 uF + 100 nF local decoupling footprints;
- test points on source 3V3 and each branch;
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

