# MeshCrossway Android 0.4.7 beta

Tap the GPS button on the map to follow your phone continuously. The selected
button stays highlighted. Further position updates move the map while preserving
its zoom and rotation. A map gesture pauses following; tap GPS to resume. Fit
nodes, selecting a place, and choosing an offline download area also pause it.

While moving, your blue position dot becomes an arrow pointing in the direction
of travel, including on a rotated map. It returns to a dot when stopped, when the
fix is old, or when motion/direction is uncertain. This uses GPS course rather than
phone orientation. If speed/course are absent, sufficiently clear displacement
between accurate fixes can establish a direction. Small position drift is ignored.

Foreground phone location requests now use a one-second interval and no minimum
distance, allowing slow movement and stops to update the map. Android/provider
availability still controls actual delivery. The map timer stops when paused.

Install over the current app **without uninstalling**. Version code 41 uses the
existing permanent signing key. No database migration or radio protocol changes.
Field testing on a moving phone remains pending.
