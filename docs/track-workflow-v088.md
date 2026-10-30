# Recording workflow and map completion — v0.8.8

## Device workflow

Record uses compact two-column controls and shows only the recording state,
point count, distance and mode. Internal storage success messages and library
counts no longer occupy this screen. Start is disabled during recording/pausing;
Pause and Resume follow the actual worker state. Pending commands disable the
controls while the device responds.

Start opens a setup page to choose a saved recording preset and track color.
Choose **Custom / this track** to set distance and interval for this recording
without changing a saved preset. Confirm **Record** to create the new track.

Finish pauses sampling and opens a name field. **Save** finishes and exports the
track using that name. **Use Default** uses the generated `Track <ID>` name.
Leaving this naming page via the navigation rails leaves the recording paused;
use Resume or Finish when returning. No unnamed prompt forces the user to discard
recorded points.

Track Details provides a compact visibility switch, palette colors, custom RGB,
rename, confirmed deletion, centre and GPX export. A currently recording or paused
track must be finished before deletion. The local web page also offers rename and
delete. Names are limited to 39 UTF-8 bytes and cannot contain control characters
or the command separator `|`.

## Storage compatibility

Existing journals and GPX files remain readable. Human-readable names are stored
in checksummed `.name` sidecars with replacement/backup handling. GPX `<name>` is
escaped and updated after renaming. File IDs and download URLs remain stable.

Deletion first writes and flushes an ID-specific `.del` marker, then removes the
journal, GPX and name files. The catalog ignores committed deletion markers on
restart, so an interrupted cleanup cannot resurrect a deleted track. IDs are not
reused. Preferences and map overlays are updated by the same storage worker.

## Map loading after a drag

A completed movement is remembered independently of the release coordinate.
Ready map frames remain queued while a drag is active. After release the latest
viewport is requested; a displaced preview is retried if its replacement has not
completed. The map shows loading or offline/missing-tile feedback. Offline mode,
SD availability, provider backoff and the existing visible-tile download limits
are preserved; missing offline tiles cannot be generated without map data.

## Validation

Tests cover actual LVGL control states and compact bounds, setup payloads,
finish/name/default actions, rename/delete commands, a frame arriving mid-drag,
GPX name escaping, independent custom sampling, reboot persistence and deletion
markers. Physical Wi-Fi map downloads and touchscreen usability still require
validation on the device.
