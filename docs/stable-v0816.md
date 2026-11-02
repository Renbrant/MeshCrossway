# Stable firmware v0.8.16

On 2026-11-01, simultaneous connections to both radio networks were confirmed, and v0.8.16 was designated the stable release. Map tiles and saved routes were also reported not to load in this version; the issue remains under investigation.

This historical stable designation was superseded when v0.8.17 was confirmed working well on the device. See the [current stable release record](stable-v0817.md), which addresses the map/route regression.

The Web Flasher pins the exact published image; v0.8.14 remains available as rollback. The stable designation records the release decision and does not imply that the map regression is resolved.

- Version: `v0.8.16`
- Compiled source ref: `1b86b1db5d08a793fca2ee64f1d32e040383119a`
- Binary SHA-256: `a3ac7f83061c8c9932662540a48173432f70fb392ac44e4ee0400d90d130321f`
- Binary size: 2,159,744 bytes
- Original successful Pages deployment run: `41332894632`

The build restores this pinned image even when stable and latest are equal, verifying its source identity and checksum. Dual-radio operation is confirmed; map-tile and route loading remain under investigation.
