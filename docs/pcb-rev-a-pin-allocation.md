# MeshCrossway PCB Rev A — pin allocation and schematic plan

> Status: engineering schematic baseline, 2026-10-26. Pin/net assignments below are the source of truth for the Rev A carrier schematic. This is not yet fabrication approval; the listed validation gates still apply.

## 1. Schematic format

- One schematic sheet only.
- Paper: **A4 landscape**.
- No hierarchical sheets and no separate functional block diagram on the schematic sheet.
- Treat the Waveshare host, each XIAO + Wio radio assembly, and the Uputronics GNSS board as **plug-in modules / black boxes**.
- Show only the real carrier-board interconnects and support circuitry: mating connectors, UART resistors, power-source jumpers, Schottky isolation, current links, decoupling, optional pull-ups, debug header, and test points.
- Use short local wires and net labels for long-distance signals.
- Keep the lower-right KiCad title-block area clear.

## 2. Major assemblies and carrier references

| Ref | Assembly | Role |
|---|---|---|
| J1 | Waveshare ESP32-S3-Touch-LCD-3.5 J8 mating connector | central host interface |
| U1 | Local 5 V source, exact regulator/module TBD | alternate radio power source for Rev A testing |
| U2 | Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | Meshtastic radio |
| U3 | Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit | MeshCore Companion radio |
| U4 | Uputronics u-blox MAX-M10S 3 V breakout | GNSS receiver |
| J2 | 1x4 host debug header | host service UART / 3V3 reference |

The Wio-SX1262 is part of each XIAO/Wio plug-in assembly. The carrier does **not** route SX1262 SPI, control, or RF signals.

## 3. Authoritative Waveshare J8 pinout

Based on the manufacturer pin-definition drawing supplied for the Waveshare board:

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

Carrier footprint candidate for J1: **2x16, 2.54 mm vertical male pin header**, mating with the Waveshare female J8 socket. Final mechanical location/orientation must be checked against the actual board and official mechanical drawing before PCB fabrication.

## 4. Host-to-radio UART allocation

The carrier uses XIAO **D6/GPIO43 as radio TX** and **D7/GPIO44 as radio RX**. The host-side GPIO allocation is independent for the two radios.

| Carrier net | Waveshare J8 / host | XIAO endpoint | Direction |
|---|---|---|---|
| `MT_HOST_TX` -> R1 -> `MT_RADIO_RX` | J8-9 / GPIO39 | U2 pin 8, D7 / GPIO44 / RX | host -> Meshtastic |
| `MT_RADIO_TX` -> R2 -> `MT_HOST_RX` | J8-11 / GPIO40 | U2 pin 7, D6 / GPIO43 / TX | Meshtastic -> host |
| `MC_HOST_TX` -> R3 -> `MC_RADIO_RX` | J8-16 / GPIO17 | U3 pin 8, D7 / GPIO44 / RX | host -> MeshCore |
| `MC_RADIO_TX` -> R4 -> `MC_HOST_RX` | J8-18 / GPIO18 | U3 pin 7, D6 / GPIO43 / TX | MeshCore -> host |

R1-R4 are **47 ohm series damping resistors** in Rev A. They are not substitutes for dedicated ESD protection.

Target serial configuration remains **115200 8N1, no hardware flow control**, subject to bench validation with the selected firmware versions.

### XIAO/Wio internal resources — do not route on the carrier

Current XIAO_S3_WIO firmware definitions use the Wio radio internally. The carrier must leave the Wio SPI/control/RF implementation inside the plug-in module. In particular, the carrier uses only the XIAO edge pins needed for UART/power/test.

MeshCore upstream provides `Xiao_S3_WIO_companion_radio_serial` with:

- `SERIAL_TX=D6`
- `SERIAL_RX=D7`

## 5. XIAO carrier interface

Use the official Seeed carrier-side convention:

- symbol concept: **XIAO-Add-On**;
- carrier footprint: **XIAO-14P-Add-On**;
- physical pin numbering 1-14 exactly as Seeed defines it.

Important carrier pins:

| XIAO pin | Signal | Rev A use |
|---:|---|---|
| 7 | D6 / TX / GPIO43 | radio -> host UART TX |
| 8 | D7 / RX / GPIO44 | host -> radio UART RX |
| 12 | 3V3_OUT | output/test only; **not normal carrier power input** |
| 13 | GND | common ground |
| 14 | VBUS / 5V | normal carrier power input |

Pins 1-6 and 9-11 remain physically present in the footprint and are NC on the Rev A carrier.

## 6. Radio power architecture — Rev A testable topology

The normal carrier injection point is **XIAO pin 14 / VBUS / 5V**, not `3V3_OUT`.

Each radio has an independent 3-pin, 2.54 mm source selector:

```text
J8_5V ----- pin 1
               \
                pin 2 COMMON -> Schottky -> removable current link -> RADIO_x_5V -> XIAO VBUS
               /
LOCAL_5V -- pin 3
```

- JP1 = Radio A source selector.
- JP3 = Radio B source selector.
- D1/D2 = Schottky isolation between selected external source and XIAO VBUS, retained because Seeed specifies diode isolation when feeding XIAO 5V externally.
- JP2/JP4 = removable 1x2 current-measurement links. Remove the shunt and insert an ammeter for characterization.
- U1 = alternate `LOCAL_5V` source placeholder; exact regulator/module is still TBD.

U1 logical pinout:

- pin 1 `VIN_BAT` -> `BAT`
- pin 2 `GND` -> `GND`
- pin 3 `VOUT_5V` -> `LOCAL_5V`

Do not connect XIAO USB and external carrier power simultaneously until backfeed/power-path behavior has been verified on the actual XIAO revision.

## 7. Radio decoupling / bulk footprints

On each `RADIO_x_5V` rail, close to the corresponding XIAO socket, reserve:

- 100 nF ceramic, populated;
- 10 uF ceramic, populated;
- 47 uF bulk footprint, DNP initially;
- 100 uF bulk footprint, DNP initially.

Also expose test points for `RADIO_A_5V`, `RADIO_B_5V`, `A_3V3_OUT`, and `B_3V3_OUT`.

The optional bulk parts are for TX/load-step testing; validate inrush, startup, regulator stability, and effective capacitance under DC bias before selecting a production population.

## 8. GNSS — Uputronics MAX-M10S 3 V breakout

Treat the 3 V Uputronics board as a **7-pin, 2.54 mm plug-in module**. Carrier numbering follows the physical header order used for the Rev A socket:

| U4 pin | Breakout signal | Carrier connection |
|---:|---|---|
| 1 | SDA | `GNSS_SDA` -> J8-28 / GPIO8 |
| 2 | SCL | `GNSS_SCL` -> J8-26 / GPIO7 |
| 3 | TXD | NC in Rev A |
| 4 | RXD | NC in Rev A |
| 5 | TP / PPS | `GNSS_PPS_RAW` -> R5 0R -> `GNSS_PPS` -> J8-5 / GPIO21 |
| 6 | GND | GND |
| 7 | VCC | `J8_3V3` |

The Uputronics 3 V board supply range is 2.7-3.6 V. RF remains on the breakout/SMA path; the carrier does not route GNSS RF.

### I2C pull-ups

Reserve optional hardware pull-ups:

- R6 = 4.7k, DNP, `J8_3V3` -> `GNSS_SDA`
- R7 = 4.7k, DNP, `J8_3V3` -> `GNSS_SCL`

Populate only if measurements show that the existing host/breakout pull-ups are insufficient.

GNSS local decoupling:

- 100 nF populated;
- 10 uF populated.

## 9. Host debug header

J2 is a 1x4, 2.54 mm service header:

| J2 pin | Net |
|---:|---|
| 1 | `J8_3V3` reference |
| 2 | GND |
| 3 | `HOST_RXD` from J8-25 / GPIO43 |
| 4 | `HOST_TXD` from J8-27 / GPIO44 |

The 3V3 pin is a voltage reference/supply pin only; use a 3.3 V logic UART adapter and verify adapter power behavior before connecting.

## 10. Rev A test points

Provide at minimum:

- `J8_5V`
- `LOCAL_5V`
- `RADIO_A_5V`
- `RADIO_B_5V`
- `MT_HOST_TX`
- `MT_HOST_RX`
- `MC_HOST_TX`
- `MC_HOST_RX`
- `A_3V3_OUT`
- `B_3V3_OUT`

## 11. RF / antenna handling

- No SX1262 RF traces on the carrier.
- Preserve each Wio-SX1262 U.FL/IPEX connector and antenna keep-out.
- Put the two radio modules on opposite sides of the display/carrier where practical.
- Keep GNSS antenna/coax path separated from the LoRa antenna regions.
- Antennas must be connected before RF transmit testing.

## 12. A4 schematic placement

```text
+--------------------------------------------------------------------------+
| POWER            HOST / WAVESHARE J8               RADIO A / MESHTASTIC |
| U1 local 5V      J1 2x16 mating header             UART + power select  |
|                                                     XIAO + Wio module     |
|                                                                          |
| GNSS             DEBUG / TEST                      RADIO B / MESHCORE    |
| U4 MAX-M10S      J2 + test points                  UART + power select  |
| I2C + PPS                                           XIAO + Wio module     |
|                                                     decoupling            |
|                                                [title block kept clear]  |
+--------------------------------------------------------------------------+
```

## 13. Validation gates before layout/fabrication

- identify the actual Waveshare V1/V2 PCB revision;
- continuity-check J8 signal and power pins on the physical board;
- prove Meshtastic serial operation on XIAO D6/D7;
- prove MeshCore `Xiao_S3_WIO_companion_radio_serial` on D6/D7;
- prove simultaneous two-UART operation;
- measure XIAO external-5V / USB backfeed behavior;
- measure J8 5V source capability and voltage droop under the intended host load;
- select/validate the local 5V regulator architecture;
- verify the exact Uputronics 3V header orientation before PCB placement;
- verify GNSS I2C pull-up requirement;
- validate RF coexistence and antenna placement;
- run native KiCad ERC and PCB DRC before fabrication.

## 14. Schematic audit rule

For generated/edited carrier schematics, verify connectivity by **physical pin number -> expected net**, not by the apparent vertical location of a pin in the drawing. This rule was added after a coordinate-orientation error was found in an early Rev A draft.
