# MeshCrossway Rev A - PCB layout baseline

> **ARCHIVED INTEGRATED PCB VARIANT — DO NOT MODIFY KICAD:** This engineering reference describes the older integrated Rev A board. The new project architecture is three detachable modules with BLE links; PCB work is deferred while software is developed on separate boards. [Current architecture](modular-architecture.md).

> Status: engineering floorplan/routing baseline. This is suitable for review and bench-layout iteration, **not yet for fabrication**.

## Board and stackup

- Engineering carrier outline: **160 x 94 mm**.
- 4 copper layers: **F.Cu / In1.Cu / In2.Cu / B.Cu**.
- `In1.Cu` is a continuous GND reference plane under the host, both XIAO/Wio radios and the GNSS module.
- `B.Cu` also carries a broad GND pour.
- `In2.Cu` carries the main battery/5 V/3V3 power trunks.
- Nominal board thickness in the KiCad file is 1.6 mm; final dielectric thicknesses must follow the selected PCB fab stackup.

## Floorplan

- Waveshare host mechanical envelope is centered in the carrier as a `Dwgs.User` reference.
- Radio A / Meshtastic is on the **left wing**, rotated so its LoRa U.FL/pigtail exits toward the left board edge.
- Radio B / MeshCore is on the **right wing**, rotated so its LoRa U.FL/pigtail exits toward the right board edge.
- MAX-M10S GNSS is at the **top center**, opposite the LoRa antenna exits and far from the local 5 V regulator placeholder.
- `U1` local 5 V source is at the lower-left wing.
- `JP1/JP2/D1` are adjacent to Radio A VBUS; `JP3/JP4/D2` are adjacent to Radio B VBUS.
- `TP1` through `TP10` are grouped along the lower edge for bench access.

## Power delivery

- Local `RADIO_A_5V` and `RADIO_B_5V` paths: **1.0 mm F.Cu**.
- `J8_5V`, `LOCAL_5V` and battery trunks: **0.8 mm In2.Cu**.
- GNSS/J8 3V3 trunk: **0.5 mm In2.Cu**.
- C1/C2 and C5/C6 are immediately adjacent to the corresponding XIAO VBUS entry.
- C3/C4/C7/C8 use **1210** footprints and remain DNP initially.
- GNSS C9/C10 are adjacent to the 3V3 end of the module header.

## Signal integrity

- UART and GNSS digital traces use **0.25 mm** signal traces referenced to the uninterrupted In1 GND plane.
- R1 and R3 are placed near host-side TX sources.
- R2 and R4 are placed near radio-side TX sources.
- GNSS SDA/SCL run together on B.Cu away from the local regulator/power-switch loops.
- PPS passes through R5 0R before reaching the host.

## RF / ground strategy

- No LoRa RF or GNSS RF is routed on the carrier; those paths remain on the plug-in modules and their coax/U.FL/SMA interfaces.
- Ground is not split below U2, U3 or U4.
- Ground stitching vias are placed around the board perimeter, along the host/radio boundaries and at the lower edge of the GNSS quiet region.
- The top-center GNSS region is marked as a quiet zone: no regulator switching node or radio power loop should enter it.

## Mechanical references still provisional

The layout intentionally does **not** drill the four Waveshare mounting holes yet. Their known center span is shown on `Dwgs.User`, but the final hole diameter and J8 registration must be checked on the physical Waveshare board.

The Uputronics breakout footprint uses the confirmed 1x7, 2.54 mm electrical header and an approximately 22 x 22 mm module envelope. The exact header-to-board-edge offset remains a physical-measurement gate.

## Required checks before fabrication

1. Measure the physical Waveshare J8 origin/orientation and all four mounting holes.
2. Measure the exact Uputronics breakout header offset and antenna/SMA envelope.
3. Freeze the `LOCAL_5V` regulator/module and replace the U1 placeholder.
4. Verify XIAO/Wio stack height, USB access and pigtail bend radius.
5. Run KiCad ERC, refill zones, run DRC and inspect every unconnected pad/net.
6. Confirm the selected fab's 4-layer stackup and via/annular-ring rules.
7. Bench-check J8 5 V headroom, Schottky drop and simultaneous LoRa TX transients before production routing is frozen.
