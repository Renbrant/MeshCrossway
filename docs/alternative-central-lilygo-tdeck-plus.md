# Alternative central controller: LilyGO T-Deck Plus (proposal)

**Status:** Candidate / feasibility assessment, not implemented or validated on hardware.
**Recorded:** 2026-10-28
**Role:** Optional drop-in *functional replacement* for the Waveshare ESP32-S3 Touch LCD 3.5 central console, **not** a pin-compatible hardware replacement.

## Product decision

MeshCrossway supports two proposed central-console choices:

1. **Main modular reference:** Waveshare ESP32-S3 Touch LCD 3.5 paired with independent MeshCore and Meshtastic radio nodes.
2. **Alternative portable console:** LilyGO **T-Deck Plus**, running a dedicated MeshCrossway firmware built for its screen, keyboard, trackball and power/GNSS capabilities (verify exact board revisions). It can replace the Waveshare's *controller/UI role* while retaining independently powered radio devices.

Neither option supersedes the other. Preserve existing KiCad designs and keep the three-device modular architecture as the current reference.

## Connectivity architecture

```text
             LilyGO T-Deck Plus
       MeshCrossway UI and coordinator
         screen / keyboard / trackball
               BLE central
                /      \
      BLE connection   BLE connection
             /              \
       MeshCore node     Meshtastic node
       (e.g. Heltec V3) (e.g. Heltec V3)
          LoRa mesh A      LoRa mesh B
```

Additional BLE peers may be supported, subject to tests of connection count, latency, memory and coexistence. Start with two peers; target four only after stability tests. Each node must provide the proper MeshCore Companion or Meshtastic BLE interface; a repeater-only firmware may not provide the required interface. Distinct networks continue operating independently; **do not imply automatic message cross-network bridging**, which requires separate policy, protocol, consent and loop prevention design.

The built-in T-Deck Plus SX1262 is a potential *optional* additional LoRa interface, not a substitute for two radios if concurrent independent mesh reception is required. Wi-Fi may serve the browser UI and network-based management, subject to BLE coexistence validation.

## Firmware and launcher integration

- Build a **board-specific MeshCrossway T-Deck Plus target** sharing protocol adapters and app logic with the existing central console where practical.
- Provide a Launcher-compatible artifact after confirming which **Launcher Web Flasher** and image/partition/storage mechanisms are actually installed on the user's device.
- Launcher handles launching/installing the application; configuring and connecting mesh peers belongs to the MeshCrossway firmware UI. Launcher-based configuration itself is a future enhancement dependent on supported launcher mechanisms.
- Keep local configuration for paired peers, identities, network settings and UI preferences, respecting firmware-switching data preservation. Do not assume flash partitions are preserved without testing.
- Support on-device UI on the screen/keyboard/trackball; a browser control panel is an optional companion interface, not required for basic field operation.

## Validation gates

1. Identify exact T-Deck Plus board variant, display/touch input capabilities, and installed Launcher Web Flasher version.
2. Compile, install and successfully return to Launcher with a minimal T-Deck Plus firmware, without damaging existing firmware/data.
3. Discover and connect to one Heltec V3 MeshCore Companion BLE node and one Heltec V3 Meshtastic BLE node; read identity and receive/send test messages over their respective protocols.
4. Sustain two concurrent BLE connections; characterize reconnection, sleep/power and latency; increase toward four only if measured stable.
5. Validate keyboard, trackball, display, power management, battery monitoring, microSD and GNSS where present.
6. Verify BLE/Wi-Fi coexistence and optionally test internal SX1262 alongside two external nodes.
7. Define routing policy: unified inbox is distinct from automatic MeshCore/Meshtastic gateway forwarding; preserve upstream identity/security and prevent unwanted retransmission.
8. Package versioned firmware and explicit Launcher installation/rollback instructions.

## Suggested phased implementation

- **PoC 0:** launcher compatibility + screen/input + persistent configuration.
- **PoC 1:** BLE scan and connect to one upstream node.
- **PoC 2:** concurrent MeshCore and Meshtastic connections, independent inbox and send.
- **PoC 3:** optional third/fourth BLE node, Wi-Fi Web UI, robust reconnect.
- **Future:** selective cross-network bridging with explicit policy and security review; integrated T-Deck Plus LoRa radio.

## Scope limitations

This is a candidate architecture and an alternative central unit, **not an approved working firmware or verified drop-in replacement**. No claim of universal BLE compatibility, unlimited simultaneous peers, or launcher-supported configuration is made. No PCB/KiCad changes are authorized by this document.
