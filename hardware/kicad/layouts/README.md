# PCB layout alternatives

The MeshCrossway Rev A schematic is shared by multiple PCB floorplans.

## Layout A

Status: **preserved alternative**.

The earlier engineering layout explored a wider 4-layer carrier with the radios on opposite sides and GNSS separated from the radio power section. It is retained as a reference, not the active direction.

## Layout B

Status: **active**.

Layout B starts from a blank PCB and targets a **2-layer carrier**:

- F.Cu: components, power, UART/I2C/PPS and most routing.
- B.Cu: nearly continuous GND plane.
- Avoid bottom-layer routing beneath the XIAO/Wio RF modules and GNSS where it would cut the return plane.
- Begin with J1, U2, U3 and U4 only.
- Freeze the mechanical/RF floorplan before placing support components or routing.

When alternative PCB files are added here, keep their names explicit:

```text
MeshCrossway_RevA_LAYOUT_A.kicad_pcb
MeshCrossway_RevA_LAYOUT_B.kicad_pcb
```

Do not create separate schematics for the alternatives unless the electrical design itself changes.
