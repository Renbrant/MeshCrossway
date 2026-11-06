# Map controls and navigation state — v0.9.31

This experimental candidate fixes the map controls and action highlighting reported on v0.9.30. The recommended stable firmware remains v0.9.28 until this candidate is checked on the device.

## Changes

- Zoom and Center GPS controls are created above the map image and labels, so refreshed tiles cannot cover them after a pan.
- Exactly one right-side map action reflects the current screen. Map Options, Record, Tracks, POIs, map diagnostics and GNSS status no longer leave Map View highlighted at the same time.
- Closing a map subpage restores the action associated with the current map view.

## Device checks

- [ ] Confirm v0.9.31 appears as the latest experimental build in the Web Flasher; v0.9.28 remains recommended stable.
- [ ] Drag the map in several directions, release, and confirm Zoom and Center GPS remain visible and tappable after tiles refresh.
- [ ] Open Map Options and confirm it is highlighted while Map View is not.
- [ ] Open Record, Tracks and POIs separately and confirm each active screen highlights its own icon only.
- [ ] Open GNSS Status and confirm it remains highlighted correctly.
- [ ] Close each subpage and confirm the selected map action returns to Map View.
