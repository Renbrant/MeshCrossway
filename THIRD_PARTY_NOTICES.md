# Third-party notices

MeshCrossway's [MIT license](LICENSE) covers its original code and documentation. It does not replace the licenses of upstream build inputs, separately distributed radio firmware or third-party assets.

## Initial verified license collection

- **Waveshare ESP32-S3-Touch-LCD-3.5:** [Apache-2.0 text](LICENSES/Waveshare-Apache-2.0.txt), copied unchanged from upstream commit `d9ff7837148b47b33edadc4b87775c341cf35bbe`. The display patcher changes the example's display rotation, initialization/includes and snapshot configuration, and adds MeshCrossway headers. This is a modified upstream example; upstream notices must remain intact.
- **LVGL:** [MIT text](LICENSES/LVGL-8.4.0-MIT.txt), copyright (c) 2021 LVGL Kft, copied unchanged from the `v8.4.0` reference. Verify the exact resolved patch release before preparing a binary notice bundle.
- **XPowersLib and SensorLib:** [MIT text](LICENSES/XPowersLib-SensorLib-MIT.txt), copyright (c) 2022 lewis he. The two license files in the selected Waveshare example at the pinned commit are identical; this copy preserves their text.
- **ESP-IDF:** the build uses v5.3.2. Its root license is Apache-2.0, with component-specific terms to be reviewed individually. See the [upstream license](https://github.com/espressif/esp-idf/blob/v5.3.2/LICENSE).

This is an initial source-level notice collection, **not a complete notice bundle for any published firmware binary**. Review exact resolved components, file-level copyright notices and upstream NOTICE files before redistribution. Track incomplete work in the [dependency review](docs/third-party-licenses.md#open-review-items).

## Independent firmware and assets

Meshtastic and MeshCore native firmware keep their upstream licenses. Communication through BLE does not transfer their ownership or license to MeshCrossway, nor does it remove obligations when distributing those binaries.

Product photographs, vendor CAD, map data and other third-party assets retain their own terms. See [image credits](docs/images/README.md) and [hardware sources](docs/hardware-sources.md). References to third-party names do not imply endorsement or an agreed partnership.
