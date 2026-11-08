# Android functional reference matrix

This increment uses public product documentation to identify useful workflows. Implementations are original MeshCrossway code; no client source, layouts or branding were copied.

| Reference | Workflow studied | MeshCrossway adaptation |
| --- | --- | --- |
| [Meshtastic Android](https://meshtastic.org/docs/software/android/user/) | Connection/pairing, node information, maps, waypoints and notifications | Explicit setup phases, node favorites/details, local map tools and network-scoped alerts |
| [MeshCore](https://meshcore.co.uk/) / [user guide](https://meshcore.co.uk/user-guide.html) | Companion contacts, channels, messaging and map | Preserve independent contact/channel identity and current-radio validation in combined views |
| [Cairn companion](https://www.cvhvisuals.com/cairn.html) | Unified inbox, favorites, map/offline use and phone location | Pinned inbox, local GPS and named offline areas without automatic location broadcasts |
| [Wadamesh](https://wadamesh.com/docs.html) | Dashboard, unread messages, quick replies, map and contacts | Compact two-network overview, unread filters and editable quick-reply drafts |

The defining behavior is simultaneous Meshtastic and MeshCore sessions. Combined screens never merge same-named channels or create a forwarding path. Every send resolves one network, one radio and one conversation.

## Mapping dependencies and provider

[MapLibre Native Android](https://maplibre.org/maplibre-native/android/examples/getting-started/) 11.13.5 supplies the native renderer and offline-region database. Its BSD license is bundled in app assets and accessible from Settings. Transitive AndroidX, Kotlin, OkHttp, Okio, Gson, GeoJSON and annotation dependencies retain upstream licenses.

[OpenFreeMap setup](https://openfreemap.org/quick_start/) and [terms](https://openfreemap.org/tos/) were reviewed for online and offline maps. The provider permits limited offline downloads (3,000 tiles or 50 MB per download; four concurrent requests; identifying user agent). MeshCrossway applies smaller tile/byte budgets and downloads one region at a time. It does not bulk-download the standard OpenStreetMap raster service. Data attribution remains visible: OpenFreeMap, OpenMapTiles and © OpenStreetMap contributors ([ODbL](https://www.openstreetmap.org/copyright)).

Offline regions depend on provider availability and may be incomplete after a pause/error. Users can inspect region bounds and status before relying on them. Download selection is deliberately bounded; this is not country-scale map packaging.

## Acceptance scope

Alpha02 is the user-tested S25/two-Heltec baseline. Alpha03 adds independently testable field tools and setup changes; hardware coexistence, pairing, real map downloads, location permission behavior and background endurance still require device acceptance. Tracks, remote administration and radio configuration are outside this increment.
