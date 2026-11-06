# Map UI candidate — v0.9.30

v0.9.30 packages the map UI changes merged in PR #151 as a separately selectable
experimental candidate. The recommended stable image remains v0.9.28 until this
candidate is verified on the device.

## Changes

- Node reception age is displayed as hours, minutes and seconds.
- Opening one node's details hides the right-side map action rail.
- Opening Map Options keeps the map rail visible and highlights Map Options.
- Zoom and center GPS are compact controls over the map instead of rail actions.

## Device checks

- [ ] Confirm v0.9.30 appears as the latest experimental build in the Web Flasher.
- [ ] Open a node and confirm the reception age is readable in hours, minutes and seconds.
- [ ] Confirm the right-side map actions disappear while node details are open and return after closing them.
- [ ] Open Map Options and confirm the active option is highlighted in the map rail.
- [ ] Use both map controls and confirm zoom and center behavior still work.
- [ ] Check that controls remain inside the map at both compact and expanded navigation widths.
- [ ] Confirm the recommended stable image remains v0.9.28.
