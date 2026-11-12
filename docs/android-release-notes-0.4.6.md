# MeshCrossway Android 0.4.6 beta

Repeater settings load automatically after administrator login. Tap a card to edit
its current value, then Save; the app reads the setting again to show what the
repeater actually reports. Numeric keyboards, switches and labeled radio inputs
replace nested read/change menus.

This beta includes the 0.4.5 card interface plus two feedback corrections: public
names containing text such as "key" still receive normal save/readback confirmation,
and advanced-console replies remain visible when explicitly requested. Password
command responses remain private. Console, password changes and reboot stay under
Advanced options, and diagnostics are expandable.

Configuration reads are sequential, stop when leaving the view, and never create
writes or retry automatically. Firmware support varies; field retesting of the new
interaction is pending. Version code 40, with the existing permanent signing key.

Install over 0.4.4 or 0.4.5 **without uninstalling** to retain data. SHA256SUMS.txt
and SIGNING-CERT-SHA256.txt accompany the APK.
