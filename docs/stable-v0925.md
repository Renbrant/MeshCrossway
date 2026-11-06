# v0.9.25 stable acceptance record

Historical acceptance record. [v0.9.28 is now the recommended stable image](stable-v0928.md); these exact v0.9.25 bytes remain previous stable recovery.

On 2026-11-05 hardware testing confirmed that v0.9.25 was working well and was stable, and this image was explicitly accepted as stable. This promotion changes release metadata and distribution only. It does not change or recompile firmware.

## Immutable tested image

- Version: **v0.9.25**, recommended stable in the Web Flasher.
- Build-info source: `65f4214b15fe9486a7f07e7471380114bcbf8826` (the tested PR merge build; its source tree matches main commit `538eb09e096a8a01aa4509915bfc0ec1c548e206`).
- Combined image: 2,212,096 bytes.
- SHA-256: `daeebb735024fcdcb474acdfe0887c3d72e608ba794ba1343e1b60749f3d6b23`.
- Successful preflight: [run 42125351561](https://github.com/Renbrant/MeshCrossway/actions/runs/42125351561), artifact `13461246425`.
- Original artifact ZIP SHA-256: `ac2bb8afc73245cc8d062df3237dd41cb424699ecb0945a4a142db1401896dd0`.
- Permanent image archive: [v0.9.25 release](https://github.com/Renbrant/MeshCrossway/releases/tag/v0.9.25).

The release retains the tested zoom control auto-close, separate Meshtastic/MeshCore marker settings, local OpenStreetMap night rendering, and MeshCore position visibility correction. Existing node list and memory improvements are retained.

## Observed hardware evidence

The submitted console excerpt contains 39 memory samples from one observed boot, uptime 723–154218 ms, plus two samples from a partial preceding boot. The complete observed interval recorded no allocation failures or dropped memory-log samples. Internal free heap reached a minimum of 18,739 bytes and finished at 20,287 bytes; DMA-capable free heap reached 11,207 bytes and finished at 12,755 bytes. DMA is a subset of internal memory, not additional memory.

No watchdog, panic, backtrace, or AES/TLS errors appear in this excerpt. The final touch diagnostic reports zero errors and zero overflows, maximum gap 111 ms, queue peak 24, and stack free 1108 bytes. The excerpt also records `rst:0x15 (USB_UART_CHIP_RESET)` at 16:02:33. That reset reason is not a watchdog diagnosis; the initiator cannot be established from this excerpt.

Successful hardware use and explicit acceptance establish this release status. The short console excerpt does not establish indefinite runtime stability. [Memory investigation #137](https://github.com/Renbrant/MeshCrossway/issues/137) remains open for longer observations.

## Recovery and publication rules

[v0.9.16](https://github.com/Renbrant/MeshCrossway/releases/tag/v0.9.16) remains selectable as previous stable recovery. Its pinned source is `d821b78979b3c2b9d716757f0cff02dbf89e776f`; its combined-image SHA-256 remains `54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a`.

The publisher restores approved images rather than recompiling them. Both combined and split images, build metadata, and install manifests are validated. Existing permanent release assets are verified and never overwritten; pinned Actions archives provide the initial bootstrap. NVS settings are preserved by default. Only manifest metadata may be normalized for that installation guard.

## Verification procedure

1. Run `python3 scripts/preflight_firmware_catalog.py` for installer and immutable-release regression checks.
2. Verify both actual images using `scripts/restore_stable_firmware.py --version VERSION --verify DIRECTORY`.
3. Generate and validate the catalog with stable/latest both set to v0.9.25; v0.9.16 must remain selectable.
4. After merge, require the Pages deployment and permanent-release archive job to succeed.
5. Check the public catalog, version labels and image checksum. On subsequent hardware testing, collect console logs across radio synchronization, reconnects, map pans and hotspot changes; record uptime, allocation failures, minimum internal/DMA heap, touch overflows and reset reason in issue #137.
