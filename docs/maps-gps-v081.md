# GPS status, settings and maps — v0.8.1 candidate

## On the device

The top bar shows a small navigation symbol and satellites used. Green means a
fresh valid fix; amber means the reader is enabled without a fresh fix; grey and
`--` mean reading is disabled. This does not claim metre accuracy.

Map displays real georeferenced raster tiles in Web Mercator with the local GPS
marker. Drag to pan (which pauses following), use +/- to zoom from 3 to 19, and
press GPS to resume following. A missing tile is an empty checkerboard, not an
invented terrain image. The marker is hidden when the fix expires. During panning
it is hidden until the new view is established. Other mesh networks never receive
an automatic location packet.

Map Options selects online tile retrieval, the HTTPS XYZ root URL, and the visible
provider attribution. The default free interactive layer is OpenStreetMap.
Only tiles intersecting the displayed viewport are requested, at a bounded rate;
there is no region/country prefetch. HTTP errors back off, redirects are refused,
TLS uses the certificate bundle, and online retrieval waits for clock sync. The
NTP service starts after Wi-Fi is available. Online viewing reveals the viewed
area to the provider; disable online retrieval to use only local files.

Open GPS Settings from GNSS Status or More. Settings are applied and saved by the
reader worker, not by blocking the touch task:

- Reading ON/OFF (does not switch receiver power).
- Host UART baud AUTO (9600/38400), or 9600, 38400, 115200.
- Fix expiry after 3, 5 or 10 seconds without a new position.
- Metric or imperial altitude/speed display.
- Current baud, received bytes, valid/bad NMEA counters, UART error and save status.

The UART remains RX GPIO44 / TX GPIO43, already confirmed by the user's v0.8.0
live-fix photos. Settings change host reading behavior; no UBX configuration or
flash command is sent to the receiver. Choose a manual baud only when the receiver
uses that baud. Changing units or expiry does not reset the UART or discard a fix.
HDOP is a geometry indicator, not an accuracy in metres.

## Offline PNG XYZ packs

Copy unpacked **256×256 PNG** tiles to the microSD, preserving this tree:

```
/tiles/<zoom>/<x>/<y>.png
```

For example `/tiles/14/10000/6000.png` (the coordinates are just a path example,
not a supplied tile pack). Device VFS paths start `/sdcard/tiles/...`.
Only the covered area and zoom levels can render. ZIP, MBTiles, PMTiles, vector
and JPEG archives are not implemented by this reader. There is no pack bundled
with firmware and no browser uploader in this change; copy files with a computer.

Generate PNG XYZ directories with QGIS using a source that permits offline/bulk
use, or use a properly licensed existing pack. In QGIS choose Generate XYZ tiles
(Directory), PNG, 256×256 and standard XYZ Y (not inverted TMS Y). Use a practical
region and zoom range. Set the matching attribution in Map Options.

Do not create offline packs by bulk downloading from the public OSM tile server.
Free map data does not mean unrestricted tile-service bandwidth. The firmware's
online mode uses only the current interactive view and retains its cache; it does
not offer a "download area" action.

Online tiles are cached separately under `/mcx-map-cache/<source-hash>/z/x/y.png`.
A writable microSD is required for online requests so revisits can use persistent
cache. Existing cached tiles are retained without automatic expiry (at least the
OSM minimum retention), and provider changes use different directories. Files
are bounded to 256 KiB, checked as 256×256 PNG, decoded with bounded decompression,
then written through a flushed/synced temporary file. Corrupt app-owned cache may
be replaced after a valid replacement is ready; user offline packs are not deleted.
New downloads stop before consuming the last 8 MiB of reported free space,
leaving a reserve for other device data. There is no cache quota manager or
automatic purge yet: use a suitably sized card and manage space
on a computer. A full/unavailable SD leaves placeholders rather than formatting it.

## Responsiveness and validation

Network access, SD reads and PNG decode run on a low-priority map worker (6144-byte
internal stack). Compressed data, decoded RGBA and display frames use PSRAM via
the existing allocator. At most one worker result waits for the UI; ownership
transfers under a lock. Navigation cancels stale jobs and releases old images
only after LVGL no longer references them. TLS dynamic memory is in PSRAM to
reduce pressure on BLE/DMA internal memory. No LVGL widget operations run in the
map worker. The UI polls completed frames and snapshots, not UART or tile files.

Host tests exercise Mercator/poles/dateline, valid and malformed PNG, real SD
raster composition, mocked HTTPS cache commits/reuse/repair/503 backoff/no-SD
behavior, real LVGL pan and settings/editor geometry, header badge states/spacing,
and the production GPS worker's non-disruptive settings changes. Transport tests
mock HTTP; they do not prove an actual certificate handshake on this board.

Hardware acceptance pending: live OSM view with Wi-Fi, offline packs without Wi-Fi,
SD removal/full/error recovery, touch during downloads, zoom/pan/follow, stale fix,
GPS settings across restart, and BLE traffic with map TLS. Check internal heap,
worker stack margins and touch latency before classifying this as stable. Current
radio protocol limitations remain unchanged. Install without Erase; keep the
owner-tested v0.7.8 recovery intact.

## Public references

- [OSM raster tile service policy](https://operations.osmfoundation.org/policies/tiles/).
- [Standard XYZ offline directory interoperability](https://github.com/ALLFATHER-BV/wadamesh#offline-map-tiles), researched as a format reference; no source copied.
- [ESP-IDF 5.3.2 SNTP service](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32/api-reference/network/esp_netif.html).
- LVGL v8.4.0 `src/extra/libs/png/lodepng.{c,h}` (existing build input).
