# MeshCrossway Rev A — KiCad project

This folder is the canonical home for the MeshCrossway Rev A KiCad design files and project-local footprint library.

## Current layout strategy

The Rev A schematic is shared by multiple PCB layout alternatives. The file `MeshCrossway_RevA.kicad_pcb` in this directory is the **active Layout B** and is intentionally blank/2-layer at the start so footprints can be pulled from the schematic with F8.

- **Layout A** — preserved alternative based on the earlier 4-layer engineering floorplan.
- **Layout B** — **active layout**, restarted from a blank PCB and targeting a **2-layer board**.
  - F.Cu: components, power and most signal routing.
  - B.Cu: keep as a nearly continuous GND reference plane; use bottom routing only when necessary and avoid cuts under RF modules or critical signals.

The schematic and footprints are common to both layouts. Do not duplicate or fork the schematic just to create a new PCB floorplan.

## Recommended project structure

```text
hardware/kicad/
├── MeshCrossway_RevA.kicad_pro
├── MeshCrossway_RevA.kicad_sch
├── MeshCrossway_RevA.kicad_pcb   # active Layout B
├── fp-lib-table
├── MeshCrossway.pretty/
│   ├── Waveshare_J8_Mating_2x16_P2.54mm.kicad_mod
│   ├── Module_XIAO_ESP32S3_WioSX1262_Socket.kicad_mod
│   ├── Module_LOCAL_5V_3Pin_Prototype.kicad_mod
│   └── Module_Uputronics_MAX_M10S_3V_MECH_APPROX.kicad_mod
└── layouts/
    ├── README.md
    ├── MeshCrossway_RevA_LAYOUT_A.kicad_pcb
    └── MeshCrossway_RevA_LAYOUT_B.kicad_pcb
```

The `layouts/*.kicad_pcb` files are alternatives. The schematic remains the electrical source of truth.

## Project-local footprint library

`fp-lib-table` registers:

```text
MeshCrossway -> ${KIPRJMOD}/MeshCrossway.pretty
```

When the project is opened from this folder, symbols that reference `MeshCrossway:<footprint>` resolve without any global KiCad-library configuration.

Current custom footprints:

- `Waveshare_J8_Mating_2x16_P2.54mm`
- `Module_XIAO_ESP32S3_WioSX1262_Socket`
- `Module_LOCAL_5V_3Pin_Prototype`
- `Module_Uputronics_MAX_M10S_3V_MECH_APPROX`

## Starting Layout B from scratch

1. Open `MeshCrossway_RevA.kicad_pro` from this directory.
2. Open the schematic and verify the project-local footprint library loads.
3. Create a blank PCB and save it as `layouts/MeshCrossway_RevA_LAYOUT_B.kicad_pcb`.
4. Open the existing blank `MeshCrossway_RevA.kicad_pcb` (already configured as a 2-layer starting board) or use `layouts/MeshCrossway_RevA_LAYOUT_B.kicad_pcb` as an untouched copy.
5. Use **Tools -> Update PCB from Schematic (F8)**.
6. Confirm zero footprint-library errors before applying the update.
7. Start with mechanical floorplanning only: J1, U2, U3 and U4. Do not route until the module positions and board outline are accepted.

## Fabrication gates

Before ordering a PCB, verify:

1. Waveshare J8 exact X/Y/orientation relative to the display PCB and mounting holes.
2. Waveshare mounting-hole drill diameter and exact mechanical coordinates.
3. Uputronics MAX-M10S header offset/orientation; the current module envelope is marked `MECH_APPROX`.
4. Exact `LOCAL_5V` regulator/module and final footprint.
5. XIAO/Wio stack height, USB-C access, U.FL pigtail bend radius, antenna keep-out and enclosure clearance.
6. Native KiCad ERC/DRC and zone refill.
7. For Layout B, preserve a nearly continuous B.Cu GND plane and review every bottom-layer trace for return-path disruption.

## Source control rules

Commit the actual design sources:

- `*.kicad_pro`
- `*.kicad_sch`
- `*.kicad_pcb`
- `*.kicad_mod`
- `fp-lib-table`

Do not commit editor locks, personal settings, autosaves or automatic backup folders. See `.gitignore` in this directory.
