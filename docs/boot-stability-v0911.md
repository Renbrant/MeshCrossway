# v0.9.11 boot stability

v0.9.11 is an experimental firmware candidate. The hardware-tested stable release remains v0.8.26.

The v0.9.10 device log showed a repeatable `Stack canary watchpoint triggered (main)` immediately after initial Home-screen drawing. The call trace identified `draw()` synchronously invoking `home_refresh()`: the combined stack use exceeded the main task's budget and rebooted the device every few seconds, which appeared as a flickering screen.

Home cards now begin with safe placeholder values and receive live status from the existing 250 ms LVGL timer after initialization. This prevents the large Home refresh frame from nesting under `draw()` on the small main task stack. v0.9.11 keeps the v0.9.10 Wi-Fi/Bluetooth controls and node-list viewport changes.

The ESP-IDF build and automated preflight must pass before publication. Hardware confirmation is still required before considering this release stable.
