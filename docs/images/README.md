# MeshCrossway — image sources and V1 visual status

[Project README](../../README.md) · [Current BOM and pin-level proposal](../hardware-wiring.md)

## Provisional final-device concept

The selected arrangement is **one custom carrier PCB** with a Waveshare ESP32-S3-Touch-LCD-3.5, **two Seeed XIAO ESP32S3 + Wio-SX1262 kits**, u-blox MAX-M10S GNSS breakout, onboard microSD and a protected **rear-mounted 1S LiPo battery**. Two LoRa antennas are independent; the enclosure is intended to be nearly invisible and transparent.

- [Approved provisional hero image](meshcrossway-hero.webp)
- [Approved provisional exploded view](architecture/meshcrossway-exploded.webp)

These are **AI-generated concept illustrations**, not actual supplier photographs, mechanical CAD, electric schematics or proof that the components fit as shown. The physical appearance, component placement, silkscreen, connectors, antenna routing and proportions may differ from the finished boards. Never use these renders to identify exact part orientation, voltage or connector pinout.

## Native vector diagrams

| Diagram | Source | Purpose |
|---|---|---|
| System architecture | [architecture/system-overview.svg](architecture/system-overview.svg) | Unified host, separate radios, rear power |
| Hardware/carrier enclosure | [hardware-overview.svg](hardware-overview.svg) | One PCB and transparent front/rear layers |
| Wiring map | [architecture/wiring-diagram.svg](architecture/wiring-diagram.svg) | UART + GNSS proposed signals |
| Radio UART wiring | [radio-wiring.svg](radio-wiring.svg) | Meshtastic/MC TX↔RX pairs |
| Connector GPIO guide | [connector-reference.svg](connector-reference.svg) | Host GPIO and XIAO pin identification |
| GNSS and battery | [gnss-and-power.svg](gnss-and-power.svg) | GNSS and rear-battery relationship |
| Power architecture | [architecture/power-architecture.svg](architecture/power-architecture.svg) | Conditional J8 3V3 supply |
| Communication stack | [architecture/communication-stack.svg](architecture/communication-stack.svg) | Native protocol boundaries |
| Firmware ownership | [architecture/firmware-responsibilities.svg](architecture/firmware-responsibilities.svg) | Host/radio responsibilities |
| Tracking mode concept | [architecture/direct-tracking-modes.svg](architecture/direct-tracking-modes.svg) | Planned zero-hop behavior |

SVG diagrams are maintained by [scripts/generate_wiring_diagrams.py](../../scripts/generate_wiring_diagrams.py). They are editable source graphics, but **not validated schematics, mechanical drawings or firmware tests**. UART module pins, 3.3 V spare current, antenna isolation, battery dimensions and the combined PCB design require bench verification before manufacturing.

## Images that remain intentionally separate

- [UI simulator illustrations](../ui-prototype/README.md): simulated UI examples, not physical-hardware images.
- [Protocol comparison figures](../meshtastic-vs-meshcore.md): conceptual packet routing and coverage; no selected radio model is implied.
- [Third-party Amazon listing photo archive](amazon/README.md): credit and source links. Aideepen/Heltec V3 photos are superseded, never the active V1 BOM.

Third-party product images are not relicensed by the project. Use official Seeed, Waveshare and u-blox datasheets and PCB CAD for any actual placement or manufacturing work.
