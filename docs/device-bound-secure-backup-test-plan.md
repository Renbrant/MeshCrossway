# future version (TBD) — secure backup verification matrix (gates, no eFuse writes in this PR)

This file provides an executable acceptance checklist for the separate design at [device-bound-secure-backup-plan.md](device-bound-secure-backup-plan.md). Pass/fail fields are deliberately **not marked passed**: no physical Core has been inspected or provisioned under this proposal.

## Gate 0 — non-destructive inventory

- [ ] Read only the target Core's eFuse configuration. Confirm ESP32-S3 silicon revision, available KEY0..KEY5 blocks, purpose/read/write protection, debugger restrictions, Secure Boot and Flash Encryption states. Redact hardware IDs before committing any results.
- [ ] Validate exact ESP-IDF v5.3.2 `esp_hmac_calculate` / `esp_efuse_write_key` signatures in actual resolved build and key block availability. Check HMAC_UP does not compete with other planned keys.
- [ ] Pin the NimBLE backend revision used by this build. Prove typed export + restore for all keys, IRKs and privacy identities with 4 bonds; document schema.
- [ ] Confirm eFuse is not provisioned by boot/flash/recovery. Confirm non-erase Web Flasher leaves NVS untouched and SD FAT32 mounts once, at startup.
- [ ] Confirm `/sdcard/meshcrossway-wifi.json` remains plaintext and is NOT a security backup covered by this feature; document separate remediation.

**No activation until all Gate 0 items pass.**

## Gate 1 — independent host fixtures (no hardware secret needed)

| Case | Expected |
| --- | --- |
| Encrypt/decrypt canonical fixture with mock eFuse-HMAC key | Exact byte-for-byte round-trip |
| Second independent device-key with identical file | Reject: GCM authentication fails |
| Modified AAD/magic/version/record count/ciphertext/tag | Reject before any bond import |
| Truncated, oversized (over 16 KiB), malformed lengths | Bounded failure, no allocation explosion |
| Repeated saves, random salt+nonce | Independent IV; no key/nonce reuse |
| Reboot/same fake device-key | Recover exactly same records |
| Cross-format, future-version or unsupported feature bits | Fail safely, never reinterpret bytes |
| Fuzz serialization/deserialization and verify no over-read | ASan/UBSan clean, deterministic outcomes |
| Secret logging and snapshot/HTTP exports | Zero secret exposure |
| Power loss before/after write/fsync/rename | Either old verified or new verified backup; never partial accepted |

## Gate 2 — BLE NimBLE adapter

| Case | Expected |
| --- | --- |
| Export 0/1/4 bond records | Typed, versioned, complete, bounded |
| Five peers on capacity-4 firmware | Match configured eviction policy; never claim five saved |
| Local and peer key material (IRK/LTK, identity/address types) | Round-trip and retain authentication |
| Two radios with different identities | No mixing or mistaken peer assignment |
| Meshtastic vs MeshCore radio labels | Bookkeeping remains separate, no protocol conversion |
| Current NVS has valid bonds, SD contains older snapshot | Do not overwrite NVS |
| Corrupted SD snapshot, NVS valid | Keep live bonds and normal operation |
| Empty/erased NVS, authenticated SD snapshot | Restore before NimBLE begins, confirm readback |
| SD removed, filesystem fails, insufficient memory | Continue safely with NVS, report unavailable |
| Radio reset/forgot Core, backup restored | New PIN pairing requested; do not weaken security |
| Stale backup would reintroduce revoked bond | Do not restore automatically without revocation/conflict policy; record decision |

## Gate 3 — irreversible provisioning (disposable board first)

- [ ] Document and approve eFuse key purpose and selection.
- [ ] Generate secret from CSPRNG, keep it transient, burn to eFuse **only after two explicit local confirmations on a disposable test board**.
- [ ] Verify read protection, correct purpose and stable HMAC results after power cycle.
- [ ] Verify same SD encrypted backup fails on a second ESP32-S3 with different eFuse secret.
- [ ] Verify erased flash on the original board retains eFuse secret and successfully restores.
- [ ] Demonstrate inability to program an occupied block; no automatic fallback/block reuse.
- [ ] Document that a chip replacement makes its device-bound backup unrecoverable.

## Gate 4 — UI/firmware coexistence

- [ ] All firmware changes remain opt-in; boot not blocked by missing SD.
- [ ] Touchscreen, LVGL, Wi-Fi AP+STA, read-only web screenshot and BLE remain responsive during repeated backup/restore cycles.
- [ ] No extra reset, watchdog, SPI LCD queue or SD corruption during 20+ repeated writes and 10+ restarts.
- [ ] Erase-device explicit selection still present; installing without erase preserves NVS.
- [ ] Restoring after erase involves no outgoing LoRa messages or automatic message replays.
- [ ] No secrets in GitHub Actions artifacts, serial logs, HTTP pages, screenshots or diagnostic exports.
- [ ] Physical Core evidence available before announcing "future version (TBD) functional", with reversible rollback to immutable owner-tested v0.6.8.

## Gate 5 — threat/UX disclosures

- [ ] Removed SD alone is cryptographically protected; foreign ESP32 cannot decrypt.
- [ ] Original device running attacker-controlled code is **not claimed secure** (HMAC_UP outputs derived material to software). Evaluate Secure Boot/Flash Encryption separately; do not activate either without migration and Web Flasher review.
- [ ] The UI clearly distinguishes: `no backup`, `bond not established`, `backup stored`, `backup authenticated`, `restored`, and `radio forgot pairing`.
- [ ] No confusion between backup of original Bluetooth PIN and authenticated bond material.
- [ ] Recovery after a chip replacement is not promised; hardware-tied backup is deliberately non-portable.

## Evidence template (only sanitized results)

| Check | Date and firmware build | Pass/fail | Sanitized observation / CI link |
| --- | --- | --- | --- |
| eFuse inventory (read-only) | Pending | Not run | No raw eFuse dump or MAC |
| Exact NimBLE bond record API | Pending | Not run | List public API names, version |
| Crypto golden test vectors | Pending | Not run | Published vector uses dummy key only |
| SD corruption/power loss | Pending | Not run | No secrets |
| Original hardware erase/recover | Pending | Not run | Test board first |
| Different ESP32-S3 rejects backup | Pending | Not run | Dummy test device |
| BLE reconnection without PIN | Pending | Not run | Only with radio retaining bond |
