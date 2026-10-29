# Secure microSD storage — implementation stage A (eFuse inventory)

Related: [Issue #46](https://github.com/Renbrant/MeshCrossway/issues/46) (persistent encrypted history), [Issue #47](https://github.com/Renbrant/MeshCrossway/issues/47) (device physical key) and separate bond-backup design [draft PR #42](https://github.com/Renbrant/MeshCrossway/pull/42).

## Implemented in experimental v0.7.5

- **More > Diagnostics** now reports **public eFuse metadata only**: number of unused 256-bit blocks, HMAC_UP key presence, and missing protection.
- The display explicitly states **Storage vault: NOT ACTIVE**. A pre-existing protected HMAC_UP key is *unverified*; never assume it belongs to MeshCrossway.
- A native host C++11 test covers unprovisioned, protected/unprotected HMAC, occupied slots, invalid metadata, and buffer safety. A script prohibits eFuse burn/set/read-secret APIs in the audit header.
- Pinned ESP-IDF v5.3.2 APIs: esp_efuse_key_block_unused, esp_efuse_get_key_purpose, esp_efuse_get_key_dis_read, esp_efuse_get_key_dis_write, esp_efuse_get_keypurpose_dis_write.
- **There is no hardware key write, flash encryption or Secure Boot activation, NVS modification, SD write, storage key derivation, or BLE message change.**

## Still missing before secure history exists

1. Manual maintenance provisioner only on a physical test device; double consent before any irreversible burn.
2. Storage-vault enrollment and authenticated HMAC/HKDF key derivation; AES-256-GCM encrypted journal and crash-safe recovery.
3. Persist MeshCore messages independently of BLE session; pagination, dedup, read state, no auto-retransmission, SD status.
4. Future Meshtastic storage with separate namespace and keys, without bridging the two networks.
5. Separate migration of existing **plaintext** Wi-Fi SD backup. This PR deliberately does not touch it.
6. Privacy hardening of the current unauthenticated LAN screenshot endpoint before offering confidential chat screenshots.

## Hardware test and security limits

After CI and candidate release approval, install **without Erase** and open More > Diagnostics. Share only the status and count of free slots; **never share a full eFuse dump or a HMAC digest**. Do not enroll or burn on the production Waveshare until a separate test-board process passes and a human explicitly authorizes the irreversible action.

Read-only metadata reporting is not proof that history is encrypted. eFuse-HMAC alone protects removable-SD privacy but **does not defend against malicious firmware running on the same ESP32-S3** without trusted boot.

References: [ESP-IDF 5.3 HMAC](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/peripherals/hmac.html), [ESP-IDF eFuse Manager](https://docs.espressif.com/projects/esp-idf/en/v5.3.4/esp32s3/api-reference/system/efuse.html).
