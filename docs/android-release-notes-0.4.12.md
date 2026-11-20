# MeshCrossway Android 0.4.12 beta

New **Settings → RF coexistence** page for running the Meshtastic and MeshCore radios side
by side, for example in the dual-radio car-roof mount.

- **Dashboard** built from the real settings of both radios: an overall status, a gauge for
  the packets one radio is expected to lose because the other one transmits on its own, a
  gauge for the peak power reaching the other radio's antenna input, and a card per radio
  with transmit state, power, automatic airtime per hour and measured airtime.
- **Meshtastic transmission on/off.** Turning transmission off makes the radio receive-only,
  so it causes no interference. Bluetooth stays connected so it can be turned back on.
- **Presets:** Minimum interference (recommended for vehicles), Balanced and Help the mesh.
  Changes are applied one at a time and each one is read back from the radio. The
  confirmation lists every change and **how many times the Meshtastic radio will restart**.
- **Antenna spacing** (side by side or one above the other) for the power estimate.
- **Advanced settings** for both radios, each with the current and recommended value, its
  advantages and disadvantages, and a notice when changing it restarts the radio.

Figures are estimates. Background and technical details:
[RF-COEXISTENCE.md](https://github.com/Renbrant/MeshCrossway/blob/main/hardware/3D/car-dual-solar-mount/RF-COEXISTENCE.md).

Includes the 0.4.11 radio battery on Home.

Install over the current app **without uninstalling**. Version code 46, same permanent
signing certificate and data format. Field testing remains pending, especially the presets
that restart the Meshtastic radio.
