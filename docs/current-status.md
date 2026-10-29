# MeshCrossway — current project status

**Snapshot:** 2026-10-28 (America/Denver; some GitHub events are dated 2026-10-29 UTC).  
**Source of truth:** `main` for integrated firmware, GitHub PRs for unmerged candidates, and owner hardware observations for tested releases.  
**Hardware target:** Waveshare ESP32-S3-Touch-LCD-3.5 **Rev2.0 non-B**, ESP-IDF 5.3.2, LVGL 8.4.0. Firmware version on `main`: **v0.7.6**.

> A successful CI build is not physical validation. Features in an open PR are not part of `main`, the Web Flasher, or a tested release unless separately merged and published.

## Release and validation matrix

| Branch/version | Verified scope | Deployment status |
| --- | --- | --- |
| **v0.6.8** | Last **owner-confirmed physical baseline**: Waveshare touchscreen, Wi-Fi and PIN-free manual **read-only** screenshots | Keep as **hardware-tested default/rollback** in the Web Flasher |
| **v0.7.0** | MeshCore Companion client, real contacts/channels/chats, bounded command/response handling and typed 59-opcode tools | Integrated in later experimental versions; no blanket hardware certification |
| **v0.7.1** | Reuse BLE bonding keys stored in NVS after authenticated pairing; do not store six-digit PIN as plaintext | Experimental functionality |
| **v0.7.2** | Up to five known Wi-Fi profiles and auto-reconnect to the **last single bonded BLE radio** | Experimental; **not** dual-radio auto-reconnect |
| **v0.7.3–v0.7.4** | Color emoji presentation and checkered-flag glyph (93 total in v0.7.4); UTF-8 transport untouched | Integrated experimental functionality; unsupported sequences still exist |
| **v0.7.5** | Read-only physical eFuse/HMAC_UP block inventory in More > Diagnostics | **Vault not active**; no key has been programmed by this feature |
| **v0.7.6** | Official mountain/wordmark rendering and contextual Home/Nodes/Groups/Chats/Map/Settings UI refinements | **Latest on main**, experimental, physical acceptance pending |
| **v0.7.7** | Edge-swipe rails, persistent left navigation in Settings, matched rail icon palette, compact Wi-Fi + MT/MC BLE indicators, AXP2101 battery readout, sender-colored chat/timestamp/hops, 14 px emoji presentation, MeshCore Companion response compatibility | **PR [#54](https://github.com/Renbrant/MeshCrossway/pull/54) OPEN** — candidate only, not yet in `main`; requires CI and Waveshare testing |

**v0.7.6 versus proposed v0.7.7:** the v0.7.6 Settings/Radio panels intentionally omit the left rail. The always-present global left rail in Settings and Radios, no top-bar IP, six sender colors, hours/hops, swipes, battery icon and proportional emojis are **PR #54 behavior**, not claims about firmware already merged.

## What actually communicates

- **MeshCore (MC):** a dedicated BLE Companion adapter handles authenticated connections and radio-specific contacts, channels, private/channel chats and tools. Responses and confirmations must be correlated with the correct pending command; BLE diagnostics do not prove that all companion firmware variants work.
- **Meshtastic (MT):** the present BLE support does **not** implement production protobuf PhoneAPI messaging. Connection/security discovery is not a Meshtastic application-session feature.
- **One BLE session in current firmware:** the intended architecture is one Core plus two independently powered detachable LoRa Pods (MX-M and MX-C), each with its own BLE link. **Simultaneous connected MX-M + MX-C sessions are future work**, tracked in [issue #38](https://github.com/Renbrant/MeshCrossway/issues/38). UI MT/MC separation does not mean the current BLE driver supports two active connections.
- **Strict network isolation:** no automatic bridge, relay, synchronization, identity merging, payload conversion or cross-send between Meshtastic and MeshCore. A unified UI is presentation only.
- **Temporary T-Deck test radio:** any T-Deck used to test the MeshCore side must actually run compatible **MeshCore BLE Companion** firmware. PR #54 expands parser support for *documented* response variants; the precise cause of the user's earlier malformed/unsupported response has **not** been physically confirmed.

## Persistence, privacy and security

- **Wi-Fi:** credentials / up to five known networks and last bonded BLE identity are stored in ESP32 NVS. The former FAT32 microSD Wi-Fi backup, where enabled, contains credentials **in plaintext**. Do not confuse it with an encrypted message vault.
- **Messages:** current history is session/RAM-bound; reopening pages, reconnecting or restarting must **not** be described as durable history. Local encrypted history is **not working** and is not safe to assume.
- **eFuse:** v0.7.5 only reads metadata; no eFuse key burn and no master key provisioned. Any irreversible eFuse operation needs a separate audited maintenance workflow and explicit owner consent.
- **Storage work PAUSED:** [issue #46](https://github.com/Renbrant/MeshCrossway/issues/46) and [issue #47](https://github.com/Renbrant/MeshCrossway/issues/47) specify device-bound AES-256-GCM/HMAC design. [PR #50](https://github.com/Renbrant/MeshCrossway/pull/50) (isolated crypto) and [PR #52](https://github.com/Renbrant/MeshCrossway/pull/52) (unfinished journal) are **open drafts, unmerged, untested for use and explicitly paused until ChatGPT Work is available and the owner authorizes resumption**. Do not publish, install or activate them.
- **Manual remote screenshots:** PIN-free, read-only, over **unauthenticated/unencrypted local HTTP**. Use only on a trusted LAN; screen contents may expose private messages. Remote touch/control is **disabled**.
- **Firmware updates:** keep the Web Flasher **Erase** checkbox unchecked for ordinary upgrades; erasing can discard NVS saved networks and bonding keys. A compatible backed-up Wi-Fi file can restore only its own supported data; it is not a vault backup.

## UI and assets

- **Official logo:** use `docs/site/assets/meshcrossway-logo.svg` and the existing banner `docs/images/meshcrossway-modular-hero.jpg`. Do not redraw the wordmark using a generic font.
- **Navigation:** Home, Nodes, Groups, Chats, Map, More. On `main` v0.7.6, compact left navigation retains six icons on regular pages; right contextual actions appear for Chats/Map only. See [v0.7.6 UI contract](contextual-ui-branding-v076.md) and [v0.7.7 unmerged test plan](https://github.com/Renbrant/MeshCrossway/blob/feature/v077-edge-swipe-sidebars/docs/edge-swipe-sidebars-v077.md).
- **Emojis:** the merged atlas contains **93** color sprites (v0.7.4). The **14×14** scaled appearance and visual `$` fallback (instead of unsupported emoji or literal `[emoji]`) are **v0.7.7 PR work**, not a fix already shipped.
- **Web simulator:** `docs/ui-prototype` is a separate concept, not the authoritative layout or a guarantee of hardware functionality. Maps, GNSS, tracks, live location, wanderer, ATAK, Reticulum and most field modes remain planned/demo/research.

## Next work, in priority order

1. Finish review, CI and physical tests for [v0.7.7 PR #54](https://github.com/Renbrant/MeshCrossway/pull/54), especially touch gesture safety, AXP2101 presence/charging, actual chat metadata and T-Deck Companion diagnostics.
2. Build independent MT/MC BLE session managers and Meshtastic PhoneAPI messaging, maintaining hard separation between both LoRa networks.
3. Verify each native radio Pod's BLE, RF, power and automatic reconnection without disabling native operation; test simultaneous connections and Wi-Fi/LVGL/PSRAM coexistence.
4. Build real GNSS, offline maps, tracks and selected trail-library integration in subsequent separately reviewed increments.
5. **Do not resume encrypted microSD journal implementation** until authorized for ChatGPT Work; then review security and actual MCU CPU/RAM/SD budgets before any provisioning or release.
6. Revalidate Web Flasher catalog, manifest hashes and hardware-tested default before marking any new release stable.

See [firmware historical log](firmware-status.md), [publication policy](web-flasher-release-policy.md), [approved modular architecture](modular-architecture.md), [MeshCore client](meshcore-v070.md), [automatic reconnect](auto-reconnect-v072.md) and [current source code](https://github.com/Renbrant/MeshCrossway).
