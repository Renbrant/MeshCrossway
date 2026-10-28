# Colorado Mesh-Client — collaboration opportunity

- Repository: https://github.com/Colorado-Mesh/mesh-client
- Architecture: https://github.com/Colorado-Mesh/mesh-client/blob/main/ARCHITECTURE.md
- Contributions: https://github.com/Colorado-Mesh/mesh-client/blob/main/CONTRIBUTING.md
- Status: **research only; no collaboration agreed** (2026-10-28).

## Why it matters

Mesh-Client is an Electron desktop application supporting Meshtastic, MeshCore and Reticulum with concurrent protocol sessions, BLE/serial/TCP connectivity, SQLite history, RF diagnostics and TAK/CoT features. MeshCrossway is a standalone touchscreen embedded device with two independent XIAO/Wio radios and a Waveshare controller. They are complementary, not interchangeable.

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
- First prove real serial APIs and concurrent radio operation on bench hardware.

## Next action

Once the V1 serial integration works, open a focused upstream discussion with device architecture, firmware versions, transport proposal and test results. Do not imply an existing partnership.
