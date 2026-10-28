# MeshCrossway modular architecture — approved decision

**Decision date:** 2026-10-28. **Status:** architecture approved; no electrical, BLE, mechanical, RF or environmental validation completed.

## Architecture and scope

MeshCrossway shall comprise three **independently powered, detachable devices**:

1. **MeshCrossway Core:** Waveshare ESP32-S3-Touch-LCD-3.5, LVGL touchscreen UI, central u-blox MAX-M10S GNSS, microSD, message/position storage, two simultaneous BLE central client connections and independent application-level protocol adapters.
2. **MX-M Pod:** Seeed XIAO ESP32-S3 + Wio-SX1262 B2B kit with native Meshtastic firmware configured for its BLE PhoneAPI, a suitable 902–928 MHz antenna, protected rechargeable battery, suitable power management and weather-resistant case.
3. **MX-C Pod:** same hardware kit with MeshCore **BLE Companion** firmware, independent antenna, battery, power management and case.

**BLE is the default and required Core↔Pod data transport.** The two LoRa protocols and identities remain strictly separate. Do not treat the two BLE interfaces as an interchangeable wire protocol: implement one Meshtastic protobuf PhoneAPI BLE client and one MeshCore Companion GATT binary-protocol client.

The two Pods must continue functioning as native radios if the Core is switched off or out of BLE range. BLE disconnection shall never automatically disable their LoRa reception. Phone interoperability should be tested with native applications where firmware supports it, allowing only intended authenticated client sessions.

## Docking and wearability

Design the three units to dock as one handheld, or be carried apart (backpack, vest, helmet, cap, car). **Wireless BLE remains the baseline even while docked** to simplify the first production design. A mechanical dock initially supplies physical retention and optionally charging; **a docked UART data transport is a future option, not a V1 dependency**. Avoid routing exposed UART signals through external contacts merely for future compatibility.

Use keyed mechanical locating features plus a positive latch; magnets can assist alignment but should not be the sole retention system. If charging through contacts is implemented, use protected recessed/covered contacts, charge control, fault/short and ESD protection, and validated isolated power-domain behavior. Do not directly parallel independent LiPo cells or charger outputs. A dock detection switch/Hall sensor can trigger UI presentation but must not change radio TX power without explicit validated policy.

Proposed hardware boundaries:
- One Core controller/display/GNSS battery, charging and enclosure.
- Two small independent Pod PCBs/carriers for mounting XIAO/Wio, LiPo, charging/protection, battery measurement, on/off and service USB.
- No shared large three-device carrier PCB in the **new** product baseline.
- The older integrated Rev A PCB schematic/wiring documents are retained as historical bench references **only**, not production instructions.

## BLE implementation

Core is BLE **Central**, each Pod is BLE **Peripheral**, using documented official native GATT interfaces:
- Meshtastic PhoneAPI service `6BA1B218-15A8-461F-9FA8-5DCAE273EAFD`. `ToRadio` and `FromRadio` protobuf messages over BLE GATT (no UART stream framing).
- MeshCore Companion service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, write RX `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`, notify TX `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`. Negotiate API version and supported command set.

Firmware selection: do **not** flash an old MeshCore `..._serial` variant as the new Pod firmware. Start with matching **Xiao_S3_WIO BLE Companion** upstream target and a verified Meshtastic XIAO ESP32-S3/Wio BLE-enabled target; verify current release names, capabilities, displayless pairing process and battery telemetry on the actual kit. For Meshtastic ESP32 boards, Wi-Fi enablement disables BLE, so keep Wi-Fi disabled in the normal Pod configuration. For MeshCore, avoid relying on Wi-Fi+BLE coexistence even when a recent firmware build makes that possible.

Core requirements:
- Maintain two independent BLE connections; configure ESP-IDF/NimBLE host resources, pairing and reconnection appropriately.
- Never block LVGL while scanning/pairing/receiving; bounded queues, per-radio state machines and reconnect backoff.
- Store bonded identities securely; require authenticated pairing (PIN or equivalent). No fixed demo PIN in shipping build.
- On startup/disconnect, reconcile queued messages and session state; no claims of remote delivery from BLE ACK alone.
- Explicit user choice for phone takeover / changing controller; avoid accidental multi-central contention.
- Build a transport abstraction that can later support docked UART without rewriting network adapters; BLE is primary.

## RF coexistence

Separate physical Pods **do not** inherently solve RF coupling when mounted a few centimetres apart. At US defaults Meshtastic and MeshCore may use close channels in the same ~915 MHz ISM band. Consider +22 dBm TX near a weak-signal adjacent receiver and test receiver desensitization, antenna coupling (S21), conducted/blocking protection, LoRa packet reception and TX collisions. Allow user-selectable legal power presets only after calibrated testing.

Mechanical variants to validate:
- docked handheld: compact antennas and mutual interference worst case;
- shoulder/backpack: better height and separation, altered body blockage;
- headwear: mass, antenna placement and RF exposure near head, no blanket safety assumption;
- vehicle roof: two antennas separated on suitable ground plane, weather seals, cables or contained Pods.

For **vehicle roof mounting**, a magnet by itself is not an approved retention system. Specify vehicle-rated mount, positive backup tether, appropriate antenna ground plane, vehicle paint protection, wind load/highway-speed retention, temperature, vibration, water ingress, road debris and theft/security. Do not place loose LiPo electronics in sun-heated weatherproof boxes without thermal qualification. RF exposure and legal EIRP must account for antenna gain and proximity of people. Verify that BLE reliably reaches the cabin from roof through vehicle structure; antenna positioning can be critical. Weather-resistant roof Pods are a later validated mechanical SKU, not an assumption of the handheld housing.

## Power and charging

Each unit has its own protected rechargeable battery. Final cells, protection IC, charge IC, USB-C/dock charging, battery gauge, cutoffs, autonomy and thermal budget are TBD. XIAO external supply and USB/VBUS power-path isolation must be validated per exact board revision. A shared dock charger must provide **independent appropriately controlled charging paths** for all three batteries; never connect cell terminals together. Include safe firmware behavior during charging and long-term unattended operation.

## Milestones / acceptance gates

1. Flash Meshtastic BLE and MeshCore BLE Companion on **two exact XIAO/Wio kits**, verify native phone connection separately.
2. Pair the Waveshare ESP32-S3 as BLE central with both Pods concurrently, then decode/send native messages without mobile phones.
3. Validate secure pairing, independent reconnection after dropout, reboot recovery, queued inbound event synchronization, BLE packet sizes/MTU and version compatibility.
4. Measure actual Pod TX/RX/BLE current profiles, battery autonomy, thermal behavior and independent charge/protection.
5. RF tests at multiple antenna separations/power settings: inter-radio S21, weak-signal packet loss while the other Pod TXs, both directions, roof/body/docked geometry.
6. Build mechanical prototypes with docking, secure latch, contact protection, USB maintenance access, and separate wear/roof accessories.
7. Conduct wind/road-retention, IP/weather and thermal tests for any car-mounted version; test Core BLE link inside the car.
8. Update project graphics, README, pinout, BOM and KiCad design only when detailed modular schematic/footprints are approved. **Do not fabricate former integrated Rev A as new final architecture.**

## References

- [Meshtastic BLE radio configuration](https://meshtastic.org/docs/configuration/radio/bluetooth/)
- [Meshtastic PhoneAPI transports](https://github.com/meshtastic/meshtastic-sdk/blob/main/docs/protocol.md)
- [MeshCore Companion BLE protocol](https://docs.meshcore.io/companion_protocol/)
- [MeshCore v1.18.0 BLE/Wi-Fi support update](https://blog.meshcore.io/2026/10/15/release-1-18-0)
- [Seeed XIAO ESP32-S3 + Wio-SX1262](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html)

Related legacy/bench documents: [integrated wiring](hardware-wiring.md), [UART architecture](radio-communication-architecture.md), [old power prototype](power-architecture.md), [firmware strategy](radio-firmware-strategy.md).
