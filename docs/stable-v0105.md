# v0.10.5 stable acceptance and archive record

v0.10.5 was selected as the stable ESP32 controller release on 2026-11-09 after device use. The user reports that it is working very stably. This promotion updates release metadata and distribution; it preserves the exact compiled firmware image.

## Immutable firmware image

- Version: **v0.10.5**, recommended stable.
- Build source commit: `4afece0daa608dc2675f1d84eb383bf13931c0ff`.
- Combined image size: **2,385,216 bytes**.
- Combined image SHA-256: `04ff93cba20f09d53a338a15d8557b40474bb38089a78512cb9e8ae099dc8f10`.
- Reviewed Pages deployment: [run 42633516262](https://github.com/Renbrant/MeshCrossway/actions/runs/42633516262).
- Pages artifact ID: `13680863905`; ZIP SHA-256: `8dcc2cbed7b8e94c324055cc127ca6ef1fe56d1697a3d17f0ee791b568a79f05`.
- Permanent firmware archive: [v0.10.5 release](https://github.com/Renbrant/MeshCrossway/releases/tag/v0.10.5).

The image is checksum-pinned and must be archived byte-for-byte. The first permanent release is restored from the reviewed Pages artifact; subsequent builds use the permanent release assets. Do not rebuild the stable image from a publishing commit.

## Features

This release includes offline raster map packs and selected-area downloads, map pinch zoom and parent-tile overzoom, filtered node fit, saved-area previews, resumable downloads and SD cleanup that preserves tiles shared by other saved areas. It also includes the v0.9.33 Wi-Fi and saved-track controls.

## Acceptance and recovery

The stable designation records the user's report after using the device. It does not claim indefinite runtime, universal radio compatibility, or additional RF, battery, enclosure or SD-failure qualification.

- **v0.9.32 — previous stable:** exact recovery image retained, SHA-256 `87abaeb2b6bb9e954e752f9e610cd1150e3ebe7c8d11fe0c2652b98aa06daf2c`.
- **v0.9.28 — earlier stable recovery:** exact image retained, SHA-256 `a36970339c00cc327515eac2648c5421e1b9aab460484919a6ed8f57cc040c61`.
- **v0.9.16 — legacy recovery:** exact image retained, SHA-256 `54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a`.

