# Channel management — Android 0.3.12 beta

Open **Channels → Manage Meshtastic channels** or **Manage MeshCore channels**. The same managers are available under each radio's Settings. Connect and synchronize the corresponding radio before editing.

| Action | Meshtastic | MeshCore |
|---|---|---|
| Create | Private secondary with random AES-256 key; public/default or no-encryption key choices | Private, hashtag or Public |
| Edit | Name, encryption key, MQTT uplink/downlink; retain other supported channel fields | Name and key; region scope checked during import |
| Leave | Disable a secondary slot; slot 0 stays primary | Clear the channel slot |
| Share | Official ChannelSet URL and QR image | Official meshcore://channel/add URL and QR image |
| Receive | Paste/open an official link, scan with camera or read a saved QR image | Same three paths |

## Share or join

Tap an enabled channel in its manager and choose **Share link or QR Code** (MeshCore: **Share channel link**). Confirm sharing the key, then use **Share link** or **Share QR image**. These contain the channel secret; anyone receiving them can join. QR images are generated locally and shared only using the destination you choose.

To join, choose **Import channel link**, **Scan QR Code** or **Read QR Code from image**. Camera permission is requested only for scanning; links and image imports do not require camera access. Opening a supported link in another app can route it to MeshCrossway. Android may require you to choose MeshCrossway as the handler for meshtastic.org links.

The app always shows a review before **Save to radio**. Reading a QR Code or opening a link does not automatically write to either radio. Save confirmation requires radio readback; an accepted Bluetooth write alone is not confirmation. Existing history is retained when you leave a channel or replace its key. Reopen the current channel after replacing a key; historical conversations cannot send using the old key.

## Meshtastic primary and secondary channels

All channels share one LoRa radio configuration. A secondary-channel share includes both the primary settings/key and the selected secondary, so an official ChannelSet recipient receives the primary name used for automatic frequency selection. The share confirmation explicitly discloses the extra key. To share only the primary, use its own share action.

Multi-channel imports are reviewed **one channel at a time**. Select a shared primary to review replacing slot 0, or select a secondary to use a synchronized empty slot. Unrelated slots are retained. When automatic frequency selection depends on a different primary name, join the shared primary first, or configure the matching explicit frequency slot before joining the secondary.

If a link supplies different LoRa parameters, a separate **Apply LoRa first** confirmation is required. This can affect all Meshtastic channels. The app retains the local region, TX power and transmit policy, validates the parameters for the connected firmware, and verifies the save. After verification/reconnection, reopen the retained import link to select the channel. No multi-step transaction is silently replayed.

Names are limited to 11 UTF-8 bytes. Supported keys are none, documented one-byte public/default shortcuts, AES-128 and AES-256. Licensed mode rejects encrypted channel edits. Unknown/new channel fields (including experimental AEAD settings) are rejected rather than silently downgraded. A complete ChannelSet must fit the bounded 512-byte parser; oversized links are rejected. Primary deletion/reordering and automatic bulk replacement of all slots are not offered.

## Validation

The release gates include 45 channel-link/protocol checks, existing 206 Meshtastic conformance checks, 96 MeshCore regression checks, 106 MeshCore user-feature checks, QR encode/decode round trips for both link formats, API 29/35 channel-manager/deep-link instrumentation, build, lint and permanent signing verification. Camera optics, OEM permission flows, physical radio/RF acceptance and actual interoperability with official apps still require field testing.

Official format references: [Meshtastic ChannelSet](https://github.com/meshtastic/protobufs/blob/master/meshtastic/apponly.proto), [ChannelSettings](https://github.com/meshtastic/protobufs/blob/master/meshtastic/channel.proto), [MeshCore channel QR/link specification](https://github.com/meshcore-dev/MeshCore/blob/main/docs/qr_codes.md).
