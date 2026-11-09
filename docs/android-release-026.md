# MeshCrossway Android 0.2.6 — Message actions and official icon (beta)

**Public beta; not version 1.0.0.** This APK is signed with the **same permanent release certificate** used for 0.2.3–0.2.5. Install **directly over 0.2.5** and choose **Update**; **do not uninstall or clear app storage**, which would erase locally saved conversations, settings, favorites, tracks, and offline maps. Version code increases from 15 to 16.

## Changes since 0.2.5

- **Chat sender cleanup:** received MeshCore channel messages no longer repeat the sender name in the message text when it already appears as a heading. The original message remains unchanged in history/backups.
- **Long-press message options:** mention sender, prepare a reception/ACK reply, quote, copy text, copy available reception metadata, view technical details, share via Android, show message coordinates on the map, and add/remove a uniquely matched sender as a favorite.
- **Receiving information:** SNR, LoRa RSSI, observed hops/repeater hashes are included only where the radio actually supplied them. An ACK reply is **editable plain text**, not a protocol-level delivery acknowledgement. No automatic send.
- **Official MeshCrossway branding:** orange mountain app icon on dark background, matching Home navigation symbol, notification icon, and local-browser favicon/header.
- **Existing capabilities kept:** separate Meshtastic and MeshCore BLE connections, independent chats/channels/nodes, maps, offline areas, tracks, customizable appearance and local HTTPS Remote Access. **No bridge between mesh networks.**

## Signing and verification

The release pipeline checks protocol/browser fixtures, Android unit tests and lint, builds a release APK, verifies the expected package ID and version, and **refuses publication** if the permanent certificate SHA-256 changes. A checksum (`SHA256SUMS.txt`) and signing certificate fingerprint (`SIGNING-CERT-SHA256.txt`) accompany the APK.

**Field validation still recommended:** test long-press behavior on actual phone, both BLE radios, and saved drafts/favorites before depending on it for emergency communications. This is a beta and is not certified for safety-critical use.
