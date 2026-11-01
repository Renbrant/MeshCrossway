# v0.8.15: native message visibility and device-local timestamps

The owner reported seeing some messages in remote access that were not visible on the device, and requested local message times matching the device. No message ID, protocol or reproducing log was supplied for the specific missing messages, so the individual cause is not yet established.

Inspection found two native-only visibility paths worth correcting:

- MeshCore's conversation renderer inferred message presence from next_id and message_count, while the remote view enumerates stored entries. Meshtastic additionally inferred the physical ring slot from each sequence. Native rendering now enumerates actual occupied entries in chronological ID order, using a bounded PSRAM index array. Sparse or rearranged stored entries cannot be omitted merely because of counters or array position.
- MeshCore's list search could select a thread by its name, then silently exclude messages that did not themselves contain that search text. Selecting a conversation now clears the list search.

Native and remote Meshtastic views share one destination/channel-identity predicate. Radio and protocol identity boundaries remain enforced. Read flags do not remove messages; native renderers enumerate incoming read and unread messages alike. Device-side acceptance with the reported conversation remains necessary.

## Time display

Previously native cards explicitly formatted UTC; the browser independently used the computer's local timezone. Both now format against the device's existing TZ configuration, which the GPS clock service chooses automatically or through its manual override.

MeshCore uses the original message epoch when valid, falling back to its observed epoch if unavailable. Meshtastic uses its existing message epoch. Timestamps in packets, protocol models and SD archives remain unchanged. Unknown times display --:--. The timezone rule is evaluated at each message's date, including DST and date rollover.

Native cards show local HH:MM. Remote JSON retains the numeric epoch and adds local_time (date, HH:MM and zone abbreviation); the browser displays that supplied string. Existing cards refresh when the device timezone changes, without requiring a new message. When the device has no known zone, its existing clock fallback applies; no geographic zone is guessed by the browser.

## Validation

Pure helpers are checked under C++11/C++20 for Denver winter/summer, date rollover, half-hour offsets, invalid times and sparse/shuffled entry ordering. Real LVGL tests exercise native sparse history, clearing stale search, timezone redraw, both protocol UIs and retained cards/scroll positions. Remote production-model tests check device-local MC/MT strings and unchanged destination isolation. Full target build and all stack/catalog guards remain required, followed by hardware acceptance.

The exact v0.8.14 stable binary is preserved; v0.8.15 is an experimental candidate.
