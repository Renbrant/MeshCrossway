# v0.9.1 settings navigation candidate

**Status:** experimental candidate. v0.8.26 remains the recommended stable firmware.

This revision improves the WadaMesh-inspired category menus introduced in v0.9.0:

- Uses compact rows so more settings and categories fit on screen.
- Adds clear Back navigation on the device, with Close kept as a separate action.
- Returns from a radio setting or form to its previous category page instead of closing the settings flow.
- Makes remote settings behave as nested pages; the selected form replaces the category list until the user goes Back.
- Keeps Meshtastic and MeshCore settings and categories separate.

The release needs normal firmware build and physical-device validation before anyone selects it as stable. The exact v0.8.26 image remains unchanged as the recommended stable release.
