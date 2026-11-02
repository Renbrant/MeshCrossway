# v0.8.17: remote map and track preview reliability

Status: experimental candidate. Map backgrounds and saved-route previews were reported to have stopped loading after v0.8.16. The current remote HTTP server config allowed only two open sockets while the companion page concurrently polls messages, radio status and navigation data and requests route geometry. This creates contention for map/track requests. v0.8.17 raises the bounded HTTP socket capacity to four while retaining the existing 14-route limit and per-request stack guards.

The firmware version is v0.8.17. v0.8.16 remains the approved stable release until this candidate is verified on the device and browser. Map tile downloads from external providers still require working Wi-Fi, valid device time for TLS, SD storage for tile cache and an available provider. Route geometry is read locally from the track store; the browser background tiles are fetched directly by the browser.

This change addresses contention observed in the remote request architecture. The report does not include device logs, so this remains a code-based diagnosis. Check both native map tile loading and remote route geometry/preview after flashing, with both BLE radios connected and while messages/status refresh. If native tile loading still fails, capture the on-screen map status and device log before promoting v0.8.17.

