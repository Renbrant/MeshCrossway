# MeshCrossway Android 0.4.8 beta

Tap GPS on the map to follow your phone as you move. The button remains highlighted;
map gestures pause following, and tapping GPS resumes it. Your chosen zoom and map
rotation are preserved. Camera padding now explicitly follows the visible map area
after opening or closing map panels.

The position dot becomes a blue arrow when fresh GPS data establishes movement and
a reliable direction of travel. It returns to a dot when stopped or when course is
uncertain or stale. Small GPS drift is ignored; phone orientation does not steer
the arrow. The arrow is drawn as vector geometry, avoiding an invisible custom-icon
rendering problem found during native-image testing.

Includes the 0.4.7 map changes and a stricter visual regression: native snapshots
must show the phone marker's own blue, rather than a nearby MeshCore node. Snapshot
collection waits for the rendered frame and verifies exported PNG files.

Install over the current app **without uninstalling**. Version code 42, same
permanent signing certificate and data format. Field testing remains pending.
