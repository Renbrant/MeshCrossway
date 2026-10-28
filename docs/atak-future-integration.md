# Future ATAK / Cursor on Target integration

**Status:** Future research / not part of V1. No ATAK support is currently implemented or verified.

## What is ATAK?

Android Team Awareness Kit (ATAK) is an Android geospatial situational-awareness application used to display maps, positions, markers, routes, and shared operational information. ATAK-CIV is the civilian distribution. ATAK is **not** a LoRa radio protocol; off-grid exchange depends on compatible plugins, gateways, and network transport.

## Why consider it for MeshCrossway?

MeshCrossway is planned as an offline-first terminal with **two independent radio modules**: one Meshtastic and one MeshCore Companion. A future ATAK integration could expose authorized location updates, waypoints, and selected messages to an ATAK client while preserving the identity and security semantics of their originating networks.

### Candidate architecture (concept only)

```text
ATAK-CIV (Android)
      |
  Compatible plugin / gateway (to be selected)
      |
  CoT translation and policy layer (future)
      |
 MeshCrossway host
      |                         |
 Meshtastic adapter       MeshCore Companion adapter
      |                         |
 XIAO + Wio-SX1262        XIAO + Wio-SX1262
```

**Cursor on Target (CoT)** is a candidate common event format for geographic positions and markers. CoT does not itself solve transport, encryption, acknowledgments, or native radio interoperability. A bridge must not assume Meshtastic and MeshCore can exchange packets directly.

## Proposed investigation

1. Confirm current ATAK-CIV availability, plugin APIs, CoT schemas and relevant distribution/licensing conditions.
2. Test existing Meshtastic-to-ATAK plugins with the chosen firmware and transport, checking whether a dedicated Android Meshtastic radio connection is required.
3. Investigate MeshCore Companion APIs and the feasibility of a separate CoT adapter; do not assume a maintained ATAK/MeshCore plugin exists.
4. Evaluate BLE, Wi-Fi, USB, or an external gateway between the ATAK Android device and MeshCrossway; the modular BLE Core-to-Pod links do not automatically provide an ATAK connection.
5. Define a minimal translation schema for position, timestamp, source network, identity, accuracy, expiry, and marker type. Avoid conflating identities from different networks.
6. Implement explicit user authorization and per-network sharing policies. Default to **no automatic cross-network forwarding**, consistent with the V1 design.
7. Validate airtime budgets, message size, rate limits, deduplication, stale-position handling, offline operation, authentication, encryption boundaries, and loss/reconnection behavior.
8. Prototype with real devices and document observed interoperability before marking the feature supported.

## Possible future features

- Show MeshCrossway peers and GNSS positions as ATAK map markers.
- Export user-approved waypoints and location events using CoT.
- Optionally exchange selected text or operational events if the target plugin and transport support them.
- Offer a controlled gateway mode with explicit routing rules and visible source-network labels.

## Out of scope for V1

- Bundling ATAK on the embedded Waveshare display.
- Native ATAK support in either upstream radio firmware.
- Transparent Meshtastic-to-MeshCore routing.
- Guaranteed delivery, continuous live tracking, or safety-critical service claims.

## References

- [CivTAK community information](https://www.civtak.org/)
- [Meshtastic documentation](https://meshtastic.org/docs/)
- [MeshCore project](https://github.com/meshcore-dev/MeshCore)

These are research starting points; plugin compatibility and current API details require verification before implementation.
