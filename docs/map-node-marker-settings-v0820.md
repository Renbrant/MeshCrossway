# Map node marker settings — v0.8.20

Map options now let the user show or hide saved radio nodes, optionally show their names, and select circle, square or cross markers. The settings apply to both Meshtastic and MeshCore and are saved with the existing map preferences. Radio colors remain distinct. Labels are off by default to keep the map clear.

All firmware switches use a compact 44×24 size. The remote companion page constrains checkboxes to 18×18, so they no longer inherit the padding or width of text fields. The track preview background and device-map visibility controls use the same checkbox treatment.

The map preference storage keeps its previous byte layout; old settings continue to load with names hidden and circle icons. v0.8.17 remains the recommended stable release; v0.8.20 is a candidate pending device confirmation.
