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
| WADAMESH | UI/reference research | Current firmware README says the UI is original and does not copy WADAMESH code/assets; future source reuse requires a separate GPL review |
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
