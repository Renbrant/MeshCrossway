# v0.9.32 stable acceptance and archive record

v0.9.32 was selected as stable on 2026-11-07 after device use. The user reports the version is functioning very well and remains extremely stable. The exact compiled image is retained unchanged; the promotion updates release metadata and the Web Flasher selection only.

## Immutable firmware image

- Version: **v0.9.32**, recommended stable image.
- Build source commit: `c83f21adc5a3b35486ed7bd70fa6e5260cc13cce` (the verified PR merge tree, identical to the merged main tree).
- Main merge commit: `50226c0503962280d4990ca2d289e1243455fe33`.
- Combined image size: **2,348,112 bytes**.
- Combined image SHA-256: `87abaeb2b6bb9e954e752f9e610cd1150e3ebe7c8d11fe0c2652b98aa06daf2c`.
- Successful ESP-IDF build and firmware archive checks: [PR preflight run 42316318229](https://github.com/Renbrant/MeshCrossway/actions/runs/42316318229).
- Reviewed build artifact: `firmware-version-archive-preflight`, artifact `13551122742`, ZIP SHA-256 `6278a40dfcedca12c421265e8d6cd27141fa3cbf6d80610e50d6b3a10cb1c35c`.
- Successful initial publication of the v0.9.32 image: [Pages deployment run 42316952745](https://github.com/Renbrant/MeshCrossway/actions/runs/42316952745).
- Permanent firmware archive: [v0.9.32 release](https://github.com/Renbrant/MeshCrossway/releases/tag/v0.9.32).

The release workflow restores the reviewed artifact, verifies its archive digest, checks the embedded version and image checksum, and publishes the exact approved files. A promotion must never rebuild or replace this stable image. Future development starts at a higher version.

## Functionality included

The release restores the v0.9.31 raster map, adds two-finger pinch-to-zoom and a saved **Larger map labels / 1.25x** option. Magnification applies to the composed map and overlays, leaves cached tile files and downloads unchanged, and trades visible area for larger map details. Raster street-label typography is baked into each tile and cannot be resized independently.

It also retains the v0.9.28 age colors and node-retention controls, display orientation controls, independent dual-radio BLE sessions, local Wi-Fi companion, GNSS, tracks, POIs, folders and GPX handling.

## Acceptance evidence and limits

Stability selection is based on the user's explicit report after using v0.9.32 on the device. The report confirms normal operation and very high stability. It does not include a separate timed endurance log or new radio, battery, RF, SD or enclosure qualification. Existing project hardware validation remains applicable only to the configurations actually tested.

Automated evidence includes the map navigation host test, display preflight, firmware compilation with ESP-IDF 5.3.2, embedded-version checks, catalog/manifest verification and archive checksum validation. The user should continue to observe map gestures, Wi-Fi/BLE coexistence and track behavior during normal use.

## Recovery

- **v0.9.28 — previous stable:** exact published image retained, SHA-256 `a36970339c00cc327515eac2648c5421e1b9aab460484919a6ed8f57cc040c61`.
- **v0.9.25 — earlier stable recovery:** exact tested image retained, SHA-256 `daeebb735024fcdcb474acdfe0887c3d72e608ba794ba1343e1b60749f3d6b23`.
- **v0.9.16 — legacy recovery:** exact tested image retained, SHA-256 `54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a`.

The Web Flasher selects v0.9.32 by default and keeps all three recovery images available. For normal updates, leave the erase option unchecked to preserve saved Wi-Fi and Bluetooth credentials.
