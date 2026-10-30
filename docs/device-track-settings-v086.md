# Device track settings — v0.8.6

The touchscreen now exposes the track-manager settings independently of the
local web interface. Both interfaces submit the same storage-worker commands
and use the same SD preferences. No Wi-Fi or browser is needed for these controls.

## Presets on the touchscreen

Open **Map → Record → Edit Recording Presets**. Choose one of the four preset
slots. Tap Name, Distance, or Interval to edit it with the on-screen keyboard.
**Apply** updates the draft. **Save and Use Preset** writes it to the SD and
selects it for recording, including an ongoing recording. Use a slot such as
Custom to create your own preset. The limits match the remote interface:
23 ASCII characters, 3–1000 metres, and 1–300 seconds.

Use the Record preset selector to switch between saved presets without editing.
The existing stationary filtering is shared by all presets.

## Tracks on the touchscreen

Open **Map → Tracks** and choose a track:

- Show or hide it on the map, independently of the recording in progress.
- Choose a palette color or **Custom RGB Color** (six hexadecimal digits).
- Centre the map on it.
- **Export GPX to SD** for a completed track. Finish an active recording first.

Finishing a recording also generates its GPX. GPX files stay on the microSD;
computer downloads remain available on the optional web interface.

The eight-visible-track limit and existing journal/GPX formats are unchanged.
The broader graphical redesign of the remote page is deferred.
