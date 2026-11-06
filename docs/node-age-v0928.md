# v0.9.28: map colors by last reception and configurable local retention

v0.9.28 was selected as stable on 2026-11-05. Its exact published bytes are retained; v0.9.25 remains previous stable recovery. See the [stable acceptance record](stable-v0928.md).

## Controls

Open Map Options (the sliders button). In each network section, Color gains **By last heard / age**. It is independent for Meshtastic and MeshCore, and works with circles, squares, crosses and name badges. Fixed colors and Auto / network color remain available. Existing selections migrate unchanged.

- Green: less than 24 hours since the last reception.
- Yellow: at least 24 hours and less than 7 days.
- Gray: at least 7 days, or an unknown/invalid/future timestamp.

The new **NODE RETENTION / BOTH NETWORKS** section has **Keep days (0 = off)**, adjustable from 0 to 365 with +/- controls. The default is 30 days; choose 7 to expire ordinary discoveries older than seven days. Exactly the configured age remains eligible; expiry occurs after that boundary. Colors continue to use the same fixed age bands regardless of the chosen expiry limit. Protected older nodes can remain gray.

## Local cleanup and protection

The policy removes expired ordinary discovery records from MeshCrossway's local working sets and subsequent SD snapshots. Radio firmware databases are unchanged. Import filtering applies to Meshtastic NodeInfo and initialized MeshCore contact synchronization, so old radio records do not continually reappear. Importing or restoring an entry does not refresh its observation time.

Preserve attached radio identities, radio favorites, app-local MeshCore favorites/drafts and contacts referenced by retained messages/drafts. Conversation messages are never deleted by this policy. Missing, future and invalid timestamps are kept as unknown rather than guessed old. A valid wall clock is required for expiry.

The Nodes page retains its bounded 32-row manual-refresh behavior. It now uses the configured retention policy instead of the former fixed 14-day visibility cutoff. Maps filter at overlay construction. Physical compaction runs at most once per minute when idle on Home or Map, after SD restore, outside gestures, dialogs, radio requests and contact synchronization. No index-based list/editor is compacted while open. Existing storage workers persist the changed generation; there is no new task or GUI-thread SD I/O. Color invalidation runs at most once per minute on an active map.

MeshCore colors/expiry use advert reception time, not contact modification time. Actual advert notifications refresh known contacts. When an expired contact advertises again, a bounded, rate-limited read-only single-contact lookup can restore its details; new full adverts are marked with the valid local reception time.

## Storage and compatibility

Map settings schema 6 preserves schema 1-5 styles, sizes, grouping, provider and night mode. New age-mode bits and the retention value are saved in NVS.

Meshtastic archive schema 2 appends a 128-byte favorite flag table without changing the legacy prefix. New snapshots use `mt2-<radio>.bin/.bak/.tmp`; the original `mt-<radio>` archives are retained for stable-firmware recovery. Load tries the new files before the legacy files. Legacy archives cannot prove favorite status, so their metadata is protected as unknown while offline; a complete fresh radio synchronization resolves it. Real radio favorites survive expiry and restart. Rolling back to v0.9.25 can still read the preserved pre-upgrade legacy history; newer candidate-only MT history remains in the separate mt2 files.

The fixed-capacity client/map arrays remain allocated. Pruning reduces populated counts, iteration and stale UI clutter; it does not itself release those reserved arrays. Do not interpret this feature as a measured reduction in internal-RAM usage. The new favorite metadata resides with the existing PSRAM-owned MT state/archive buffers.

## Validation and hardware procedure

Host tests exercise exact color and expiry boundaries, zero/future/invalid time, disabled retention, both protocols, protected identities/favorites/messages/drafts, stale import rejection, fresh adverts and legacy/current MT archive recovery. Real LVGL tests exercise both age selectors, all four formats, day increment/decrement, NVS persistence and schema 1-5 migration. Existing radio, history and map suites remain required.

On hardware:

1. Install without erasing settings. Confirm existing shapes, sizes, fixed colors, maps, radio pairing and history.
2. Select By last heard / age for each network; verify known ages and readable icons in street/night modes.
3. Choose 7 days, close settings and leave Home/Map idle for approximately a minute. Confirm stale ordinary records disappear but favorites and conversations remain. Refresh Nodes manually.
4. Reboot and reconnect both radios; verify persisted choices and no stale re-import. Reintroduce a previously expired node with a real advert/message and verify rediscovery.
5. Test 0 (disabled), 1, 7, 30 and 365 days, clock unavailable/future data and no SD. Confirm messages are preserved and SD errors retain RAM data.
6. Collect console memory/latency diagnostics during maps, reconnection, cleanup and SD persistence; record results in issue #137. Expected cleanup log: `[MCX-NODES] expired local records ...`.

The release has been accepted as stable. Continue recording the hardware checks above and measured runtime evidence in issue #137; acceptance does not establish every radio, SD card or operating condition.
