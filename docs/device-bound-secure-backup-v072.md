# v0.7.2 — Device-Bound Secure BLE Backup (design and implementation plan)

**Status:** proposed/design-only. No runtime firmware changes, no eFuse write, no bootloader/partition modifications, no change to the Web Flasher. This document is an engineering specification and checklist, **not** a claim that secure backup is already functional. Target: Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B, ESP-IDF **5.3.2**, LVGL 8.x, NimBLE Central.

## 1. Objective and non-goals

Preserve the *Bluetooth bonding keys* (not a reusable six-digit PIN) for known, trusted MeshCore radios and eventual Meshtastic radios on the Core's removable microSD. If the Core NVS is erased and the **same physical ESP32-S3** still exists with intact eFuse hardware key, its saved bonds can be restored and used without entering the PIN again **if the radio still retains its corresponding bond**. A removed SD card alone, inserted into a laptop/other ESP32-S3, must not reveal the plaintext credentials.

This is **not** cross-network messaging, a bridge or relay between Meshtastic and MeshCore, generic disk encryption, a portable backup usable on a replacement Core, protection against malicious firmware running on the original Core, a firmware PIN export, or a way to recover radio-side bond keys after the radio has reset. Do not implement any BLE auto-resend or automatic cross-network routing.

### Current repository baseline and coupling

- `firmware/display-prototype/meshcrossway_ble.h`: NimBLE Central; `CONFIG_BT_NIMBLE_NVS_PERSIST=y`, max 4 bonds, MITM and SC, `ble_store_config_init()`, `ble_gap_security_initiate()`. Reports `desc.sec_state.bonded`; BLE startup remains explicitly opt-in. **Physical bond reuse still requires testing**.
- `firmware/display-prototype/meshcrossway_backup.h`: existing **plaintext Wi-Fi JSON** at `/sdcard/meshcrossway-wifi.json`; its own worker, .tmp/.bak rotation, `fflush/fsync`, FAT32 mounted through `esp_sdcard_port_init()`. Must remain separate: do not claim entire SD is encrypted and do not overwrite this file.
- `firmware/display-prototype/meshcrossway_network.h`: starts Wi-Fi and mounts the SD via the existing backup. Verify init sequencing before adding secure restoration; avoid duplicate mounts, races or access from LVGL callbacks.
- `scripts/write_firmware_version.py`, `scripts/write_firmware_catalog.py` and `docs/site/flasher/index.html`: split images from v0.5.8 preserve NVS when Erase is **unchecked**, and the recent hotfix restores an explicit erase prompt. Never silently erase NVS while testing.
- `firmware/display-prototype/VERSION` is currently v0.7.1; v0.6.8 is the owner-tested rollback. **This design PR must not bump VERSION or publish v0.7.2 until real implementation and evidence exist.**

## 2. Root of trust: eFuse instead of MAC/serial

ESP32-S3 offers six 256-bit key blocks `EFUSE_BLK_KEY0..KEY5` with per-key purposes. The HMAC peripheral's software-accessible **upstream** mode uses `ESP_EFUSE_KEY_PURPOSE_HMAC_UP` and `esp_hmac_calculate()` on ESP-IDF v5.x. The MAC address/serial number is **public identity, not key material**, and shall never be the only decryption secret.

**Provisioning gate, read-only first**:
1. On the actual, owned Core, record a **redacted** eFuse inventory: chip variant/revision, count of unused 256-bit key blocks, existing purposes and read/write protection status, secure-boot/flash-encryption/debug state, and SD/partition health. Do not publish the full eFuse dump, MAC address or confidential fields.
2. Verify an *unused* permitted 256-bit key block exists and that its irreversible assignment does not collide with current/future Secure Boot, flash/NVS encryption, JTAG/DS features or manufacturer reservations. The chosen block is **not hardcoded to KEY0**.
3. Develop and test using a software mock and **a disposable lab board** before any production Core provisioning.
4. Provide an explicit, opt-in, double-confirmed, local-only provisioning ceremony with a read-only preview; no provisioning in `app_main`, ordinary boot, Web Flasher, SD insertion or implicit backup. Generate independent 256-bit random secret via a validated ESP32 CSPRNG, burn once with the ESP-IDF eFuse API to purpose `HMAC_UP`, set the appropriate protections, verify purpose/lock and HMAC operation, and securely clear temporary RAM. Never print/export/reuse a baked-in shared key.
5. After provision, the eFuse secret survives a flash erase. It cannot be restored on a replacement chip and must never be overwritten. An already programmed/incompatible key, insufficient free blocks or test failure blocks provisioning and exposes a clear non-secret status.

**IMPORTANT threat boundary:** HMAC_UP returns a derived value to *software*. Untrusted/malicious firmware executing on the **same** ESP32-S3 may call this primitive and decrypt the SD backup. This protects primarily against **SD-only extraction** and different devices, not against arbitrary firmware replacement or invasive attacks. Secure Boot + Flash Encryption / secure provisioning are separate, high-risk decisions and require a USB/Web Flasher impact assessment before any change.

## 3. Cryptographic envelope (format v1, pending fixture approval)

- Confidentiality + integrity: mbedTLS **AES-256-GCM**, 32-byte key, 12-byte unique cryptographically random nonce per encryption, full 16-byte authentication tag. Never reuse a (key,nonce) pair.
- Key derivation: software calls `esp_hmac_calculate(eFuse_key_id, "MCX|BLE-BONDS|v1|AES-GCM" || file_salt)`, where `file_salt` is an independent 16- or 32-byte random value stored in the authenticated header. The resulting 256-bit HMAC-SHA256 is used as the AES-256 key. Domain separation must be fixed and tested against independent known-answer fixtures. If migrating to HKDF, document the exact extract/expand construction and version the format; do not silently change it.
- Public header fields (exact byte order, canonical encoding, length and version frozen in a fixture before coding): magic `MCXB`, format version, algorithm and KDF IDs, salt, nonce, ciphertext length, record count. The **entire header** is GCM AAD, including length. Secret plaintext contains the native bond fields, no network message contents. Omit MAC/serial by default; if a nonsecret device identifier is included, it is merely a diagnostic hint and is covered by AAD, never a cryptographic key.
- File paths: `/sdcard/meshcrossway/security/ble-bonds.v1.enc`; atomic staging `ble-bonds.v1.tmp` and last-known-good `ble-bonds.v1.bak`. FAT32 filesystem; do not assume permission bits or ACLs are a confidentiality boundary.
- Enforce small fixed limits: MAX_BONDS=4 (effective firmware limit), MAX_FILE_BYTES=16 KiB including metadata, strict allowed format/version, per-field length validation and identity checks. Explicit deterministic serialization (canonical TLV/CBOR); **no raw C struct memcpy, complete NVS partition dump, unchecked JSON, or arbitrary pointer serialization**.
- Cryptographic failures are indistinguishable to external callers (e.g. `backup unavailable / cannot unlock`); internal nonsecret status can distinguish missing SD, unsupported version, wrong key/tag, IO error. Never log passkeys, LTKs, IRKs, decrypted records, raw keys, HMAC output, RNG state or plaintext.
- Zero key material and plaintext with `mbedtls_platform_zeroize` / equivalent, never persist plaintext in SD temp files or expose through screenshot/HTTP/diagnostic export.

## 4. NimBLE store integration: crucial feasibility gate

**Do not assume copying a raw NVS namespace is a supported export format.** First inspect/pin the **actual ESP-IDF 5.3.2 bundled NimBLE revision and Espressif NVS persistence backend** and prove *typed enumeration + read/write* of all bond records needed for reconnect. Likely interfaces to investigate include `host/ble_store.h`: `ble_store_read_our_sec`, `ble_store_read_peer_sec`, `ble_store_write_our_sec`, `ble_store_write_peer_sec`; CCCD and local identity/IRK storage may also be relevant. Their signatures and enumeration behaviors are **not yet verified against the pinned source**. Audit privacy addresses, IRK/LTK/EDIV/RAND, key distribution and whether our/peripheral secrets are both necessary. Respect key eviction/re-pairing semantics and the four-slot cap.

Use a **versioned native-record adapter** with snapshot validation. If a complete/portable bond round-trip cannot be verified safely in the exact NimBLE backend, the feature must remain **blocked/unsupported**, not pretend that backing up a PIN is equivalent. Do not overwrite the entire NVS partition: it contains unrelated Wi-Fi settings and may vary by SDK version.

## 5. Write and restore lifecycle

**Backup** (background FreeRTOS worker, never LVGL/BLE callback): after a durable authenticated bond, deduplicated/debounced, collect a coherent snapshot from NimBLE storage after its NVS commit, serialize typed records, derive ephemeral encryption key, encrypt and seal with GCM, write exclusively to .tmp, `fflush` + `fsync` + `fclose`, read-back/authenticate/parse the candidate, rotate valid .enc -> .bak and rename .tmp -> .enc. Preserve last known good on any failure, never claim success before the verified commit, and avoid UI/render/SD contention.

**Restore**: after NVS init and SD mount, **before `nimble_port_init` / `ble_store_config_init()` or at a rigorously documented safe NimBLE store initialization stage**. Only try auto-restore when the relevant bond store is empty/erased and eFuse is provisioned. Validate file bounds, AAD/tag, format, schema, per-record ownership, max 4 bonds, key distributions and compatibility. Restore through the validated typed store interface, verify subsequent reads, then initialize/use the NimBLE stack. Never overwrite nonempty live bonds automatically; when conflict exists, show `manual review needed` and keep active NVS. If `.enc` fails, try authenticated `.bak`; never accept invalid tag. No implicit change of radio configuration and no unsolicited BLE/LoRa traffic.

If backup is older than the radio's state (radio reset, key rotation, peer deleted), reconnect can still fail; fall back to a **new secure pairing**, not automatic trust or security downgrade. A wrongly paired/untrusted radio must not inherit another peer's bond. Separate records by protocol/radio identity, but **never bridge, synchronize or relay** Meshtastic and MeshCore networks.

**Erase semantics**: An ESP Web Tools erase should remove the Core NVS but not its eFuse key. With the original SD and intact eFuse provisioning, future firmware can restore bonds. Verify recovery even after a full erased-flash reflash (only on a test unit, with explicit authorization). Normal updates with `Erase` unchecked must preserve everything without requiring SD restore.

## 6. UI and failure handling

Settings > Security & Backups (proposed within existing More UI, not app store): `Not provisioned`, `Ready`, `Backup current`, `Last verified backup`, `Restored`, `SD unavailable`, `Wrong device / bad tag`, `Incompatible format`, `Needs radio re-pair`. Never claim "PIN saved" when there is no bond. Show only counts and timestamps/backup generation; no secrets. Provisioning is disabled until the read-only preflight has passed and explicit local confirmations occur. Add manual `Restore backup` with confirmation and non-destructive conflict resolution, not arbitrary overwrites.

## 7. Milestones and gates

| Stage | Work | Pass criteria |
| --- | --- | --- |
| A. Read-only investigation | eFuse safety inventory on actual board; inspect 5.3.2 NimBLE store and partition map; confirm SD mount semantics | No writes; verified complete API/schema feasibility and reserved key purpose |
| B. Host cryptography | Build deterministic v1 serialization and AES-GCM vector fixtures with simulated per-device HMAC secrets | Different keys/tampered bytes fail; unique salt/nonces; bounds/fuzz tests |
| C. BLE store adapter | Implement typed export/import behind feature gate, including local/peer privacy material | Reconnect after NVS-clear on disposable test board, all records consistent |
| D. eFuse provisioning | Explicit irreversible one-time provisioning on disposable test board first; later owner-approved Core | Read/write protection and HMAC; no automatic burn; secure logs |
| E. SD durability/UI | Async atomic journal, encryption, restore order, UI states | Power-loss/corruption tests pass without breaking Wi-Fi or touchscreen |
| F. Physical acceptance and publication | Battery/SD/BLE restart and erase/reflash matrix, review CI + artifacts | Owner hardware confirms MC bond reuse; only then enable production/release v0.7.2 |

## 8. Hardware answers still needed, not guessed

These **cannot be established from GitHub alone**: actual eFuse KEY block availability/protection, secure boot/flash encryption state, true BLE bond persistence and key types on the Heltec/XIAO firmware, SD behavior while BLE+Wi-Fi+touch are active, and behavior after deliberate erase. Conduct a **read-only** inventory (e.g. Espressif `espefuse.py -p <PORT> summary` on a connected development workstation), redact identifiers/secret fields, and attach a summarized checklist to the engineering issue, not a raw dump. Never issue an eFuse `burn_*` command from a standard firmware update.

## 9. Verification and product constraints

- Every change keeps `v0.6.8` rollback immutable; does not reintroduce remote touch or a web capture PIN, and leaves Wi-Fi and NVS partitions intact on a no-Erase update.
- Works with SD absent (BLE simply uses NVS); fail closed for corrupt/foreign backup; secure backup status must not prevent normal boot or pairing.
- AES-GCM decryption failure MUST NOT emit partial plaintext or restore a subset of bonds.
- Old/rollback firmware does not understand this file; never backport misleading automatic recovery claims to earlier versions.
- eFuse secrets are lost when the **entire ESP32-S3 chip is replaced**; restore to a different chip is explicitly forbidden by design.
- The existing Wi-Fi SD JSON is **still unencrypted**; schedule a separate migration if complete-card confidentiality is required. Avoid copying that file into this BLE backup without a migration plan.

## Sources inspected

- [Espressif HMAC ESP32-S3 (IDF v5.x API)](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-reference/peripherals/hmac.html)
- [Espressif eFuse Manager, IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32/api-reference/system/efuse.html)
- [Espressif ESP32-S3 Security, IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/security/security.html)
- [Apache NimBLE typed security store API — candidate, exact pinned version still to verify](https://github.com/apache/mynewt-nimble/blob/master/nimble/host/include/host/ble_store.h)
- [ESP-IDF NimBLE NVS persistence option](https://github.com/espressif/esp-idf/blob/master/components/bt/host/nimble/Kconfig.in)
- [espefuse read-only summary command](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/espefuse/summary-cmd.html)

**Release gate:** Only after stages A–F are satisfied should firmware VERSION advance. This design PR authorizes *planning*, not burning hardware keys or publishing an unvalidated security feature.
