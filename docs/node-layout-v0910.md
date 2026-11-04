# v0.9.10 radio controls and node-list stability

v0.9.10 is an experimental firmware candidate. The user-selected stable release remains v0.8.26.

## Radio controls

The device Settings view now has explicit Wi-Fi and Bluetooth on/off controls. The BLE / Radio Pods page also exposes Bluetooth off while a connection is still starting, so a stalled startup can be stopped from the touchscreen. Disabling Wi-Fi stops the station and access point; remote access is unavailable until Wi-Fi is turned back on. The saved station profile is reused when Wi-Fi is re-enabled.

Bluetooth shutdown is serialized through the BLE coordinator: it disconnects both independent radio links, stops the NimBLE host, deinitializes the host stack and disables the controller. An explicit off choice is retained across reboot. If the NimBLE host task never starts, shutdown is refused and the UI reports the failure instead of deinitializing a stack that may still be starting.

## Nodes view

The node list uses a dedicated vertical scroll viewport below a fixed heading and controls. Redundant full-list redraws during synchronization are suppressed to reduce work on the LVGL task, addressing watchdog events observed while entering Nodes.

## Validation status

This candidate requires build and device validation. v0.8.26 remains the recommended stable image until the new Bluetooth lifecycle and node-list behavior are confirmed on hardware.
