# Map fallback centering — v0.8.25

The map previously waited for a current local GPS fix before requesting a map frame. Without that fix, the background and radio-node markers remained unavailable even when a connected radio already had stored node coordinates.

When local GPS has a valid fix, the map continues to open centered on the device and follow GPS. Before a fix is available, it now opens on a positioned Meshtastic node, or on a positioned MeshCore contact if Meshtastic has none. The Center GPS action remains available to move the map to the device after a fix arrives. Existing node visibility preferences still control whether node markers are drawn.

Map tiles still require the configured online provider or compatible tiles on the SD card. This change makes the map view and positioned-node overlays available without waiting for a local GPS fix; it does not create missing map tiles or radio positions.
