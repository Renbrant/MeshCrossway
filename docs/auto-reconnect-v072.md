# v0.7.2 — Automatic Wi-Fi and last BLE radio reconnection (experimental)

## User behavior

- On every boot, Wi-Fi AP+STA initializes normally. If previously connected Wi-Fi profiles exist, the Core first tries the **most recently connected successfully**, and after two reconnect retries tries up to four other saved networks. If all are unavailable, it waits 60 seconds then retries, leaving the MeshCrossway-Dev recovery AP and touchscreen functional.
- Once a Wi-Fi connection obtains an IP address, the successful SSID/password pair is added to a five-entry NVS profile list (most recently used first). Merely *attempting* an incorrect password does not permanently replace the remembered known networks. Existing single STA NVS configuration and previous removable SD Wi-Fi backup are initial migration/recovery sources.
- When selecting an already known SSID manually in Settings > Wi-Fi, the password may be left empty. Selecting a **new** SSID still requires its password. Credentials are not printed to serial or exposed by HTTP.
- **Twelve seconds after normal boot**, a nonblocking worker checks for the most recently bonded BLE peer in the Core NVS. If none exists, BLE stays disabled. If one exists, it initializes NimBLE in its existing worker, scans for the saved address and connects to that **identity only**, using the existing MITM-authenticated stored bond.
- The BLE reconnect worker scans again at a bounded 30-second interval while offline. The last BLE address/protocol is saved only when a bond is authenticated and the GATT link reaches READY. An intentional manual **Disconnect** pauses automatic scans until the next boot or another successful new pairing.
- If a peripheral advertises a rotating privacy address that the host does not resolve to its saved identity, auto-reconnection may not find it. Never connect to a device solely by its advertising name. Retest manually and investigate privacy/address resolution.
- Current BLE implementation has **only one active session**, and Meshtastic GATT is still discovery/verified-security only. Automatic simultaneous reconnection of independent MX-M and MX-C Pods awaits the multi-session adapter. No relay, bridge or message conversion is added.

## Technical boundaries

- Source: `firmware/display-prototype/meshcrossway_network.h`: a versioned `mcx_wifi/profiles` NVS blob of at most 5 SSID/password pairs. Last successful connect is moved to the front; runtime event handler switches credentials without blocking LVGL. The existing `meshcrossway_backup.h` plaintext Wi-Fi SD backup remains unchanged.
- Source: `firmware/display-prototype/meshcrossway_ble.h`: `mcx_ble/last_peer` NVS blob of 1 verified security identity and protocol. Existing NimBLE bonding key storage (v0.7.1) remains separate. PIN is **not** cached in plaintext.
- Source: `firmware/display-prototype/apply_to_waveshare.py`: after Wi-Fi+HTTP initialization, schedules (but does not synchronously perform) delayed BLE startup. The working LCD DMA buffers and Wi-Fi/AP boot sequence remain unchanged.
- ESP-IDF firmware: v5.3.2. NVS is **not encrypted at rest** in this experimental build; a separate [device-bound encrypted microSD backup proposal](https://github.com/Renbrant/MeshCrossway/pull/42) is design only and is not implemented by auto-reconnect.
- This implementation does not restore erased NVS without a separately secured backup. **Leave Erase unchecked** during normal Web Flasher updates. No eFuse or bootloader changes.
- Must remain labeled EXPERIMENTAL until tested on the physical Waveshare Rev2.0 with the real BLE Companion and Wi-Fi routers. The immutable v0.6.8 rollback remains available.

## Test plan

1. Flash without Erase. Check boot touch, Wi-Fi, local IP, web manual screenshot and that the recoverable AP still appears.
2. Connect to Wi-Fi A, then Wi-Fi B. Reboot with both available: last successful B should be preferred; turn off B and reboot: A should connect automatically.
3. Power-cycle with both networks unavailable: keep AP/UI responsive and retry without excessive rate. Provide a network after a delay and verify recovery.
4. Enter incorrect password for another network: existing A/B profiles must survive. Selecting A with an empty password must work because A is already saved.
5. Bond to MeshCore radio (six-digit PIN initially), confirm bond and working GATT session. Reboot with radio in range; verify delayed automatic secure reconnect without PIN and without outgoing replayed messages.
6. Power off the radio, reboot Core; verify bounded scan duty cycle and reconnection upon return. Manually select Disconnect; verify retries stop until the next boot.
7. Test a second radio with a matching advertised name but **different identity**: it must never be automatically selected.
8. Test random-address rotation / RPA with NimBLE identity resolution. If unresolved, log nonsecret diagnostics and use manual reconnection; do not weaken identity matching.
9. Stress repeated Wi-Fi/BLE failures, SD reads, screen captures and LVGL touch; verify no watchdog, SPI queue or memory regression.
10. Confirm the 0.6.8 image and NVS erase-option prompt remain intact. No change to Wi-Fi plaintext backup or the experimental encrypted-SD design.

### Known limitations / change control

Five is a deliberate bounded NVS capacity, **not** an unlimited Wi-Fi credential vault. The last connected BLE peer is a single target until simultaneous multi-radio connections are implemented. The device still requires NVS and peer-side BLE bonding keys to persist. Do not interpret compile success as hardware validation. The future security release is tracked as version 0.7.3 for encrypted microSD recovery to keep version numbers sequential.
