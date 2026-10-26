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
