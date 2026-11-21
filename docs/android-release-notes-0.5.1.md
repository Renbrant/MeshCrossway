# MeshCrossway Android 0.5.1 beta

- **Connection lines per network.** In **Layers**, Meshtastic and MeshCore now have separate
  switches, so you can show the lines of one network and hide the other.
- **Line style** (Layers → *Line style: width, transparency, colors…*), separately for each network:
  - **Thickness** from 1 to 8 dp. Reported (dashed) lines are drawn at three quarters of it.
  - **Opacity** from 10% to 100%. Lines still fade with age over 24 hours on top of this.
  - **Colors** for each signal level: strong, fair, weak and no value, from a palette of 12.
  - **Show reported (dashed) lines** on or off, to keep only what your own radio measured.
  - **Reset to defaults** per network.
- Changes apply to the map immediately and are kept across restarts. The legend in Layers
  follows the chosen colors and thickness.
- Upgrading from 0.5.0: if you had turned the connections layer off, both networks start off.

Install over the current app **without uninstalling**. Version code 52, same permanent
signing certificate and data format.
