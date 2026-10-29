# v0.6.7 — Bluetooth PIN keypad

EXPERIMENTAL — NOT HARDWARE TESTED.

The owner physically exercised v0.6.6: BLE host synchronization, repeated scans,
discovery of the Heltec Companion and connection initiation worked without the
previous LCD/SPI watchdog freeze. Pairing could not finish because the PIN keyboard
was invisible. This validates those stages, not completed encryption or messaging.
The supplied log contains no new LCD transfer error or watchdog; the reset at the
start is explicitly identified as USB_UART_CHIP_RESET.

The v0.6.6 keyboard inherited LVGL 8.4's BOTTOM_MID alignment. Its positive y offset
placed the entire keyboard below its 236-pixel parent, which clipped its children.
The regression test reproduces that layout with real LVGL and asserts the clipping.

v0.6.7 changes only the pairing UI, version labels, non-secret pairing diagnostics
and associated checks. The new top-layer dialog fills 480 x 320; its numeric keypad
uses explicit TOP_LEFT alignment, large 3-column keys, backspace and OK. PAIR also
submits; CANCEL disconnects. Input accepts six digits, preserves leading zeros and
is masked. Short-input and submission failures show feedback. Keyboard storage is
static, auto-repeat is disabled, and the textarea is detached before dialog deletion.
Existing Wi-Fi keyboard, LCD DMA buffers, SPI settings and BLE initialization are
preserved. PIN values are never printed in serial diagnostics.

`test_ble_pairing_ui.py` builds the LVGL 8.4 copy in the pinned Waveshare BSP and
runs the actual firmware dialog code with a simulated pointer and mocked BLE.
It checks the old clipping regression, visible bounds, touch-key size, one digit
per held tap, six-digit limit, leading zeros, backspace, OK/PAIR submission, cancel,
failed submission feedback and repeated open/close. This is a host UI test; it
cannot prove hardware pairing or radio encryption.

The Web Flasher retains the approved v0.6.2 default and its identical pinned binary.
v0.6.7 requires explicit experimental opt-in. v0.6.6 is not promoted to a fully
approved release because the pairing test remains incomplete.

Owner test: install v0.6.7 without erase, enable BLE, scan and select the Heltec.
The PIN screen should show the numeric keypad immediately. Enter the configured
six-digit PIN and tap PAIR or the checkmark. Confirm the secure-link status,
touch/Wi-Fi responsiveness and disconnect/reconnect. Report any pairing error and
serial lines from before connection; do not share the PIN. No application messages
are implemented or transmitted by this candidate.
