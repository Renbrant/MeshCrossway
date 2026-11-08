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

Alpha04 is the hardware-accepted S25/two-Heltec functional recovery reference. Alpha05 adds tracks and field usability as described below; real GPS recording with screen off, simultaneous radio use and endurance still require device acceptance. Remote administration and radio configuration remain future work.

## Alpha05 field workflow increment (November 8, 2026)

The alpha04 tested APK remains the recovery reference. Public documentation was
reviewed again for all five references, including MeshMapper:

| Reference | Additional workflow reviewed | Alpha05 implementation / boundary |
| --- | --- | --- |
| [Meshtastic Android](https://github.com/meshtastic/Meshtastic-Android) | Node details, maps and waypoint workflows | Coordinates in chat open on the local map; saved-target distance and true-north bearing |
| [MeshCore companion protocol](https://github.com/meshcore-dev/MeshCore/blob/main/docs/companion_protocol.md) | Typed client/repeater/room contacts and channel identity | Type filters and readable contact roles; channel activity/unread remains scoped to the originating network, radio and topic |
| [CAIRN](https://www.cvhvisuals.com/cairn.html) | GPS dashboard, compass-to-node, field map workflows | Phone position/altitude/speed card and distance/bearing sheets; no claims of copied compass or RF diagnostics |
| [Wadamesh guide](https://wadamesh.com/docs.html) | Editable quick replies, coordinates in chat and map tools | Saved quick-reply templates, append-to-draft, channel unread/activity order and local map shortcuts |
| [MeshMapper overview](https://wiki.meshmapper.net/app_overview/) | Moving field sessions, map history and network-specific coverage | Local phone-GPS track sessions, pause/resume, colored track overlays and GPX export; these are travel tracks, not RF coverage measurements or MeshMapper uploads |

Track recording is an independent location foreground service. It never receives
or sends mesh commands. Screen-off operation is implemented using Android's
[location service requirements](https://developer.android.com/develop/background-work/services/fgs/service-types#location),
with explicit foreground start, precise-location permission and an ongoing
notification. Force-stop, process termination and device restart do not restart
recording automatically. On reopening, interrupted recordings are paused and
require explicit resume; resume starts a new GPX segment.

No reference application source/assets were copied. Remote administration,
active wardriving/pings, traceroute, coverage uploads and radio settings remain
separate future work; none are silently enabled by track recording.
