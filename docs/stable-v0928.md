# v0.9.28 stable acceptance and archive record

v0.9.28 was explicitly selected as stable on 2026-11-05, together with a request to update the repository and website feature/status documentation. This promotion changes documentation and distribution metadata only. No firmware source, settings defaults or tested binary is changed or recompiled.

## Immutable published image

- Version: **v0.9.28**, recommended Web Flasher default.
- Build-info source: `37a8caca1ce57e2795be78e7a40012890a6b7147` (PR #148 merge build).
- Source tree: `8455edb527b5e89fc15259516d6cbf1d3aedf344`, also the tree of main commit `14b6ec8610faf5f223c4c0e18ce735fd66055283`.
- Combined image size: **2,219,232 bytes**.
- Combined image SHA-256: `a36970339c00cc327515eac2648c5421e1b9aab460484919a6ed8f57cc040c61`.
- Successful ESP-IDF/preflight build: [run 42150205062](https://github.com/Renbrant/MeshCrossway/actions/runs/42150205062).
- Approved Actions archive: artifact `13471144384`, ZIP SHA-256 `1e6d628a0cb3c3a3508818349c05b03a3f4726d8ce1a032cba8cdc5fe350c58a`.
- Original successful deployment: [run 42150778676](https://github.com/Renbrant/MeshCrossway/actions/runs/42150778676).
- Permanent binary archive: [v0.9.28 release](https://github.com/Renbrant/MeshCrossway/releases/tag/v0.9.28).

The public image was downloaded and matched the approved archive before promotion. The stable publisher verifies combined and split images, source metadata, manifest safety and the independent approved checksum. The release assets are retained permanently and never overwritten. Future development builds must use a new version.

## Functionality included

The release includes simultaneous independent native Meshtastic/MeshCore BLE sessions, messaging and radio configuration; the local Wi-Fi browser companion; GNSS/clock; street/topographic maps, cached tiles and local night rendering; tracks, POIs, folders and GPX; bounded manual-refresh node lists; per-network markers and optional grouping; 180-degree landscape rotation, saved lock/manual correction; brightness, timeout and connectivity controls; memory/latency diagnostics and SD recovery.

Age colors and retention are described in the [v0.9.28 operating/test guide](node-age-v0928.md). Color by last reception is opt-in per network. Keep days accepts 0–365, defaults to 30 and uses 0 to disable expiry. Protected favorites, local identities, messages/drafts and uncertain timestamps remain. Local cache cleanup does not delete the radio firmware database or free fixed-capacity table reservations.

## Acceptance evidence and limits

Stable selection is based on explicit device acceptance. No new console log or timed endurance report accompanied this particular promotion request. Earlier driving/walking stability feedback was associated with v0.9.25; it is not relabeled as a v0.9.28 endurance test.

For v0.9.28, recorded automated evidence includes production-policy color/expiry boundaries, protocol import/prune behavior, protected entries, MT archive migration/recovery, real LVGL controls/NVS persistence, map schema migration and the existing radio/history/navigation suites. ESP-IDF compilation, firmware identity and stack/static-memory checks passed before the original publication. Continue device measurements and the hardware checklist in [issue #137](https://github.com/Renbrant/MeshCrossway/issues/137); it remains open. Broader radio, RF, battery, SD and final mechanical qualification remains ongoing.

## Recovery

- **v0.9.25 — previous stable:** source `65f4214b15fe9486a7f07e7471380114bcbf8826`; SHA-256 `daeebb735024fcdcb474acdfe0887c3d72e608ba794ba1343e1b60749f3d6b23`.
- **v0.9.16 — legacy recovery:** source `d821b78979b3c2b9d716757f0cff02dbf89e776f`; SHA-256 `54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a`.

Both remain selectable and byte-for-byte pinned. NVS credentials/bonds are preserved by default during installation. v0.9.28 MT snapshots use separate mt2 files; the preserved legacy mt files remain readable by earlier stable firmware, while newer candidate-era history stays in mt2 files.

## Promotion verification

Run installer/stable-release preflight, verify all three actual images with `restore_stable_firmware.py --version VERSION --verify DIRECTORY`, generate the catalog with stable/latest v0.9.28 and both recovery entries, and validate it. After merge, require successful Pages deployment and permanent-release archive verification. Check public recommended/latest values, statuses and the full image checksum again. The new stable release must retain the original approved bytes.
