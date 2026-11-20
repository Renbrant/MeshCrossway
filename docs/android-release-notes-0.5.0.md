# MeshCrossway Android 0.5.0 beta

- **Connections and signal strength on the map.** Open the map, tap **Layers** and turn on
  **Connections and signal strength** (on by default). Lines join the nodes that heard each
  other over LoRa, coloured by how much signal is left above what LoRa can still decode at your
  radio's spreading factor:
  - **green**: 10 dB or more of margin (solid link);
  - **yellow**: 5–10 dB (works, but can drop out);
  - **red**: under 5 dB (at the edge);
  - **grey**: a contact seen in a packet path, with no signal value.

  For example, at LongFast (SF11, limit −17.5 dB) an SNR of −5 dB is green and −15 dB is red.
  If the radio has not reported its spreading factor yet, SF11 is assumed and the details say so.
- **Measured vs reported.** Solid lines are receptions measured by your own radio: Meshtastic
  packets that arrived without a relay, the radio's node list entries marked 0 hops away, and
  the last repeater of each MeshCore flood packet. Dashed lines were reported by other nodes:
  Meshtastic neighbor info and traceroute replies, MeshCore repeater neighbor lists (after a
  manual neighbors query) and the hops of a MeshCore flood path.
- **Tap a line** to see who heard whom, each SNR and RSSI, the source and how long ago.
  Lines fade over 24 hours and then disappear. The network filter (Both / MT / MC) applies.
- **No invented links.** A hop count is never drawn as a line. MeshCore paths name repeaters
  by a short key hash: a hash that matches more than one known node is not drawn. MQTT copies
  are not counted as radio receptions. Lines show radio contact, not a guaranteed route.
- Links are kept for the current radio while the app runs, including reconnects, and are
  cleared when you switch to a different radio. Nothing new is transmitted to collect them.

Install over the current app **without uninstalling**. Version code 51, same permanent
signing certificate and data format.
