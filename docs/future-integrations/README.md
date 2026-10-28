# Future Integrations & Collaborations

This directory tracks **potential** integrations, interoperability work and community collaborations for MeshCrossway. Entries are research ideas, **not implemented features, endorsements or established partnerships**. Keep the V1 modular hardware and native radio identities independent; do not introduce automatic cross-network rebroadcasting.

## Opportunity registry

| Initiative | Upstream repository / discovery link | Potential value | Status |
|---|---|---|---|
| [Colorado Mesh-Client](https://github.com/Colorado-Mesh/mesh-client) | [Source](https://github.com/Colorado-Mesh/mesh-client) | Desktop interoperability, Wi-Fi/TCP radio interfaces, diagnostics, TAK/CoT | Research; contact maintainers before proposing integration |
| [MeshMapper](https://github.com/search?q=MeshMapper&type=repositories) | [Source](https://github.com/search?q=MeshMapper&type=repositories) | Coverage mapping, field tests and potential data collaboration | Research; confirm official repository/API and discuss partnership after hardware validation |
| [ATAK / ATAK-CIV](https://github.com/deptofdefense/AndroidTacticalAssaultKit-CIV) | [Source](https://github.com/deptofdefense/AndroidTacticalAssaultKit-CIV) | CoT location sharing and tactical mapping | Future research; validate interface and licenses |
| [Reticulum Network Stack](https://github.com/markqvist/Reticulum) | [Source](https://github.com/markqvist/Reticulum) | Potential third protocol, distinct from V1 radios | Future research |
| [Meshtastic](https://github.com/meshtastic/firmware) | [Source](https://github.com/meshtastic/firmware) | Upstream Meshtastic radio firmware and serial protobuf API | V1 dependency; not a partnership |
| [MeshCore](https://github.com/meshcore-dev/MeshCore) | [Source](https://github.com/meshcore-dev/MeshCore) | Upstream MeshCore firmware and companion API | V1 dependency; not a partnership |

## Existing research

- [MeshMapper investigation](../future-meshmapper-integration.md) — proposed community/API discussion after viable hardware exists.
- [ATAK / CoT investigation](../atak-future-integration.md) — future interoperability research.
- [Reticulum investigation](../reticulum-future-integration.md) — possible future protocol.
- [Colorado Mesh-Client opportunity](colorado-mesh-client.md) — desktop interoperability and collaboration.

## Process

1. Identify the project's **official** repository and maintainers; replace discovery links once verified.
2. Document value, protocol/API compatibility, licensing, privacy and RF airtime implications.
3. Prototype in isolation and test against real hardware before declaring support.
4. Contact maintainers with a narrow proposal; record their response and any permissions.
5. Promote to roadmap only after feasibility and explicit project approval.

Avoid copying GPL-licensed code into MIT-distributed MeshCrossway components without a separate license review. Prefer documented protocol interoperability and independent implementations.
