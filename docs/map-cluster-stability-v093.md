# v0.9.3 — Map cluster stability

The device rebooted when opening a nearby-node cluster on the map. The device log reported `Stack canary watchpoint triggered (taskLVGL)`, indicating that the LVGL task stack was exhausted.

The cluster details list used a 5,400-byte local array in the LVGL task. The array is now static storage, removing that large temporary allocation from the limited UI stack. The list is cleared before reuse, and LVGL copies the label text while it is set.

This release is experimental. v0.8.26 remains the recommended stable release. The patch has not yet been validated on the physical device.
