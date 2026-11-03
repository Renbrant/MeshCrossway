# v0.9.7 experimental: nodes and groups

## Node lists
- Node and contact lists now use vertical touch scrolling; side-to-side pagination controls are removed.
- A shared sort control switches between last heard and distance from the current GPS fix. Nodes without positions sort after nodes with known distances.
- Each node shows its last locally recorded hearing time and totals from direct chat messages retained in the device's message history.

The direct-chat message/byte total counts text payloads from the bounded local message window (up to 96 messages) exchanged with that node. Meshtastic and MeshCore do not provide a universal per-repeater forwarded-byte counter in the data currently retained by MeshCrossway, so this figure does not represent all radio traffic routed through a node.

## Groups and channels
- Group/channel lists are ordered by the latest received message and show unread counts on each row.
- The page no longer has lateral pagination, MT Status, or an inline settings/tools button. Radio settings remain in the main settings area.
- The Add Channel action offers MeshCore public `#` channels (name-derived public key), MeshCore private channels (random key), and Meshtastic secondary channel settings.
- Meshtastic channel identity is controlled by its role, name and PSK; a `#` in a Meshtastic channel name is only a label and does not create a MeshCore-style public channel.

## Release
v0.9.7 is experimental; v0.8.26 remains the recommended stable version.
