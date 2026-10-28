# Colorado Mesh-Client — collaboration opportunity

- Repository: https://github.com/Colorado-Mesh/mesh-client
- Architecture: https://github.com/Colorado-Mesh/mesh-client/blob/main/ARCHITECTURE.md
- Contributions: https://github.com/Colorado-Mesh/mesh-client/blob/main/CONTRIBUTING.md
- Status: **research only; no collaboration agreed** (2026-10-28).

## Why it matters

Mesh-Client is an Electron desktop application supporting Meshtastic, MeshCore and Reticulum with concurrent protocol sessions, BLE/serial/TCP connectivity, SQLite history, RF diagnostics and TAK/CoT features. MeshCrossway is a standalone touchscreen embedded device with two independent XIAO/Wio Radio Pods and a Waveshare Core connected over two BLE links. They are complementary, not interchangeable.

## Possible workstreams

1. **Desktop interoperability:** investigate exposing each MeshCrossway radio through separately compatible Wi-Fi/TCP transports; verify actual APIs and session ownership first.
2. **Diagnostics:** compare route, hop, SNR, congestion and packet history semantics; implement lightweight embedded logic independently.
3. **Field testing:** contribute reproducible dual-radio test cases and logs with location/privacy controls.
4. **TAK / CoT:** evaluate optional position export after core firmware stabilizes.
5. **Community outreach:** introduce MeshCrossway to maintainers and request feedback on a narrow compatibility proposal.

## Boundaries and gates

- Mesh-Client GPL-3.0-or-later versus MeshCrossway MIT: review licensing before reusing source code.
- Simultaneous multi-protocol operation does **not** imply cross-network packet bridging.
- Do not assume their TCP drivers work unchanged with our ESP32-S3 controller.
- Keep all network forwarding opt-in, rate-limited and privacy-aware.
- First prove both native BLE APIs and concurrent radio operation on bench hardware.

## Next action

Once the V1 dual-Pod BLE integration works, open a focused upstream discussion with device architecture, firmware versions, transport proposal and test results. Do not imply an existing partnership.

## License-change lesson

[Mesh-Client documents](https://colorado-mesh.github.io/mesh-client/license/) that it changed from MIT to GPL-3.0-or-later because GPL-3.0-only `@meshtastic/core` was bundled into its Electron renderer. Its separate Reticulum executable is AGPL-3.0-or-later. The explanation concerns incorporated dependencies; it does not establish a requirement for every interoperating client to use GPL.

MeshCrossway retains MIT for original code. Review the exact licenses and version compatibility before importing any source, schema or generated bindings. Independent BLE/TCP interoperability and source-code reuse require different assessments; separate processes/devices alone do not guarantee license independence. See the [licensing policy](../licensing.md) and [dependency review](../third-party-licenses.md).
