# Track manager — v0.8.5

The Tracks rail opens the saved and currently recording tracks. Select an entry
for its visibility switch, color, or centre-on-track control. Up to eight tracks
can be visible concurrently. Starting a new recording preserves the other visible
tracks; recording and map selection are independent. Recording controls remain
in the centre column with both navigation rails accessible.

## Recording presets

Four editable preset slots are stored on the microSD:

| Default | Minimum distance | Minimum interval |
| --- | --- | --- |
| Walking | 3 m | 2 s |
| Cycling | 8 m | 3 s |
| Car | 20 m | 5 s |
| Custom | 5 m | 2 s |

Both spacing and interval must be satisfied. Elapsed time never forces another
stationary point. When GNSS supplies speed, positions below 0.8 km/h are filtered
between normal points. A first fix after a recording pause or loss of fix starts
a new segment. This filter reduces drift; it does not guarantee perfect detection
of stationary receivers in poor GNSS conditions. Very slow movement may be
filtered. Preset changes apply during recording as well as to future recordings.

Select a preset in Record. Use the device's local HTTP page to rename or edit any
of the four slots (3–1000 metres, 1–300 seconds; ASCII names up to 23 characters).

## Local web management

Open the same device IP used for manual screen captures. The new sections list
tracks, their visibility and colors, and provide recording controls, GPX export
and GPX downloads. Finish a recording to generate its GPX. Export can regenerate
GPX for a completed track. Download saves that file on the computer. There is no
remote touch or radio command integration.

Commands are queued for the navigation storage worker; the HTTP handler never
calls LVGL. A 202 response means queued, not committed. Check the device status
and refresh the list to confirm the resulting settings. Browser writes require a
custom same-origin request header; the server emits no CORS permissions. Access
is intended for the user's trusted LAN, as with the existing capture page.

## Storage and bounds

Existing `.trk`, `.poi` and GPX formats are preserved. Preferences use a separate
checksummed file and are saved with a flushed replacement transaction. The
catalog retains its existing limit of 32 recent tracks. Map rendering uses bounded
previews (128 points per saved track, 256 for the active recording), while the
journal and downloadable GPX retain every recorded point. Overlay loading reads
at most 128 samples per visible saved track, keeping worker I/O bounded.

Validation covers stationary sample suppression, concurrent saved overlays,
visibility and color persistence, editable presets, navigation command routing,
HTTP command validation and GPX generation. Physical GPS motion and SD tests are
still required on the Waveshare before declaring this release hardware verified.
