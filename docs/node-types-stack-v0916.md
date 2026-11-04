# v0.9.16 node types and LVGL stack fix

v0.9.16 is an experimental firmware candidate. The current stable fallback remains v0.8.26 until this build is confirmed on hardware.

## Node cards

- Replace the generic MT/MC prefix with a compact circular type marker: person/companion, repeater, room, tracker, or sensor.
- Show the network and node type in the metadata, along with the last four ID characters.
- MeshCore cards show Direct or Flood and relative last-heard age.
- Cards retain the fixed sizes and spacing that stopped text overlap.

Meshtastic User.role is decoded from its protobuf field and cached only for the live session. This keeps the existing SD archive layout unchanged.

## Reset diagnosis

The supplied firmware ELF symbols resolve the panic to MeshCore's node-list batch renderer calling the emoji font converter on each node name. LVGL then overran the task stack while refreshing the label; the preceding watchdog event is consistent with the same expensive refresh. Node and channel list labels now use the normal LVGL font path. Chat messages retain emoji rendering.
