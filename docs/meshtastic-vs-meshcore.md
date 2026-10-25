# Meshtastic and MeshCore: concepts and differences

[Back to the MeshCrossway README](../README.md)

This guide explains the two mesh ecosystems selected for the first MeshCrossway prototype. It is an introduction, not a substitute for the documentation of the exact firmware version used in a build.

## The shared foundation: LoRa and mesh networking

**LoRa** is the radio modulation used by the selected nodes. **Meshtastic** and **MeshCore** define different systems above that radio layer: packet formats, addressing, routing, messaging, and client interfaces.

A mesh can relay packets through intermediate nodes to reach beyond a direct radio link. Delivery still depends on reachable peers, compatible settings, available airtime, and the network topology. Neither system requires the internet for its core local radio operation.

These systems are intended for short messages and small data payloads. They are not substitutes for broadband Wi-Fi or cellular data. They are also distinct from **LoRaWAN**: a LoRaWAN gateway does not automatically relay Meshtastic or MeshCore traffic.

References: [Meshtastic introduction](https://meshtastic.org/docs/introduction/) and [MeshCore project overview](https://github.com/meshcore-dev/MeshCore).

## What is Meshtastic?

[Meshtastic](https://meshtastic.org/) is an open-source ecosystem for off-grid communication using supported radio devices. Its firmware and client applications support text messaging, location sharing, and additional functions such as telemetry, depending on hardware and configuration.

Users can interact through supported phone, web, and desktop tools or through devices with their own controls. A phone is an interface to a radio node; it does not replace the LoRa radio.

### How its mesh works

Meshtastic uses managed flooding for broadcasts, with mechanisms to limit redundant rebroadcasts. Whether a node relays traffic depends on its role and settings; it is incorrect to assume every node repeats everything.

Direct messaging has used a different routing approach since firmware 2.6, including learned next-hop behavior. Describing all current Meshtastic traffic as indiscriminate flooding is inaccurate.

See the official [mesh algorithm](https://meshtastic.org/docs/overview/mesh-algo/).

### Channels, identities, and privacy

Channels provide shared messaging contexts and keys. A logical channel is not the same thing as a separate RF frequency.

A channel with the published default key is not private. Direct messages use a different key mechanism from channel messages. Some security features depend on firmware version and settings; packet headers remain visible for routing.

See [Meshtastic encryption](https://meshtastic.org/docs/overview/encryption/).

### Integration with MeshCrossway

The planned adapter will use the [Protobuf client API through serial `PROTO` mode](https://meshtastic.org/docs/configuration/module/serial/). The dedicated node will handle the native radio protocol; the ESP32 will manage the interface and application data.

## What is MeshCore?

[MeshCore](https://meshcore.io/) provides a lightweight C++ mesh library and firmware applications for packet radios, including supported LoRa devices.

Its standard roles separate user endpoints from forwarding infrastructure:

| Role | Purpose |
|---|---|
| Companion | Connects a client to the radio network; standard Companion nodes do not relay other users' traffic |
| Repeater | Forwards packets to extend coverage |
| Room Server | Hosts shared posts that clients can retrieve; forwarding is separately configurable |

Two Companions can communicate within direct radio range without a repeater.

### How its mesh works

MeshCore combines flooding and explicit paths. A first direct exchange can discover a route, which later direct messages can reuse. Group channels use flooding rather than a destination-specific route.

This can reduce repeated dissemination for direct traffic when useful paths exist. It does not establish a universal advantage for all traffic or topologies.

### Software openness

The upstream MeshCore firmware repository uses MIT. Its FAQ distinguishes that open code from closed components, including Liam's native mobile apps and specific T-Deck firmware. Do not assume every product in the ecosystem shares the same license.

Source for roles, routing, and openness: [official MeshCore FAQ](https://docs.meshcore.io/faq/).

### Integration with MeshCrossway

The planned adapter uses the [Companion protocol](https://docs.meshcore.io/companion_protocol/). The selected terminal uses two ESP32-S3/SX1262 LoRa V3 boards from the Aideepen two-pack. The reviewed upstream ESP32 Companion implementation includes hardware UART selection through `SERIAL_RX` and `SERIAL_TX`; the proposal uses radio RX GPIO4 / TX GPIO5. A compatible source snapshot, custom build and actual-board validation are still required. See the [current wiring and purchase guide](hardware-wiring.md).

## Main differences

| Aspect | Meshtastic | MeshCore |
|---|---|---|
| Project structure | Firmware, clients, and modules for messaging, position, and telemetry | Mesh library and role-specific firmware applications with associated clients |
| Forwarding model | Relaying depends on device role and rebroadcast settings | Standard Companions do not relay; Repeaters provide forwarding |
| Broadcast or group routing | Managed flooding | Flooding for group channels |
| Direct-message routing | Routing differs from broadcasts; includes learned next-hop behavior | Can discover a path and embed it in later direct packets |
| Group communication | Logical channels with shared configuration and keys | Group channels; Room Servers offer a separate shared-post model |
| Central controller integration | Protobuf client API | Companion protocol |
| Licensing | Review licenses per firmware, client, and dependency | Upstream firmware is MIT; some official client/device software is closed |
| MeshCrossway implementation | Dedicated node and adapter | Separate dedicated node and adapter |

Routing details: [Meshtastic algorithm](https://meshtastic.org/docs/overview/mesh-algo/) and [MeshCore FAQ](https://docs.meshcore.io/faq/). Integration details: [Meshtastic serial module](https://meshtastic.org/docs/configuration/module/serial/) and [MeshCore Companion protocol](https://docs.meshcore.io/companion_protocol/). License terms should be checked in each upstream repository.

**Neither is universally better.** Local users, available infrastructure, traffic patterns, hardware, and actual field measurements matter more than a blanket ranking. This comparison does not claim that either system always delivers better range, battery life, capacity, or security.

## Can they communicate with each other?

**Not natively.** Compatible radio chips or the same nominal frequency do not make their packets interoperable. Their protocols and client interfaces differ.

A board supported by both projects can generally be flashed for one or the other, but support must be checked for its exact revision and firmware target. That does not prove simultaneous operation of both stacks on one radio.

The MeshCrossway design therefore uses:

- One node running Meshtastic.
- One node running MeshCore.
- One ESP32 controller presenting both through a common touchscreen.

This is an application-level integration. Automatic forwarding between networks is outside the initial scope.

A future bridge would need explicit forwarding rules, identity mapping, loop prevention, and treatment of private messages and location data. Native encryption would terminate and restart at the bridge; connecting two adapters does not create cross-protocol end-to-end encryption.

## What this means for the prototype

The following are MeshCrossway engineering decisions and validation requirements:

1. Keep contacts, channels, keys, and message identifiers scoped to their protocol.
2. Preserve the native meaning of acknowledgments; do not label every acknowledgment as delivery or human reading.
3. Maintain separate queues and reconnection handling for each adapter.
4. Present position age and source explicitly rather than assuming identical location behavior.
5. Test nearby-transmitter interference in both directions. Two active nodes do not guarantee uninterrupted simultaneous reception.
6. Freeze tested firmware versions and configurations before publishing reproducible assembly instructions.

## Official links

### Meshtastic

- [Website](https://meshtastic.org/)
- [Introduction and documentation](https://meshtastic.org/docs/introduction/)
- [Firmware repository](https://github.com/meshtastic/firmware)
- [Mesh routing algorithm](https://meshtastic.org/docs/overview/mesh-algo/)
- [Encryption and limitations](https://meshtastic.org/docs/overview/encryption/)
- [Serial module and PROTO mode](https://meshtastic.org/docs/configuration/module/serial/)
- [Web flasher](https://flasher.meshtastic.org/)

### MeshCore

- [Website](https://meshcore.io/)
- [Documentation](https://docs.meshcore.io/)
- [Firmware repository](https://github.com/meshcore-dev/MeshCore)
- [Frequently asked questions](https://docs.meshcore.io/faq/)
- [Companion protocol](https://docs.meshcore.io/companion_protocol/)
- [Companion firmware source](https://github.com/meshcore-dev/MeshCore/tree/main/examples/companion_radio)
- [Web flasher](https://flasher.meshcore.io/)

MeshCrossway is an independent project. References to upstream projects do not imply affiliation or endorsement.

