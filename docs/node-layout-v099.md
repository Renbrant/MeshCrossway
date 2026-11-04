# v0.9.9 node and channel layout

v0.9.9 is an experimental firmware candidate. The user-selected stable release remains v0.8.26.

The v0.9.8 list rows could overlap when names contained line breaks or when the default centered button title occupied the same vertical space as the activity details. This affected both MeshTastic and MeshCore node rows and Groups / Channels.

v0.9.9 normalizes line breaks in node and channel names, truncates long names to one line, and aligns unread counts and activity in a separate column beside the name. The column widths adapt to the available content width when the side panels are expanded. Channel rows use a single compact activity line so unread counts stay visible without a second line colliding with the title.

Sorting by distance and last heard is unchanged. The v0.9.8 row batching and sorting improvements are retained.

## Verification

The release pull request runs the firmware preflight and ESP32-S3 archive build. Hardware review is still needed to confirm spacing with the user's display and live radio data.
