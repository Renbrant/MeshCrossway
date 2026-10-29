# MeshCrossway secure storage — Stage B crypto primitives (not activated)

Related: [#46](https://github.com/Renbrant/MeshCrossway/issues/46), [#47](https://github.com/Renbrant/MeshCrossway/issues/47), and the read-only eFuse audit deployed in v0.7.5.

## What this stage implements

- Header-only C++11 **AES-256-GCM** (96-bit nonce, 128-bit tag) using ESP-IDF **mbedTLS** on the device. A corresponding OpenSSL backend is compiled **only for host tests** (`MCX_VAULT_HOST_TEST`).
- **HMAC-SHA256 challenge → HKDF-SHA256** (32-byte key) with domain separation by store identity, radio fingerprint, random salt and network identifier. A **mock HMAC provider exists only in host tests**. There is deliberately **no esp_hmac_calculate call, eFuse write, chip-specific key or hard-coded production secret**.
- Authenticated encryption protects message bytes and checks supplied AAD. The caller must include schema version, protocol, radio/store IDs, record type and monotonically assigned record ID in AAD in the future journal. On decryption failure, the destination buffer is zeroed.
- Bounded primitive: 512 bytes maximum message/record plaintext, 32-byte derived key, 12-byte nonce, 16-byte tag, 16-byte store ID and salt, 32-byte radio fingerprint. The header does not allocate a heap buffer; crypto backends may have their own internal context costs.
- Host test: RFC5869 HKDF test, NIST AES-GCM vector, different keys for MT and MC even using the same mock eFuse secret, changing radio/store IDs, tampering with ciphertext/tag/AAD/nonce/key, no plaintext escape on failure, invalid lengths and 2,000 encrypt/decrypt pairs (benchmark **on CI host only**).
- Code is **included for compile checking** in the firmware, but **never invoked** at boot, by the UI, or by a BLE task. No change in current chat, SD or eFuse behavior. Thus this stage does **NOT** fix disappearing chat history yet.

## Critical security gates remaining

1. **Nonce uniqueness** is not guaranteed by this primitive. Journal stage C must generate a new random segment prefix + counter (or appropriately strong unique random nonce) and prevent reuse with a given key, including across unexpected power loss; otherwise AES-GCM privacy/integrity can fail catastrophically. Do not persist messages using the primitive alone.
2. **Authenticated enrollment** and physical HMAC_UP provisioning remain missing. Do not select an eFuse block automatically. The mock used in host testing must not be delivered in firmware.
3. **Crash-safe SD journal**, validated record headers, AAD construction, secure indexes and replay protection, and temporary write/rename + fsync handling must be implemented and verified separately before saving real messages.
4. **Do not merge** until IDF ESP32-S3 compile and review are green. This branch deliberately retains VERSION v0.7.5 and is a staging PR **not intended for Web Flasher publication**. Before any future merge, assign a new unique version so no different binary is republished under v0.7.5.
5. **Performance:** the benchmark is a **host** elapsed-time measurement, not a Waveshare CPU reading. Device benchmark must measure internal SRAM/DMA free, PSRAM, encrypt/decrypt CPU time, SD fsync latency and touchscreen/BLE responsiveness with real hardware.
6. **Privacy gaps:** current Wi-Fi backup on the SD remains plaintext; unauthenticated LAN screenshot may expose visible conversations. Do not claim the device currently protects message history.

## Architecture for Stage C

```text
Radio event (MC or MT, never bridged)
   -> protocol-specific validation / stable identity / dedup
   -> storage queue (bounded, independent of LVGL and BLE)
   -> segment nonce sequencer + authenticated record header
   -> vault key derived from authenticated physical HMAC_UP + salt + network/radio
   -> AES-256-GCM seal (this stage)
   -> append-only FAT32 segment + sync + crash recovery
   -> paged history loaded from SD -> read-only UI
```

No new app, no telemetry, no change to existing BLE or SD files.

## Reproduce tests

From repository root on a Linux host with OpenSSL development headers installed:

```bash
python3 scripts/test_storage_crypto.py
```

Tests run under AddressSanitizer and UBSan. Production cryptographic code uses ESP-IDF's mbedTLS implementation. Device benchmark and real-world reliability require a separately reviewed build.

## References

- [ESP-IDF HMAC ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/peripherals/hmac.html)
- [AES-GCM specification NIST SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final)
- [RFC 5869 HKDF](https://www.rfc-editor.org/rfc/rfc5869)
