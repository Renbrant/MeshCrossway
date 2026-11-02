# v0.8.17: remote map and track preview reliability

Status: stable. The v0.8.16 browser companion and device reported map/route loading failures. The v0.8.17 remote HTTP server increases bounded socket capacity from two to four to reduce contention between navigation, message and radio refresh requests. Existing endpoint and stack limits remain in place.

The diagnosis was based on request architecture because no failure logs were supplied. After installing v0.8.17, the user confirmed the version is working very well and selected it as the recommended stable release. The exact published binary is pinned in [the stable release record](stable-v0817.md).

The fix applies to remote route and map requests. Native map tile availability still depends on Wi-Fi, valid device time for TLS, microSD tile cache and the selected map provider.
