# MeshCrossway — official hardware source registry

This page records the manufacturer/upstream sources used to define the Rev A carrier electrical interface. It exists so pin assignments, power decisions and footprints can be traced back to primary documentation rather than seller imagery or earlier project drafts.

## Waveshare host

### ESP32-S3-Touch-LCD-3.5 repository

- Source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/main
- Used for: official hardware repository, V1/V2 schematic location, firmware/examples and board-revision warning.
- Rev A rule: identify the actual Waveshare PCB revision and verify J8 continuity before fabrication.

The manufacturer pin-definition image supplied during the Rev A review is the current J8 numbering source of truth:

- 1 BAT, 2 5V
- 3/4 GND
- 5 GPIO21
- 9 GPIO39
- 11 GPIO40
- 16 GPIO17
- 18 GPIO18
- 25 RXD/GPIO43
- 26 SCL/GPIO7
- 27 TXD/GPIO44
- 28 SDA/GPIO8
- 29/30 GND
- 31/32 3V3

See [pcb-rev-a-pin-allocation.md](pcb-rev-a-pin-allocation.md) for the complete 32-pin table.

## Seeed Wio-SX1262 + XIAO ESP32-S3

### Product page

- Source: https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html
- Used for: selected radio kit identity and product-level configuration.

### Wio-SX1262 official schematic

- Source: https://files.seeedstudio.com/products/SenseCAP/Wio_SX1262/Schematic_Diagram_Wio-SX1262_for_XIAO.pdf
- Used for: internal XIAO↔Wio B2B architecture, SX1262-side signals, power and RF implementation.
- Rev A rule: the carrier does **not** route SX1262 SPI, DIO, BUSY, RESET or RF. Those remain internal to the XIAO/Wio assembly.

## Seeed XIAO ESP32-S3

### Getting started / pin reference

- Source: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
- Used for: XIAO pin names, D6/D7 UART identity and 5 V / 3V3 power guidance.

### Resource index

- Source: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#resources
- Used for: official downloadable schematics, dimensions and related design files.

### XIAO ESP32-S3 Sense V1.51 schematic

- Source: https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/new-res/202003753_XIAO%20ESP32S3%20Sense_V1.51_SCH_260611.pdf
- Used for: supporting review of XIAO ESP32-S3 power/USB/regulator circuitry and pin implementation.
- Scope caution: confirm the exact XIAO PCB revision shipped with the Wio kit before treating any revision-specific circuit detail as final.

### Official Seeed KiCad library

- Source: https://github.com/Seeed-Studio/OPL_Kicad_Library/tree/master/Seeed%20Studio%20XIAO%20Series%20Library
- Used for: carrier-side XIAO symbol/footprint reference, including `XIAO-14P-Add-On`.

## Firmware pin-binding cross-checks

These are not electrical schematics, but they confirm the firmware-side use of the selected external UART pins:

- MeshCore XIAO S3 Wio variant: https://github.com/meshcore-dev/MeshCore/blob/main/variants/xiao_s3_wio/platformio.ini
  - includes `Xiao_S3_WIO_companion_radio_serial`
  - `SERIAL_TX=D6`
  - `SERIAL_RX=D7`
- Meshtastic firmware repository: https://github.com/meshtastic/firmware

Firmware definitions do not replace electrical verification of the actual boards.

## GNSS

The selected module is the Uputronics u-blox MAX-M10S **3 V breakout** with active-antenna support. The carrier uses its physical header as a plug-in module and currently allocates:

- SDA -> Waveshare J8-28 / GPIO8
- SCL -> J8-26 / GPIO7
- TP/PPS -> J8-5 / GPIO21 through 0 ohm removable link
- VCC -> J8 3V3
- GND -> GND
- TXD/RXD -> NC in Rev A

The exact breakout header orientation and active-antenna bias must be verified on the purchased board before PCB fabrication.

## Source hierarchy

When documentation conflicts, use this order:

1. exact physical board revision and continuity measurement;
2. manufacturer schematic / pin-definition drawing for that revision;
3. manufacturer product/resource documentation;
4. upstream firmware board definitions;
5. MeshCrossway documentation and diagrams;
6. reseller imagery or third-party descriptions.

Any change to a physical pin assignment should update [pcb-rev-a-pin-allocation.md](pcb-rev-a-pin-allocation.md), [hardware-wiring.md](hardware-wiring.md), the affected diagrams, and the diagram generator in the same change.
