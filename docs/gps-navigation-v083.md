# v0.8.3 — local tracks, POIs and map controls

## Confirmed recovery

On 29 October 2026 the owner confirmed v0.8.2 GPS/map operation and MeshCore BLE
connection after fresh pairing, then reconnection after restarting the hardware.
It is now the recommended recovery. The original published binary is retained:

- compiled source: `72abc3bed5353319f6c32a15781d0e419b6937d7`
- SHA-256: `60aa1e431ffaf5270130608a6ad401e017b1b0f3822933e36485c7066790256c`

This is owner acceptance of the observed functions, not certification of all
Meshtastic features, two simultaneous radios or long-term reliability. Existing
bonds must be preserved during normal updates; install without Erase.

## Right rail

Order: Map View, Zoom, Center GPS, Record, Tracks, POIs, Map Details, GNSS,
Map Options, then the existing Live Share and Team previews. The first seven fit
in the 286-pixel-high rail. Expanded buttons use the same icon-above-caption
layout and dark/orange/white palette as the left rail, with no orange outline.
Collapsed buttons retain working icons. Native drawn icons include a folded map,
magnifier, crosshair, recording circle, path, location pin, info circle,
satellite and settings sliders. No new raster/font assets are needed.

## Record and review a track

Open Record and choose START, PAUSE, RESUME or FINISH. The asynchronous worker
records fresh valid GPS fixes to the SD card; the UI never performs file I/O.
A compact REC/PAUSED/SD ERR indicator appears on the map. Waiting for a fix does
not create invented positions. Sampling uses a minimum two-second interval;
stationary fixes are retained at fifteen-second intervals or after three metres
of movement. This is a practical logging policy, not a claim of GPS accuracy.

Pause/resume and missing/stale fixes create separate segments, so gaps are not
joined as if the user travelled in a straight line. Distance sums only contiguous
samples in the same segment. UTC comes from a valid RMC date/time, otherwise from
the synchronized system clock; absent trustworthy time is omitted from GPX.

Files live under `/meshcrossway/navigation/` on the SD:

- `00000001.trk`, etc.: versioned binary track journal, header and per-record
  checksum; flush/fsync after each accepted point; exclusive-create IDs.
- `00000001.gpx`, etc.: GPX 1.1 export produced at FINISH or recovery.
- `00000001.poi`, etc.: checked POI records with exclusive IDs.
- `pois.gpx`: waypoint export updated after POI changes and reload.

On restart, valid journal prefixes are recovered and missing GPX exports generated.
Recording does **not** restart automatically. A partial or invalid tail is ignored
without appending across the damage. Original journals are retained. GPX exports
are replaceable derivatives; temporary files and backup rotation accommodate FAT
rename limitations, but FAT/fsync cannot guarantee survival of arbitrary card
failure or power loss. No card is formatted and no message/configuration file is
modified by navigation storage. Text GPX files and navigation records are not
encrypted, matching the currently approved plaintext SD approach.

Tracks lists the latest 32 journal IDs and opens a selected track on the map.
The overlay is a bounded, decimated preview of at most 256 points; the journal and
GPX retain every committed sample. End an active recording before opening another
saved track. Each journal is limited to 16 MiB; new recording/POI writes require
more than 8 MiB free. Errors stop or pause recording visibly and retain the saved
prefix. No automatic file deletion or unlimited in-memory track archive exists.

## POIs

POIs offers SAVE CURRENT GPS AS POI. Enter a name and SAVE while the receiver has
a fresh fix. The position is captured at SAVE, not at the later SD write. Names
are at most 39 UTF-8 bytes; XML reserved characters are escaped in GPX. Saved POIs
survive restart, appear as map markers, and offer SHOW ON MAP or an explicitly
confirmed DELETE. The current local index supports 64 POIs. GPX export is available
on the SD without an online account. Naming existing tracks/POIs and GPX import
are not implemented in this increment.

## Map Options and GNSS

Select Street / OpenStreetMap or Topographic / OpenTopoMap. The online switch
controls retrieval of visible missing tiles; OFF uses existing local files/cache.
There is no editable source URL or attribution field. Presets apply their own
mandatory credits automatically, with read-only full credits available on tap.
Street zoom is 3–19, topographic zoom 3–17. Provider changes are saved in NVS and
isolate persistent caches. Topographic local tiles use `/tiles/opentopo/z/x/y.png`;
legacy `/tiles/z/x/y.png` packs are treated as the street/OSM layer and must have
matching provenance. Use local packs only when their source permits offline use.

Online requests remain interactive only, with cache retention, a distinct
User-Agent and no bulk download/prefetch. Public community servers have no SLA.
OSM data attribution, SRTM and OpenTopoMap style credits are visibly retained.
The GNSS view adds altitude, speed, fix age, UART byte/error counts, valid/invalid
NMEA counters and receiver UTC when available. Existing GPS settings remain below.

Provider references checked during implementation:
- https://operations.osmfoundation.org/policies/tiles/
- https://osmfoundation.org/wiki/Licence/Attribution_Guidelines
- https://www.opentopomap.org/about (raster URL, SRTM/OSM attribution and CC-BY-SA)

## Tests and acceptance

Host tests cover real LVGL slider/map controls, exact rail order and borderless
styles in both modes, native icons, provider selection/online switch, record and
POI UI actions, journal sampling/segments, reboot and torn-tail recovery, GPX/XML
escaping, POI persistence/deletion and UTC/date validation. Existing BLE, touch,
protocol and SD-history regressions remain required. Full ESP-IDF compilation and
immutable recovery checks must pass before publication.

Hardware acceptance remains pending: an outdoor route with pauses/fix loss,
POI creation/show/delete, restart/recovery, SD missing/full/write failure, real
OpenTopoMap TLS/tiles, and responsive BLE/touch/Wi-Fi during recording. Monitor
internal RAM and the new 4096-byte navigation-worker stack; bulk state is in
PSRAM. Routes and POIs never transmit automatically on either mesh network.
No bridge, relay, conversion, cross-send or automatic GPX sharing is introduced.
