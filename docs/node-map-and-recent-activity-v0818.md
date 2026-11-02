# v0.8.18: node map and recent activity

Status: experimental candidate. v0.8.17 remains the recommended stable firmware.

## Map nodes

The map can show or hide saved node locations from both independent radios. Meshtastic locations come from valid node positions; MeshCore locations come from contacts that report the protocol's location flag. Markers use different colors per network. The Show saved radio nodes setting is stored with map preferences and defaults to visible for existing devices. Nodes without a valid position are omitted. The map only displays locations already present in the radios' records; MeshCrossway does not share positions between networks.

## Home status and ordering

Home cards show the number of nodes saved by each radio and how many have a heard/advertised timestamp within the last 15 minutes. The recent figure is a timestamp-based indicator, not a guarantee that a node is reachable at this exact instant. The GPS card adds the receiver-reported satellite count.

The Nodes / Networks lists put the most recently heard Meshtastic nodes and most recently advertised or messaged MeshCore contacts first. Groups and Channels put the networks with the most recently received message first; favorites no longer outrank a newer message.

## Compatibility and data

The existing map configuration layout is retained; one previously reserved byte stores the node-marker preference. Older saved map settings therefore load with node markers enabled. Node, group, channel and position data stays local to its radio protocol and is not bridged or synchronized between networks.

Device validation is still required for marker rendering with both radios connected, saved visibility across restart, and timestamp behavior when the device clock is not synchronized. Activity counts use only valid Unix timestamps and show zero recent nodes when no trustworthy current device time is available.
