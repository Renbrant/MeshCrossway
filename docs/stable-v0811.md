# Stable firmware v0.8.11

On 2026-10-31 the owner requested v0.8.11 as the stable website release after v0.8.12 repeatedly restarted. The attached device log repeatedly reports `Stack canary watchpoint triggered (mcx_ble_resume)` and subsequent reboot. This identifies the crashing task; the underlying call-chain cause is not yet repaired.

The Web Flasher defaults to v0.8.11. v0.8.12 is marked known-issue and installation is blocked. Legacy installer URLs also use v0.8.11. Install without Erase to retain settings.

Exact approved image, never rebuilt:

- Compiled source: `411c8a3c7b9da6d5703f2f8c1cffa7dde9584c5c`
- Merged feature: PR #78
- Binary SHA-256: `dbbdf7ed69b04f134517566053cca9eccca80c0d6be5ffc0f4ef53bc636c81b8`
- Binary size: 2069376 bytes
- Reviewed archive: Actions run `41132077599`, artifact `13014125812`
- Archive SHA-256: `d5eee7cdf4c614494edab3ebfd6c9025c61166840bd7693ff44d3c28e9f87b87`

The previous v0.8.9 immutable record remains in docs/stable-v089.md. Firmware source remains at v0.8.12 pending a separate crash fix; this change promotes the tested binary without changing device code.
