# Third-party notices

MeshCrossway's [MIT license](LICENSE) covers its original code and documentation. It does not replace the licenses of upstream build inputs, separately distributed radio firmware or third-party assets.

## Initial verified license collection

- **Waveshare ESP32-S3-Touch-LCD-3.5:** [Apache-2.0 text](LICENSES/Waveshare-Apache-2.0.txt), copied unchanged from upstream commit `d9ff7837148b47b33edadc4b87775c341cf35bbe`. The display patcher changes the example's display rotation, initialization/includes and snapshot configuration, and adds MeshCrossway headers. This is a modified upstream example; upstream notices must remain intact.
- **LVGL:** [MIT text](LICENSES/LVGL-8.4.0-MIT.txt), copyright (c) 2021 LVGL Kft, copied unchanged from the `v8.4.0` reference. Verify the exact resolved patch release before preparing a binary notice bundle.
- **XPowersLib and SensorLib:** [MIT text](LICENSES/XPowersLib-SensorLib-MIT.txt), copyright (c) 2022 lewis he. The two license files in the selected Waveshare example at the pinned commit are identical; this copy preserves their text.
- **ESP-IDF:** the build uses v5.3.2. Its root license is Apache-2.0, with component-specific terms to be reviewed individually. See the [upstream license](https://github.com/espressif/esp-idf/blob/v5.3.2/LICENSE).

This is an initial source-level notice collection, **not a complete notice bundle for any published firmware binary**. Review exact resolved components, file-level copyright notices and upstream NOTICE files before redistribution. Track incomplete work in the [dependency review](docs/third-party-licenses.md#open-review-items).

## Emoji artwork (read-only message rendering)

- **Noto Color Emoji** (Google Inc. and contributors): glyphs were rasterized from the locally installed `NotoColorEmoji.ttf`, resized to 20×20 and quantized into 16-color per-glyph images; only derived bitmap data is bundled, **not the font file**. Upstream project: https://github.com/googlefonts/noto-emoji. The Noto Color Emoji font software is licensed under **SIL Open Font License 1.1**. [License and copyright](LICENSES/NotoEmoji-OFL-1.1.txt). Rasterization and palette reduction are MeshCrossway adaptations; this does not claim the bitmaps are original emoji artwork or endorsed by Google.
- The initial set contains **92** sequences (including common multi-codepoint emoji); unrecognized sequences become readable text placeholders in the message display. The original UTF-8 payload is unchanged.

## Independent firmware and assets

Meshtastic and MeshCore native firmware keep their upstream licenses. Communication through BLE does not transfer their ownership or license to MeshCrossway, nor does it remove obligations when distributing those binaries.

Product photographs, vendor CAD, map data and other third-party assets retain their own terms. See [image credits](docs/images/README.md) and [hardware sources](docs/hardware-sources.md). References to third-party names do not imply endorsement or an agreed partnership.


## Meshtastic PhoneAPI research for v0.7.12

The new Meshtastic codec, session, UI and store are original MeshCrossway code. No Meshtastic `.proto`, generated bindings or firmware/client source is linked or redistributed. The GPL-3.0 protocol/firmware sources were consulted only to understand interoperability. Exact reviewed revisions, paths and official documentation are recorded in [Meshtastic integration provenance](docs/meshtastic-v0712.md#primary-references-and-provenance). The user's T-Deck firmware remains independently installed.

## Offline timezone lookup and rules

`firmware/display-prototype/timezone/zone_data.h` contains a C++ adaptation of
[PhotoStructure tz-lookup](https://github.com/photostructure/tz-lookup), derived
from Dark Sky's implementation. Code and compact lookup data are offered under
CC0-1.0; see `LICENSES/tz-lookup-CC0-1.0.txt`. Geographic input originates from
[timezone-boundary-builder](https://github.com/evansiroky/timezone-boundary-builder)
and © OpenStreetMap contributors (ODbL). The generated compact geographic table
and regeneration script are distributed in source form here. Attribution:
https://www.openstreetmap.org/copyright and https://opendatacommons.org/licenses/odbl/1-0/.

IANA TZ Database recurring POSIX rules were extracted from the official Python
`tzdata` wheel, version and hashes recorded in the generated header. IANA timezone
data is public domain; no country outline or timezone rule is original
MeshCrossway data. Geographic lookup is approximate, as documented upstream.

## v0.8.10 remote companion

The offline dashboard and mailbox are original MeshCrossway code; the unchanged logo comes from `docs/site/assets/meshcrossway-logo.svg`. No new firmware dependencies are added. SHA-256 uses the existing ESP-IDF mbedTLS component. The host-only `tests/remote_chat_test.cpp` links system OpenSSL ([source and license](https://github.com/openssl/openssl/tree/openssl-3.0.13), Apache-2.0 in OpenSSL 3.x), tested locally with OpenSSL 3.0.13. CI uses its runner-provided version; no OpenSSL source or binary is copied into firmware or distributed by this change. Runtime release dependency/license review items remain open.

## v0.8.12 browser map and folders

The folder/geometry/viewer code is original MeshCrossway code. No map JavaScript library, copied schema or new firmware dependency is incorporated. OpenStreetMap raster tiles are fetched directly by the browser for visible interactive views, attributed in the map, and governed by the [OSMF Tile Usage Policy](https://operations.osmfoundation.org/policies/tiles/). Browser tests intercept tile requests and serve synthetic fixtures; they do not contact the tile service. Existing dependency and binary-release review items remain open.
