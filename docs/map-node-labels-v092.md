# Map node labels and clustering — v0.9.2 candidate

## Changes

- Add a per-network **Name** marker mode that displays the last four hexadecimal digits of each node identifier.
- Open node details by tapping its marker; tapping a cluster lists its member nodes.
- Group nearby nodes into map clusters to reduce label overlap.
- Add **Small (8 px)** and **Medium (12 px)** marker sizes, retaining existing sizes.
- In Name mode, use green for heard/advertised within 5 minutes, cyan within 60 minutes, and gray for older records or missing timestamps. These timestamps are an activity estimate and do not verify repeater health.
- Migrate saved map preferences to the new packed configuration while preserving the existing NVS blob layout.

The v0.8.26 firmware remains the recommended stable release. v0.9.2 is an experimental candidate pending firmware build and device validation.
