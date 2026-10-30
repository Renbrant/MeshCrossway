# Third-party dependency and asset review

Reviewed 2026-10-28 against MeshCrossway commit `21f0bf761e699767ada7bfb83ccc54bc90984759`. This is a source-level initial inventory, not a complete SBOM or certification of previously published binaries. See the [licensing policy](licensing.md).

## Display firmware build inputs

The build script pins Waveshare commit `d9ff7837148b47b33edadc4b87775c341cf35bbe` and uses `espressif/idf:v5.3.2`. The patcher modifies `examples/esp-idf/02_lvgl_example/main/main.cpp` and `sdkconfig.defaults`, and copies four MeshCrossway headers into the example. These upstream files are not converted to MIT.

| Component | Evidence / version | License evidence | Review state |
|---|---|---|---|
| Waveshare ESP32-S3-Touch-LCD-3.5 example | [Pinned source](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/d9ff7837148b47b33edadc4b87775c341cf35bbe/examples/esp-idf/02_lvgl_example) | Root [Apache-2.0 license](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/d9ff7837148b47b33edadc4b87775c341cf35bbe/LICENSE) | Root text verified; bundled components need their own review |
| ESP-IDF | [v5.3.2](https://github.com/espressif/esp-idf/tree/v5.3.2) | Root [Apache-2.0](https://github.com/espressif/esp-idf/blob/v5.3.2/LICENSE); component exceptions may apply | Root verified; full built-in component and binary-library inventory pending |
| LVGL | Declared `~8.4.0` | [MIT text at v8.4.0](https://github.com/lvgl/lvgl/blob/v8.4.0/LICENCE.txt) | Reference license verified; exact resolved patch and bundled assets pending |
| XPowersLib, in selected Waveshare example | [Pinned LICENSE](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/d9ff7837148b47b33edadc4b87775c341cf35bbe/examples/esp-idf/02_lvgl_example/components/XPowersLib/LICENSE) | MIT, copyright 2022 lewis he | License text verified; preserve notice |
| SensorLib, in selected Waveshare example | [Pinned LICENSE](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/d9ff7837148b47b33edadc4b87775c341cf35bbe/examples/esp-idf/02_lvgl_example/components/sensorlib/LICENSE) | MIT, copyright 2022 lewis he | License text verified; preserve notice |
| esp32-camera, in selected Waveshare example | [Pinned LICENSE](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/d9ff7837148b47b33edadc4b87775c341cf35bbe/examples/esp-idf/02_lvgl_example/components/esp32-camera/LICENSE) | Separate component license | Component present; review file notices and whether linked into each binary |

The selected example's [direct Component Manager declarations](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/d9ff7837148b47b33edadc4b87775c341cf35bbe/examples/esp-idf/02_lvgl_example/main/idf_component.yml) are:

| Dependency | Declared constraint | Resolved revision / license review |
|---|---|---|
| `idf` | `>=5.1` (build uses 5.3.2) | See ESP-IDF row above |
| `lvgl/lvgl` | `~8.4.0` | See LVGL row above |
| `espressif/esp_lcd_touch` | `^1.1.2` | Pending resolved-build evidence |
| `espressif/esp_io_expander_tca9554` | `^2.0.0` | Pending resolved-build evidence |
| `espressif/esp_lvgl_port` | `^2.5.0` | Pending resolved-build evidence |
| `espressif/esp_codec_dev` | `^1.3.4` | Pending resolved-build evidence |
| `espressif/button` | `^4.1.0` | Pending resolved-build evidence |

These ranges do not identify the exact dependency versions compiled into an existing image. Other component manifests introduce additional dependencies. This review does not infer their licenses from their publisher or from the Waveshare root license.

The Waveshare repository also contains Arduino examples and libraries under other licenses, including GPL. The current build selects the ESP-IDF LVGL example, not the Arduino examples. Their mere presence elsewhere in the checkout does not establish inclusion in the display binary.

## Radios and future integrations

| Material | Current relationship | Required action |
|---|---|---|
| Meshtastic / MeshCore Pod firmware | Planned native firmware on separate BLE devices; live dual-Pod integration not yet implemented | Record exact targets, revisions and upstream licenses before redistribution; do not relabel upstream binaries MIT |
| Meshtastic PhoneAPI / MeshCore Companion adapters | Planned Core-side implementation | Review each chosen library, schema, generated binding and copied example before adding it |
| Colorado Mesh-Client | Research reference; no Electron source incorporated by this work | GPL-3.0-or-later application; check source/version compatibility before any reuse; its separate Reticulum helper is AGPL |
| WADAMESH | UI/architecture research; stability benchmark at `26578cdc87896c78d533c00568846ecdd7dcfb2b` | [Stability audit and reviewed paths](stability-audit-v0711.md). No source/assets imported; new allocator uses documented ESP-IDF/LVGL APIs. Future source reuse requires separate GPL review |
| Reticulum / ATAK / MeshMapper | Future research only | Review the exact implementation, client, backend/API terms and any redistributed assets at integration time |

## Images, CAD and documentation

Use the existing [image-source register](images/README.md), [manufacturer references](hardware-sources.md) and [CAD model notes](../hardware/kicad/3dmodels/README.md). Manufacturer photographs and downloaded CAD are not relicensed by MeshCrossway. The vendor-model download script is a retrieval tool, not a redistribution license. Original hardware receives no new CERN-OHL license in this decision.

## Open review items

- [ ] Retain and review the full resolved ESP-IDF component graph, locks, licenses, notices, fonts and binary-library terms for each firmware release.
- [ ] Generate a complete per-release notice bundle and attach it to firmware distribution, including Web Flasher downloads. The repository's initial notice collection does not establish that this has already happened.
- [ ] Record patched Waveshare source changes alongside release provenance and preserve any applicable upstream NOTICE files.
- [ ] Audit historical/recovery images against their own source commits and resolved components without changing tested binary bytes.
- [ ] Record exact Pod firmware licenses and source obligations before distributing Pod binaries or assembled devices.
- [ ] Review the actual schemas, bindings and libraries chosen when real BLE adapters are implemented.
- [ ] Confirm redistribution permissions before adding vendor CAD, map data or other third-party assets to releases.


## Meshtastic adapter review — v0.7.12

The Core adapter now implements original bounded protobuf wire handling, session, UI and separate SD storage; it incorporates no upstream library, generated binding or `.proto` source. Research-only GPL-3.0 schema revision `ffd5ad428da6db4a7ddfe62e06548718cc2603e0` and firmware revision `6d41e279f1f51bd59f687b9d441c1bf47b1594fc` are identified with reviewed paths in the [provenance register](meshtastic-v0712.md#primary-references-and-provenance). No Pod firmware is redistributed by this change. Future bindings or copied upstream source require a new dependency/license review. Existing binary-release review items above remain open.

## Raster map implementation (v0.8.1 candidate)

Uses the existing pinned LVGL 8.4 LodePNG decoder (Lode Vandevenne, zlib-style notice retained in the upstream library), now enabled through `LV_USE_PNG`. No external map application source was copied. Raster data is separate from firmware code: OpenStreetMap attribution is displayed; tile service terms apply. Offline XYZ packs must carry the provider's required attribution and permission for offline use. See [map guide](maps-gps-v081.md).

## v0.8.10 remote companion

The offline dashboard and mailbox are original MeshCrossway code; the unchanged logo comes from `docs/site/assets/meshcrossway-logo.svg`. No new firmware dependencies are added. SHA-256 uses the existing ESP-IDF mbedTLS component. The host-only `tests/remote_chat_test.cpp` links system OpenSSL ([source and license](https://github.com/openssl/openssl/tree/openssl-3.0.13), Apache-2.0 in OpenSSL 3.x), tested locally with OpenSSL 3.0.13. CI uses its runner-provided version; no OpenSSL source or binary is copied into firmware or distributed by this change. Runtime release dependency/license review items remain open.

## v0.8.11 Home and GNSS accuracy

Original presentation/parser code and existing math/UART/LVGL dependencies only. Research reference: u-blox M10 SPG 5.10 interface description, UBX-21035062 R03, sections PUBX,00, GST and NMEA position validity. The document is not bundled; no manufacturer code, schema or bindings were copied. [Reference and statistical interpretation](home-live-v0811.md).
