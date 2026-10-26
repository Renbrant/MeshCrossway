# 3D models

This directory is used by the MeshCrossway KiCad project for mechanical visualization.

## Vendor models

Run from PowerShell:

```powershell
.\fetch_vendor_3d.ps1
```

The script downloads:

- **Seeed Studio XIAO ESP32-S3 official 3D model** from the Seeed resource package.
- **Seeed Studio Wio-SX1262 official STEP model** from Seeed's OPL KiCad library.
- **Waveshare ESP32-S3-Touch-LCD-3.5 official 2D/3D package** from Waveshare.

Downloaded vendor CAD is intentionally not committed by default. This keeps the repository small and preserves the original manufacturer files without duplicating multi-megabyte CAD assets in Git history.

The XIAO socket footprint references:

```text
${KIPRJMOD}/3dmodels/vendor/XIAO_ESP32S3.step
```

The Wio-SX1262 and Waveshare packages are downloaded for mechanical alignment work. Their final model offsets/orientations must be checked in KiCad against the physical boards before they are treated as fabrication references.

## GNSS

No exact manufacturer STEP model has been frozen for the Uputronics MAX-M10S breakout. The PCB footprint therefore carries the 22 x 22 mm mechanical envelope and an approximate SMA/header region. Verify the actual purchased module before fabrication.


## Current XIAO + Wio assembly transform

The U2/U3 footprint includes both vendor STEP models:

- `Wio-SX1262_for_XIAO.step`
- `XIAO_ESP32S3.step`

The current transforms below were adjusted and visually validated in KiCad against the carrier socket footprint on 2026-10-26.

### Wio-SX1262

- Offset: X = -114.5 mm, Y = 120.5 mm, Z = 5.5 mm
- Rotation: X = 0 deg, Y = 0 deg, Z = 0 deg

### XIAO ESP32-S3

- Offset: X = 6.222 mm, Y = -1.6722 mm, Z = 0.5 mm
- Rotation: X = 90 deg, Y = 0 deg, Z = 90 deg

These values are accepted as the **visual mechanical reference for Rev A floorplanning**. Before enclosure/fabrication sign-off, verify actual socket/header stack height and module orientation against the physical hardware.
