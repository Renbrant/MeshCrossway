# MeshCrossway Rev A - KiCad engineering layout baseline

This folder contains project-local footprints and layout notes for the MeshCrossway Rev A carrier.

## Board baseline

- 160 x 94 mm engineering carrier.
- 4 copper layers: F.Cu / In1.Cu / In2.Cu / B.Cu.
- In1.Cu continuous GND reference plane.
- B.Cu GND pour plus perimeter/subsystem stitching vias.
- In2.Cu main power trunks.
- Radio A on left edge, Radio B on right edge, GNSS top center.
- Tall headers/jumpers/regulator placeholder outside the Waveshare mechanical envelope.

## Fabrication gates

Before ordering a PCB, verify:

1. Waveshare J8 exact X/Y/orientation and mounting holes.
2. Waveshare mounting-hole drill diameter.
3. Uputronics MAX-M10S header offset/orientation.
4. Exact LOCAL_5V regulator/module and footprint.
5. XIAO/Wio stack height, USB-C access, U.FL pigtail bend radius and enclosure clearance.
6. Native KiCad ERC/DRC and zone refill.
7. Final 4-layer fab stackup.

The generated .kicad_pcb and matching schematic are being reviewed locally before they are treated as fabrication files.
