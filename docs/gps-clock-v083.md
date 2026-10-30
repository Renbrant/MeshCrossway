# v0.8.3 — GPS clock and compact status bar

The Core accepts checksum-validated, active RMC sentences containing a valid UTC
calendar (2020–2099). It sets the system clock without Wi-Fi and shows local time
as `HH:MM` in the top bar. Before the first valid time sample it shows `--:--`.
GGA positions alone cannot supply a calendar date. Invalid dates, inactive RMC,
bad checksums and stale samples cannot set the clock. Leap-second samples are
skipped until the next ordinary second. This is a status clock, not PPS timing.

The clock continues from a monotonic anchor if GPS reception disappears or GPS
reading is disabled. After a power cycle it waits for GPS time again. The last
resolved timezone is saved, but an old calendar timestamp is never replayed as
current time. System UTC is independent of local timezone display.

## Timezone and daylight saving

Timezone detection runs locally using the compact tz-lookup geographic quadtree;
no coordinates are transmitted, no account is needed, and no SD card or internet
is required. The first valid position selects a zone. Later changes need three
fresh position samples, reducing jumps caused by jitter near a boundary.

This compact database is an **estimate**, not exact administrative polygons. It
can choose the wrong zone near borders and sparsely populated regions. Open
**GPS Settings → CLOCK / TIME ZONE** to inspect the detected IANA zone and choose
any bundled zone manually, or return to **Automatic / GPS location**. The choice
is saved with the existing GPS settings. DST follows the selected zone's IANA
POSIX rules rather than a fixed longitude-based UTC offset. Future government
rule changes require a firmware database update. POSIX rules represent recurring
rules, and cannot guarantee future one-off religious/political exceptions.

The generated table records the tz-lookup source SHA256 and tzdata wheel SHA256.
The initial rule snapshot is IANA 2026f (tzdata 2026.6). To regenerate, supply the
upstream `tz.js` and an official `tzdata` wheel to
`scripts/generate_timezone_data.py`. Rebuilds do not download timezone data.

## Header

The official brand remains visible. Wi-Fi, independent MT/MC Bluetooth states,
GPS satellite count and charge indication share a tighter strip. The battery
body is reduced from 44 × 20 to 36 × 13 pixels; its fill and percentage still use
the real PMU reading. Missing batteries still show `--`.

## Validation

Host tests cover calendar rejection, RMC validity, timezone lookup, Denver spring
and autumn transitions, Arizona without DST, Brazil, fractional offsets,
southern-hemisphere DST, manual selection, signal loss and millisecond rollover.
The production GPS/map UI is exercised with real LVGL 8.4. The firmware preflight
runs the clock tests on every firmware build.

Hardware acceptance: install without Erase, acquire GPS outdoors, compare local
`HH:MM` and date against a reference, inspect the detected zone, select another
zone and return to Auto, disable GPS reading and confirm clock continuation,
reboot and check saved selection plus fresh-time acquisition. Check the header
with 100%, low battery, charging and no battery. Retest BLE and map interaction.
**v0.8.2 remains the exact owner-tested rollback.** v0.8.3 requires physical testing.
