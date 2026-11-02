# Display settings — v0.8.19

Pull down the top bar to change backlight brightness (5–100%) and screen timeout. Options: Always on, 30 seconds, 1, 2, 5 or 10 minutes. Defaults preserve the previous 80% brightness and always-on behavior.

The BSP PWM API applies brightness immediately. Preferences are validated and stored in NVS after a short debounce. A dark touch-blocking overlay is installed before the backlight turns off. The waking tap restores the selected brightness and is consumed before underlying controls can receive it. Only the backlight sleeps; radios, messages, GNSS and track recording continue.

v0.8.17 remains the recommended hardware-tested stable release. v0.8.19 requires physical brightness and wake testing.
