# MeshCore everyday tools — Android 0.4.0

MeshCrossway keeps Meshtastic and MeshCore on independent Bluetooth radios and independent networks. These additions apply to MeshCore. Install the [0.4.0 APK](https://github.com/Renbrant/MeshCrossway/releases/tag/android-v0.4.0) as an update; the permanent signing certificate is retained.

## Announce your presence

Open **Contacts → MeshCore → Announce my presence**, or the same action in **Settings → MeshCore**.

- **Nearby** sends a zero-hop announcement.
- **Across the mesh** requests a flood announcement.
- The confirmation explains whether the radio's configured advertisement policy includes its location. The phone's location is not automatically inserted.
- Radio acceptance is displayed separately from reception by other devices. There is no remote delivery confirmation for this advert.
- Repeated pending taps are blocked, with a 30-second cooldown after submission. Ordinary errors remain local. There is no automatic announcement on app connection, name edits or retries.

## Create, edit and remove channels

Open **Channels → Manage MeshCore channels**, or **Settings → MeshCore → Channels**. Connect and synchronize the MeshCore radio first.

**Create channel** offers:

| Type | Key | Who can join |
| --- | --- | --- |
| Private | Random 16-byte key | Anyone you share the key/link with |
| Hashtag | First 16 bytes of SHA-256 of the exact hashtag name | Anyone who knows or guesses that name |
| Public | The official publicly known Public key | Anyone |

Names allow up to 31 UTF-8 bytes. A loaded empty slot is selected for creation. A configured slot offers opening the conversation, editing, sharing and removal. Keys are masked in the editor; sharing requires an explicit confirmation.

A save sends SET_CHANNEL and reads back the actual slot/name/key, plus the radio identity. An ACK alone does not confirm the change. Unused bytes after the name's null terminator are ignored; the key is still compared exactly. A mismatched readback shows actual values with an unconfirmed result. Ordinary command errors keep the connection alive.

Channel removal clears name and key on the radio. Saved messages remain on the phone. Renaming retains the channel identity; changing its key changes the current conversation identity. Old conversation targets cannot transmit with a replaced key. Sending and key sharing for an edited slot are blocked until current values are confirmed. A readback error or cancellation keeps that slot protected until a fresh channel read succeeds; unrelated channels and direct messages remain usable. Duplicate active keys across slots are rejected to prevent ambiguous reception. **Refresh channels and region** reads slots sequentially with bounded queues.

## Share or import a channel

**Share channel link** creates the official `meshcore://channel/add` format using confirmed radio values. The link includes a secret, so send it only to intended participants. It can be shared through Android's share sheet.

Use **Import channel link** to paste a link, or open a MeshCore channel link from another app and choose MeshCrossway. Opening a link only prepares a preview: it never changes a radio automatically. Review the name, slot and key, then choose **Save to radio**. If the key is already configured, the existing channel is offered instead of adding a duplicate.

Optional `region_scope` is preserved. A scoped import requires the radio's default region to be loaded and match the shared region. Importing never silently changes global region settings. For an unscoped link, the radio retains its own default region. This release does not add a QR scanner or a complete repeater-region editor.

All new write actions and sharing confirmations are bound to their original radio/session; an editor whose channel changes must be reopened.

## Discover, approve and keep history separate

Choose **Contacts → MeshCore** and use **All**, **Discovered**, **Approved** or **Historical**. Counts update with the contact list; text badges remain visible on mixed-network lists too.

- **Discovered:** heard or saved as a discovery, with approval still required. Open the contact for its approval action.
- **Approved:** confirmed in the synchronized radio's current contact table.
- **Historical:** saved on the phone but absent from the synchronized radio's current table.
- Offline and unsynchronized views explicitly label the last saved state, rather than claiming current radio membership.

Favorites and type/search/distance filters remain available. Removing approval from the radio retains local history. Radio auto-add remains governed by the radio's policy; the app resolves actual membership with a read, without silently sending an ADD command.

## Export a support diagnostic

Choose **Settings → Support diagnostic**, or **More options → Support diagnostic**. Choose **Save file** for a UTF-8 text file or **Share text** for Android's share sheet. No account or internet connection is required to generate the report.

Included fields are allowlisted: app/Android versions, phone manufacturer/model, numeric firmware versions, connection phase, synchronization/pending state, MTU, operation/contact counts, reconnect attempts and recent numeric connection/write/protocol event codes. Event timestamps are relative to the service's first recorded event. The in-memory event buffer is limited to 200 entries; it is not a logcat dump.

Excluded: message text, coordinates/tracks, user/contact/channel names, Bluetooth addresses, public/private keys, channel secrets, passwords/PINs, account tokens, exception text, raw packets and incoming intent/link contents. Firmware suffixes are omitted. The report snapshot is taken when the support dialog opens; saving writes it off the UI thread. Reports are sent only through the destination you choose.

## Validation and limits

[Implementation PR #226](https://github.com/Renbrant/MeshCrossway/pull/226) and the [channel-send guard PR #228](https://github.com/Renbrant/MeshCrossway/pull/228) contain source and CI evidence.

- Existing **96 MeshCore** regression checks and **206 Meshtastic** conformance checks pass locally.
- **106 user-feature checks** cover official channel vectors/URI round trips, malformed and duplicate parameters, name/key limits, announcements, cooldown/error handling, readback/padding/key mismatch, key replacement/removal safety, sends queued during edits, readback-error/cancellation guards, bounded refresh, recovery without SET replay, contact states and diagnostic privacy/bounds.
- Existing protocol/browser/migration fixtures pass. Gradle's JUnit wrapper runs the feature tests too.
- API 29/35 instrumentation adds channel manager/deep-link/diagnostic compatibility checks and persistence checks that channel replacement/removal retains message history. Build, lint, emulator and permanent signing checks gate the [release workflow](https://github.com/Renbrant/MeshCrossway/actions/workflows/android-permanent-release.yml).

Physical radio/RF and actual cross-app acceptance remain necessary. These are implemented, regression-tested flows, not a certification of every hardware/firmware combination or full official-app parity. BLE pending-save and late-ACK recovery remain limited to the same selected radio and service lifetime. Messages and SET commands are never automatically retransmitted.

## Pinned official references

- [Companion 1.18.0 command handlers](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/examples/companion_radio/MyMesh.cpp): SEND_SELF_ADVERT, GET_CHANNEL and SET_CHANNEL.
- [Channel table behavior](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/src/helpers/BaseChatMesh.cpp): channel updates take effect in the in-memory table on this released firmware.
- [Official channel link specification](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/qr_codes.md): channel name, 16-byte hexadecimal secret and optional region scope.
- [Protocol/channel documentation](https://github.com/meshcore-dev/MeshCore/blob/81dbae592ffad40b4212bc0b37e82b38da08f4bc/docs/companion_protocol.md): supplemental channel type/key guidance; firmware handlers take precedence where documentation is ambiguous.
