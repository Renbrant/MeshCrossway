# MeshCrossway Android 0.4.15 beta

Radio battery on Home, the same way for both radios.

- **One curve for both radios.** The charge level is now calculated by MeshCrossway from the
  battery voltage each radio reports, with the same single-cell LiPo curve for Meshtastic and
  MeshCore. Two radios on one battery now show the same level. The level Meshtastic's firmware
  calculates with its own curve is no longer used (it is only used to recognise external power
  when no voltage is reported).
- **Voltage beside the icon.** Each radio card shows the measured voltage, for example
  `3.73 V`.
- **Level inside the battery.** The battery icon is filled in proportion to the charge, in the
  radio's colour, and turns red at 15 % or less. A lightning bolt means external power without a
  voltage reading. Screen readers announce both the percentage and the voltage.

Install over the current app **without uninstalling**. Version code 49, same permanent
signing certificate and data format.
