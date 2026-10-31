# Stable firmware v0.8.14

On 2026-10-31 the owner reported very good and apparently very stable operation after installing v0.8.14, and explicitly requested it as the latest stable release. This promotion records that owner acceptance; it does not imply exhaustive hardware qualification.

The Web Flasher selects v0.8.14 as recommended stable. It includes the Bluetooth resume stack correction, bounded HTTP navigation JSON, folder save confirmation, track folders and remote communication/map workspaces. Previous known-broken releases are not part of the supported installer line.

The exact tested binary is immutable and must not be rebuilt:

- Version: `v0.8.14`
- Compiled source ref: `9a232921ab3543d6221fc6eeee56678ad374dc28`
- Binary SHA-256: `8f3b70fedb2c8c4c2432dbe0d00801197cfe1d08ddf6c52d02668a0835f03c31`
- Binary size: 2106384 bytes
- Original successful preflight run: `41216307231`
- Original firmware artifact: `13048058040`
- Archive SHA-256: `b1e461549d8d3c122dd15cdb68021d3f8a406ec9bf289a12316d1b3c0209109b`

The build restores the pinned image even when latest and stable are equal, verifying its source identity and all checksums. It can recover from the reviewed archive if the public copy is unavailable. Historical stable records (including v0.8.11) remain in Git. New candidates require separate validation; networks remain independent with one active BLE connection.
