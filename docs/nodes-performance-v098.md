# v0.9.8 node and channel responsiveness

v0.9.8 is an experimental firmware candidate. The user-selected stable release remains v0.8.26.

## What changed

The v0.9.7 Nodes screen assembled the full result set in one LVGL render call. The MeshCore all-networks list also used insertion sorting while rescanning the 96-message ring for each comparison. With hundreds of saved contacts, that synchronous work could block the LVGL task and trigger the task watchdog, as seen in the supplied device logs.

v0.9.8 changes node ordering to quicksort and appends list rows in batches of 12 through LVGL timers. This lets the interface return to its event loop between batches. Node cards now put the node name in a narrow left column and heard time, unread messages, recent direct chat count, logged text bytes and distance in a compact right column.

The Groups / Channels screens now summarize unread direct conversations and unread channel messages separately. Each channel continues to show its own unread count, so a network-wide unread total from direct messages is not mistaken for unread channel traffic.

## Scope of the counters

Per-node message and byte totals still describe recent direct text messages retained in the local 96-message ring. They do not measure all packets or bytes relayed over RF.

## Verification

The firmware preflight and ESP32-S3 archive build run on the release pull request. Hardware validation remains necessary to confirm the screen timing and unread counts against the user's radio data.
