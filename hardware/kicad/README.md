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


## Rev A battery mechanical envelope

For rear-side floorplanning, the project includes a mechanical-only footprint and a simple 3D envelope for the currently selected 1S LiPo battery:

- Footprint: `MeshCrossway:Battery_LiPo_10000mAh_110x60x12`
- 3D model: `${KIPRJMOD}/3dmodels/generated/Battery_LiPo_110x60x12.step`
- Nominal battery: 3.7 V, 10000 mAh, 37 Wh
- Envelope used: **110 x 60 x 12 mm**
- Advertised connector: **PH 2.0 mm**
- Advertised wire length: approximately **100 mm**
- Intended placement: **back side of the carrier PCB**

This is a **mechanical planning model**, based on the supplied product dimensions, not an official manufacturer CAD file. Verify the production battery dimensions, cable exit, connector polarity, swelling allowance, adhesive/foam thickness, and enclosure clearances before fabrication.


## Uputronics MAX-M10S mechanical model

The project includes an approximate 3D/mechanical model for the Uputronics u-blox MAX-M10S 3V breakout used in Rev A:

- Footprint: `MeshCrossway:Module_Uputronics_MAX_M10S_3V_MECH_APPROX`
- 3D model: `${KIPRJMOD}/3dmodels/generated/Uputronics_MAX-M10S_3V_MECH_APPROX.wrl`
- Published breakout PCB size: approximately **22 x 22 x 1.6 mm**
- Published header pitch: **2.54 mm**
- Published antenna connector: **SMA female**
- The model includes a simplified MAX-M10S, supercapacitor and edge-launch SMA.

The SMA barrel intentionally extends beyond the 22 mm PCB edge so the carrier PCB can be floorplanned with the GNSS antenna connector exposed at the enclosure/carrier edge. The exact SMA overhang and component placement are **mechanical approximations** because Uputronics does not publish a dimensioned CAD drawing for the complete assembled breakout. Verify against the physical module before fabrication.
