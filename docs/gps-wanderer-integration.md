# GPS/GNSS, tracks and wanderer integration

Decision date: 2026-10-28.

**Decision:** select [wanderer](https://wanderer.to/) as the open-source application/service base for the connected track library and sharing experience. MeshCrossway will integrate through its documented API and open file formats. Local GNSS acquisition, LVGL rendering, track recording and microSD storage remain MeshCrossway Core responsibilities.

**Status:** approved product direction and source/documentation study, not implemented or hardware-validated. No wanderer server has been deployed for MeshCrossway, no customer accounts have been provisioned, and no partnership or support agreement exists. This document does not claim working GPS, synchronization or backup firmware.

## Scope: no automatic route calculation

Include:
- Core-side GPS/GNSS position, fix validity, timestamp and accuracy when available.
- Offline regional maps, subject to renderer, storage and map-data license validation.
- Create and manage POIs/waypoints.
- Start, pause, resume and stop local track recording; retain recorded tracks on microSD.
- Import and export GPX; use GeoJSON where appropriate for geographic features.
- Display an imported or manually drawn course and recorded history.
- Connected library organization, metadata, selected file synchronization and controlled sharing through wanderer.
- User-controlled personal export/backup and restoration.

**Deferred:** automatic routing, road/trail snapping, route optimization, turn-by-turn instruction generation and automatic recalculation. Disable wanderer's automatic routing in the MeshCrossway workflow; its documentation describes straight-line segments between anchors when Valhalla routing is disabled. Manual segments do not establish that terrain is passable. Existing upstream features are not automatically MeshCrossway commitments.

See [navigation and radio sharing requirements](navigation-and-sharing.md). Geographic autorouting is unrelated to the existing LoRa packet-routing policies.

## Research performed and selection rationale

The study examined official repositories, API documentation, endpoint implementations, installation configuration and release information. It was a source/documentation review, not an executed interoperability, performance or security audit. API existence alone does not prove ESP32 compatibility.

| Candidate | Evidence and fit | Decision |
|---|---|---|
| [wanderer](https://github.com/open-wanderer/wanderer) | Self-hosted track library, documented REST/OpenAPI, GPX upload, waypoints, lists, user/link sharing and manual drawing without autorouting | Selected connected application/service base, subject to integration acceptance tests |
| [Traccar](https://github.com/traccar/traccar) / [web client](https://github.com/traccar/traccar-web) | REST/OpenAPI and WebSocket; strong device/group position history; position ingestion uses tracking protocols, not simply its position-query API | Reserve for a future dedicated team/device tracking requirement |
| [uMap](https://github.com/umap-project/umap) | Strong custom maps, layers, markers and polygons; application endpoints and embedding found, but no equally clear external API contract established in this review | Possible custom-map complement |
| [PhoneTrack](https://github.com/julien-nc/phonetrack) / [GpxPod](https://github.com/julien-nc/gpxpod) | Position ingestion, sessions, GPX and file visualization; requires Nextcloud operation and compatibility management | Alternative if Nextcloud becomes an explicit requirement |
| [gpx.studio](https://github.com/gpxstudio/gpx.studio) | Open-source GPX editor/viewer; documented URL/iframe embedding, rather than an equivalent hosted account/library backend API | Optional editor/viewer |
| [OwnTracks Recorder](https://github.com/owntracks/recorder) | HTTP/MQTT ingestion and history API; additional access-control and product layers needed | Not the primary library/sharing base |
| [OsmAnd](https://github.com/osmandapp/OsmAnd) / [API demo](https://github.com/osmandapp/osmand-api-demo) | API controls an installed Android application, including recording and GPX exchange | Optional mobile interoperability |
| [Locus API](https://github.com/asamm/locus-api) | Open integration library, but not the full application; requires Locus installed | Does not satisfy the fully open-source service-base requirement |

The broader screening also considered [OpenTracks](https://codeberg.org/OpenTracksApp/OpenTracks), [GPSLogger](https://github.com/mendhak/gpslogger) and [MapStore](https://github.com/geosolutions-it/MapStore2). Mobile recorders and a broader GIS portal did not offer a better match to this track-library requirement. OpenTracks' archived GitHub repository points to Codeberg; archival alone was not treated as abandonment.

wanderer was selected because it combines a usable application, a documented integration surface, self-hosting, open exports and existing sharing workflows. It reduces the amount of account/library/sharing software MeshCrossway must build. It is not a map-data provider or an ESP32/LVGL rendering engine, and does not remove the need for local firmware.

Reviewed wanderer baseline: [v0.21.2](https://github.com/open-wanderer/wanderer/releases/tag/v0.21.2). Pin the exact tested server/API version during implementation rather than tracking the development branch. Mobile applications were in public beta at review time and are optional, not a V1 prerequisite.

## Accounts and permanent direct-web option

**The user must create or already hold an account on the chosen wanderer instance to save private data there, synchronize their library and manage online sharing.** This is a separate service account from any MeshCrossway account; do not promise automatic account linking or shared credentials. A MeshCrossway cloud account is not currently implemented.

wanderer is decentralized/self-hosted: there is no assumed universal account at wanderer.to. The [official project site](https://wanderer.to/) provides project information and instance discovery; the connected account belongs to a particular deployment. Its demo must not be advertised as durable production storage.

Planned connection flow:
1. Explain which functions are local and which send data to a server.
2. Let the user choose a supported wanderer instance and display its operator/address.
3. Offer **Create account / Open wanderer website**, opening that instance's registration/login page. Registration availability depends on the operator.
4. Authenticate using the supported, validated API mechanism; never request administrator access.
5. Ask which local tracks to upload and apply an explicit visibility choice.
6. Allow disconnecting/revoking integration access without deleting local tracks.

**Always retain a user-facing option to open and use the selected wanderer website directly.** On the companion web/mobile interface this opens the instance in a browser; the standalone Core can display the address/QR for an external browser. Also expose the official project site for information. This is a product-access requirement, not a guarantee that a remote website will always be online.

Users may manage/import/export/share their library through wanderer's own web UI without being locked into MeshCrossway. The web UI itself does not record live GPS tracks; recording is performed by the Core or an appropriate mobile application. Do not promise that the browser alone provides every navigation function.

**No wanderer account is required for local GNSS, local recording, local GPX management or personal microSD copies.** Without a connected service account, online library synchronization and account-based sharing remain unavailable. Public read-only links, where permitted by the host, are a separate access case.

## Architecture and integration boundary

| Component | Planned responsibility |
|---|---|
| Core: Waveshare ESP32-S3, GNSS and microSD | Local position, tracks, POIs, offline display and recovery |
| Core radio adapters | Two independent BLE sessions: Meshtastic PhoneAPI and MeshCore Companion; consent and airtime limits |
| MeshCrossway companion/adapter | Configurable instance URL, authentication, bounded transfer queues, identity mapping, integrity and interrupted-transfer recovery |
| wanderer instance | Account-backed track library, metadata, lists, web access and sharing |
| Hosting operator | Server availability, access control, updates, backups and recovery |

The connected adapter's execution location (Core directly versus companion/backend) remains an implementation decision to validate against ESP32 memory, TLS, authentication, Wi-Fi/BLE coexistence and upload limits. The complete wanderer server is not installed on the ESP32. Cloud availability must not gate local recording.

Full tracks and map packages use microSD or authenticated local bulk transfer over Wi-Fi/BLE as validated. LoRa is reserved for protocol-supported compact positions, POIs and bounded summaries. Never stream full GPX files over LoRa by default or bridge the networks automatically.

API areas reviewed include /api/v1/trail, /api/v1/trail/upload, /api/v1/waypoint, /api/v1/trail-share and /api/v1/trail-link-share. Authentication, permissions and download behavior must be verified against the selected release. A concrete finding: the v0.21.2 upload handler expects multipart/form-data with a file field, while a simplified documentation example showed a raw GPX body. Test the actual contract; do not copy that example blindly.

## Data ownership, storage and personal SD backups

In the default self-hosted deployment, PocketBase's pb_data holds the authoritative database and uploaded files; GPX/images are in its storage subfolder. Meilisearch holds a rebuildable search index. Data sent to a third-party instance is held by that operator. Hosting selection for MeshCrossway production remains open; no central service is promised.

Required personal-copy behavior:
- Record to microSD first; persist incrementally and recover after interruption.
- Synchronization must not remove the original local track.
- Provide **Back up my data to SD** for the authenticated user's own selected library, including server-created/imported items not yet on the device.
- Export GPX/GeoJSON plus a versioned metadata manifest; include notes, waypoint relationships, timestamps, identifiers and optional attachments as supported.
- Distinguish a portable personal export from complete account/server restoration. Inventory anything not represented in GPX, including sharing settings.
- Show last successful backup, item count, integrity results, available space and any omissions/failures.
- Define retention/versioning so synchronized deletion does not silently destroy all recovery copies.
- Exclude passwords, API tokens, other users' private data and administrator databases from personal exports.
- Offer a second independent copy to a computer or user-chosen destination; one SD holding the only original is not a backup.
- Validate restore/reimport into a clean supported instance and local access without wanderer.

wanderer already documents per-trail/all-trail ZIP export with GPX or GeoJSON and optional photos/logs. Automated per-user SD backup is a MeshCrossway requirement still to implement, not an existing verified firmware feature. Do not assume the UI export workflow is a single public API operation.

Server-wide recovery is separate: the operator backs up PocketBase and the necessary configuration/secrets securely, retains software/deployment versions, and tests restoration. Never distribute the multi-user server database to an individual customer's SD. Official restore guidance requires compatible minor versions.

If upstream maintainers stop development, a retained self-hosted deployment and its data do not disappear automatically. Maintenance, hosting outages, deletion and media failure still require active management. Preserve the tested upstream source, deployment configuration and required build/images alongside backups. Neither open source nor an SD card guarantees indefinite availability.

## License and relationship

wanderer is AGPLv3; MeshCrossway originals remain under the repository's MIT license. This documentation does not incorporate wanderer code or install a dependency. API interoperability is the intended boundary, not a blanket exemption from license review.

The license permits compliant use, including commercial use, without a bespoke partnership. Modified AGPL network services and redistributed components have applicable corresponding-source/notice obligations. Review the concrete implementation, SDKs, copied code and assets before distribution; follow the [licensing policy](licensing.md).

Using a third-party hosted API additionally requires that operator's access and terms. An official partnership, branding endorsement, support commitment or availability guarantee requires a separate agreement. No such agreement or maintainer contact is claimed here.

## Acceptance gates

1. Validate real GNSS/microSD recording, screen-off operation and power-loss recovery on the Core.
2. Configure a test instance/account; verify private upload, list/download and waypoint round trips without autorouting.
3. Test sharing with a second user, permission revocation, instance separation and explicit visibility.
4. Verify the direct-web option and account onboarding; disconnected users retain all local functions.
5. Interrupt and repeat transfers; detect corruption, duplicates, expired authentication and full storage.
6. Export a personal SD copy, restore into a clean compatible instance, and list any metadata that cannot be recovered.
7. Stop the server and verify continued local recording and GPX access; exercise server disaster recovery separately.
8. Pin the passing version and document licensing, map-source rights, remaining failures and physical-test evidence.

## Primary sources

Reviewed 2026-10-28; upstream behavior may change.

- [wanderer repository and license](https://github.com/open-wanderer/wanderer)
- [REST API guide](https://wanderer.to/develop/api/) and [OpenAPI reference](https://wanderer.to/api-reference/)
- [Create/edit trails, including disabling Valhalla](https://wanderer.to/use/create-a-trail/)
- [Sharing](https://wanderer.to/use/share-trails/)
- [Authentication](https://wanderer.to/use/authentication/)
- [Import/export](https://wanderer.to/use/import-export/)
- [Application/web feature boundaries](https://wanderer.to/app/what-works-where/)
- [Server backups and restore constraints](https://wanderer.to/run/backend-configuration/backup-server)
- [Docker deployment](https://wanderer.to/run/installation/docker)
- [GNU licensing FAQ](https://www.gnu.org/licenses/gpl-faq.html.en)
