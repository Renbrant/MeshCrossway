# MeshCrossway Android 0.5.1 beta

Home now tells the truth when one or both radios are off.

- **RF interference reads zero with one radio offline.** Two radios can only disturb each other
  while both are on. With one offline, packet loss shows **0 %**, the peak is hidden and the
  status reads, for example, **None · MeshCore offline**. The RF coexistence page says
  **No interference right now** and still analyses the connected radio's settings, so you can
  prepare them before turning the other radio on.
- **No stale signal bars.** An offline radio shows a red **X** and **No signal** instead of the
  bars from its last Bluetooth reading.
- **Recent nodes expire.** The "heard in the last 15 min" count keeps only nodes heard before the
  radio went offline and drops to **0** once it has been offline for 15 minutes. The time each
  radio was last online is saved, so this also holds after the app is closed and reopened.
- **Messages, contacts, channels and the map stay available offline**, as before.

Install over the current app **without uninstalling**. Version code 52, same permanent
signing certificate and data format.
