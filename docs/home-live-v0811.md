# Live Home cards — v0.8.11 candidate

## Home

- **Meshtastic / MeshCore:** actual Bluetooth phase, including startup, scanning, connecting, protocol verification, PIN pairing, native synchronization, history restore, connected, disconnected and failure. During discovery the selected peer's advertised protocol is used until GATT identifies it; unknown peers are not silently attributed to either network.
- **Counts:** locally known nodes and incoming unread messages for each independent native model. Meshtastic uses NodeDB count (including the local node if present); MeshCore uses its known contact/node records. These are not estimates of the total mesh size. Outgoing and already-read messages are excluded. Retained numbers from an inactive radio are labeled `(saved)`; an uninitialized empty model shows `--` instead of invented zeroes.
- **Wi-Fi:** replaces the Crossway card with the selected station SSID and actual connection state. The snapshot copies only SSID/state under the existing Wi-Fi lock; it never reads or exposes a password. SSIDs are bounded to 32 bytes and long names are ellipsized within two lines.
- **GPS:** no coordinates on Home. Shows `FIX GNSS`, differential fix, searching/no fix, disabled/error, or native RTK FIX/FLOAT only when reported by a fresh GGA quality field. Coordinates remain available in GNSS details and the map.

The MAX-M10S is a standard-precision receiver: a normal autonomous fix is not an RTK fixed solution, and acquiring satellites is not RTK FLOAT. User confirmation that v0.8.10 works well is recorded on 2026-10-30; the explicitly designated v0.8.9 reference image remains unchanged in this candidate.

## Horizontal accuracy estimate

The UART worker requests `$PUBX,00*33\r\n` at most once every two seconds, after incoming data and baud settling. This is a **measurement poll**, not a configuration command. It does not change message rates, baud, GNSS settings or flash/NVS on the receiver. It requires **display TX43 to GPS RX**, in addition to GPS TX to display RX44 and common ground. If only receive wiring is present or the receiver does not answer, normal GPS reception still works and accuracy is shown as unavailable.

- A valid PUBX,00 solution provides `hAcc` directly in metres. Home labels it `Est. accuracy`, converting to feet when selected by existing GPS unit settings.
- If NMEA GST already supplies fresh statistics, extra polls are suppressed. The optional fallback combines latitude and longitude error standard deviations as `sqrt(stdLat² + stdLong²)` and explicitly labels this **estimated horizontal RMS** (`Est. hRMS`). This is not the same statistical quantity as a guaranteed circular error bound or a 95% confidence radius.
- HDOP is not multiplied by an arbitrary factor to fabricate metres. Invalid, zero, negative, non-finite, absent or stale accuracy is unavailable. Accuracy/quality timestamps expire independently; a measurement-only PUBX/GST reply cannot renew an old position or clock. No estimate is displayed without a fresh valid position.

Primary reference: [u-blox M10 SPG 5.10 interface description, UBX-21035062 R03](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf), sections 2.8.2 (PUBX position poll/output), 2.7 GST and NMEA position-validity/quality fields. Original parser implementation; no upstream library or schema copied, and no new dependency.

## Validation

The pure Home model and actual LVGL adapter/card code are tested against native MC and MT message models. Checks cover selected-peer fallback during connection, independent counts, read/outgoing filtering, saved vs unknown counts, GNSS/RTK distinction, metres/feet, long names, card geometry at expanded/compact rail widths, safe page changes and **zero new display flushes on repeated idle updates**. Existing map, message, navigation and touch suites remain in use.

Production UART/parser tests cover PUBX hAcc, GST statistics, checksum/format/numeric bounds, loss of fix, independent expiry, disabled/error gates, polling rate/settling and GST suppression. The new UART poll still requires physical validation of TX wiring and the module response. Full ESP-IDF CI and real-device Home validation are required before promotion.
