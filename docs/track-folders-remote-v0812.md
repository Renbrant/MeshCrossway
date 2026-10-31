# Track folders and remote workspace — v0.8.12 candidate

## Track organization

Create, rename and remove up to 16 flat folders, and move tracks between them or **Unfiled**, on both the physical Tracks screen and remote page. Removing a folder keeps its tracks in Unfiled; deleting an actual track still requires separate confirmation. New recordings start Unfiled and may be moved while recording.

Folders are persistent logical organization, stored separately in a versioned, checksummed `folders.bin` on SD. Track journals, GPX paths, original sample data and the existing `preferences.bin` format are not moved or migrated. Old tracks appear in Unfiled. Changes use a flushed temporary file with commit/rollback; bulk catalog scratch data stays in the Store's PSRAM allocation, not on the navigation stack. A valid backup can recover metadata; unrecoverable metadata is not silently overwritten. The existing 32-track loaded catalog limit remains; folder assignments are bounded to 128 records.

## Browser route preview

The Tracks page has folder/search/route selection on the left and an interactive map on the right. Selecting a completed route fits the viewport and draws its color, with endpoints and native recording segment gaps preserved. Drag to pan, use the wheel or buttons to zoom, and Fit route to restore the extent. Selection in the browser does not change visibility on the physical device; that has its own explicit checkbox.

The geometry endpoint reads only numeric, known, completed track IDs from immutable journal data. It rejects active recordings, deleted tracks, malformed IDs, bad headers/sample checksums and files over the existing 16 MiB journal bound. It samples at most 1,024 valid points, including first and last, into PSRAM; browser text identifies a simplified preview. GPX downloads retain all points. Raw timestamps and message/radio data are not part of map requests. Antimeridian wrapping is handled by the original browser projection code.

The map viewer JavaScript is embedded in firmware and served locally. OpenStreetMap background tiles load directly from the browser, only for the visible viewport, using its normal cache and Referer; no download/prefetch feature or external JS library is added. Attribution stays visible. Without Internet or with the background switched off, the local route still displays on a geographic grid. Tile requests disclose the visible map area to the tile provider. See the [official tile policy](https://operations.osmfoundation.org/policies/tiles/).

## Remote communication

MeshCore/Meshtastic remain separate network selectors. Each has independent **Conversations**, **Groups**, **Nodes** and **Network** views. Conversations show destinations with loaded messages; groups show native channels/rooms; nodes show native node/contact entries. Meshtastic nodes are listed once under the primary (or first available) channel, while conversation history preserves native secondary-channel DMs. Selected native IDs and last-heard metadata appear beside the conversation; unread counts exclude outgoing/read messages. Viewing a conversation marks only the displayed incoming messages read, on the native owner task, allowing Home counters and SD history to follow. Selection/drafts remain separate by network and category.

Network controls enable Bluetooth, scan, connect, disconnect and complete native PIN/numeric-confirmation pairing. Only one BLE radio is active. The user must disconnect to change radios; native synchronization, TX limits, region/PKI/channel/room guards still apply. The page does not invent unsupported RF settings or bridge messages between networks.

HTTP radio commands execute through a bounded owner-task mailbox. Per-boot peer handles bind the actual advertised address, so a reordered scan list cannot connect another index by accident. PIN/confirmation actions require the current pairing state; submitted PIN fields are cleared and never logged. State-changing HTTP actions require the existing same-origin application header and are not automatically retried. Use the existing trusted-local-network access model.

## Home IP and validation

The Home Wi-Fi card shows SSID, the actual connected station IP (`IP: --` when disconnected), and connection state. Its one-line name may be ellipsized to reserve an unclipped IP line. No password is read or displayed.

Host checks cover folder compatibility/restart/rollback/metadata recovery, geometry IDs/segments/limits, HTTP commands and active/deleted preview rejection, actual LVGL folder editors and movement, Home IP and idle redraw, separate communication categories and native send boundaries, radio identity/scan/PIN/confirmation/timeout gates. Browser tests mock all map requests (no automated calls to OSM) and cover desktop/mobile folders/maps, offline backgrounds, categories, replies, remote pairing, recording and captures. Full ESP-IDF stack-budget/catalog/version checks and physical acceptance remain required. The explicitly designated v0.8.9 stable image is unchanged.
