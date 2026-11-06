# v0.9.26: landscape orientation candidate

v0.9.25 remains the recommended hardware-tested stable image. Additional driving and walking testing was reported without freezes or resets. This candidate adds sensor-driven 180-degree landscape orientation; it does not replace that accepted stable image.

## Controls

Open the top-bar quick panel. **AUTO ROTATE** allows the built-in QMI8658 accelerometer to select the landscape edge. Tap it to show **ROTATION LOCKED**, freezing the current orientation. **FLIP 180** manually reverses the image and locks it. Lock and manual orientation survive reboot in a separate, versioned NVS key; existing brightness and timeout settings are unchanged.

On first use, hold the screen upright in its current readable orientation for approximately one second without touching it. That stable gravity direction becomes the saved mounting reference. Subsequent inversions work across reboots. Flat placement, portrait-edge placement, ambiguous gravity and strong acceleration keep the current orientation. Changes require one second of consistent gravity and have a two-second cooldown. No change is applied during an active touch or outstanding LCD flush.

## Resource and failure handling

The existing battery telemetry task samples orientation every 200 ms; battery telemetry retains its 15-second interval. There is no added task, gyro stream, sensor FIFO, framebuffer or image-copy rotation. QMI8658 initialization is a bounded one-time probe at 0x6b/0x6a on the existing I2C bus. Transfers have a 12 ms bound. Missing, failed or stale sensor readings cannot rotate the display; manual flip remains available. Sensor reads pause while locked.

A pair of LCD hardware mirror flags reverses the existing 480x320 landscape image. The LVGL tree, dimensions and draw buffers are retained. The input consumer reverses both touch coordinates in the same change and cancels old queued input. No BSP touch-controller state is modified concurrently with sampling. Rotation diagnostics use the `[MCX-ROTATE]` console prefix. Physical orientation and touch acceptance are still required on the device.

Implementation references: the pinned [Waveshare BSP](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/d9ff7837148b47b33edadc4b87775c341cf35bbe), its QMI8658 register definitions and sensor library, LCD mirror configurations for 90/270 degrees, and the [board documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5).

## Test procedure

1. Install v0.9.26 without erasing settings. Confirm saved Wi-Fi, radios, brightness and timeout.
2. Hold the readable device upright, hands off, for two seconds to establish the initial reference. Reverse it; confirm the image flips after a short delay and all four touch corners, keyboard and map drags match the display. Return it to the original position.
3. Keep a finger down while reversing: the orientation must wait until release. Tap navigation and map tools after the change; no phantom tap should occur.
4. Lock at each orientation, move the device and reboot. Confirm the locked orientation survives. Try manual Flip 180, then re-enable auto rotation.
5. Lay the device flat, hold it on its portrait edge and move it briefly: no oscillation should occur. If sensor reads fail, the current image and manual control must remain usable.
6. Repeat driving/walking, dual-radio reconnects, Wi-Fi hotspot changes and map panning. Record rotation logs, heap and touch diagnostics in issue #137; compare against stable v0.9.25. Return to v0.9.25 if this candidate regresses.

Host tests cover gravity rejection, first-reference calibration, debounce, cooldown, lock, active touches, tick wraparound and all four coordinate corners. ESP-IDF compilation and publication checks validate integration; hardware results must be recorded separately.
