# MeshCrossway Android 0.4.11 beta

Each Home radio card now shows the **radio's battery** beside the Bluetooth bars.

- **Meshtastic:** the battery level the radio reports for itself in its device
  telemetry. External power without a battery reading is shown as **USB**.
- **MeshCore:** the battery voltage is read about 40 seconds after connecting and then
  every 5 minutes, and shown as an approximate charge using a single-cell LiPo curve.
- 15% or less is shown in red. The exact reading is available to screen readers.

Includes the 0.4.10 single-line radio card state and static settings summary.

Install over the current app **without uninstalling**. Version code 45, same permanent
signing certificate and data format. Field testing remains pending.
