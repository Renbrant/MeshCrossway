# Android 0.4.4: MeshCore repeater administration

Implementation branch: `feature/android-0.4.4-repeater-admin`, based on Android 0.4.3.
Version code: 38. This document does not announce a published signed APK.

## User flow

Open an approved MeshCore repeater from Contacts / the repeater inventory, then
choose **Administer repeater**. Enter its password. Controls appear only after a
remote login response grants administrator permission. A guest or legacy response
without an administrator bit does not unlock writes.

Configuration offers explicit reads and edits for name, latitude, longitude,
LoRa parameters, TX power, forwarding, local/flood advert intervals, maximum flood
hops and read-only access. Each read/write is one manual operation. The UI also
provides a remote CLI, administrator/guest password changes and a confirmed reboot.
Support depends on the repeater firmware; unsupported CLI replies are displayed,
not treated as successful settings changes. Radio parameters can require reboot.
Read a setting again after editing to verify the effective value. A reboot may not
reply, so timeout is reported as unknown delivery/change, not success or failure.

Passwords and console inputs are excluded from Android view-state saving and
autofill, are not written to chat/history/preferences/cloud, and sensitive CLI
replies are hidden. Login/command byte arrays are erased after local completion or
connection teardown. Java/Android may retain transient String/input copies until
GC; this does not claim hardware-backed credential storage or complete memory erasure.
No password is remembered for reconnect. Administrator access is scoped to a full
repeater key within one Companion connection and expires after ten idle minutes.
Closing administration clears phone-side access; it does not revoke the repeater's
ACL entry or promise a remote logout. Reconnect, timeout or local rejection require
login again. The firmware controls the actual ACL and authorization.

A shared persisted 30-second per-repeater guard covers diagnostics and administration.
Only one remote operation is allowed at a time. There are no automatic retries,
configuration writes, logins or RF queries on page opening. Existing Companion
message synchronization continues, and no Meshtastic/MeshCore bridge is introduced.

## Protocol references

Checked against public upstream source on 2026-11-12:

- [meshcore_py messaging](https://github.com/meshcore-dev/meshcore_py/blob/main/src/meshcore/commands/messaging.py): login command 26 with full public key; repeater CLI via command 2, CLI_DATA type 1, attempt 0, timestamp, six-byte prefix and UTF-8 text.
- [Companion firmware](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp): local `RESP_CODE_SENT` is only submission; login pushes 0x85/0x86 carry permissions and contact prefix; remote CLI returns in contact message frames (legacy and v3 envelopes).
- [CommonCLI](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/CommonCLI.cpp) and [radio preferences](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/CommonRadioPrefs.cpp): supported command syntax and bounds.

Remote CLI lacks a diagnostic request tag. Correlation uses a single outstanding
operation, matching unique six-byte contact prefix, drained initial message backlog
and monotonically checked remote reply timestamps. This is weaker than tagged
binary diagnostics: a delayed unseen reply with a newer remote timestamp can still
be ambiguous. Responses are labeled remote replies and never automatic verified
readbacks. Ambiguous contact prefixes are refused. CLI and legacy repeater replies
are kept out of normal chats, including after timeout, to avoid persisting private
command echoes. Re-login does not automatically re-send any previous command.

## Diagnostics UI

Uptime and neighbor ages use days/hours/minutes, battery uses volts. Consecutive
validated neighbor pages accumulate into one list (for example 8 + 5 = 13). The
next page remains manual. A new offset-zero snapshot replaces the previous list;
changed totals/noncontiguous pages cannot combine unrelated snapshots. Protocol,
unknown extension bytes, full identities and routing notes are under **Technical
details**. These are session observations, not proof of current radio reachability.

## Validation and publication

Protocol fixtures cover local-vs-remote acknowledgement, wrong prefixes, guest
permissions, duplicate/stale replies, timeout, reconnect isolation, command bounds,
secret response suppression, chat isolation, concurrency and 8+5 neighbor merging.
Run JUnit, protocol fixtures, Android debug build/lint and API29/API35 smoke before
merging. Real repeater login/configuration acceptance remains a hardware test.

Preserve signed 0.4.3. Publish 0.4.4 only through the manual permanent-key workflow
on `baseline/android-0.1.0`, after integration and checks, using the existing signing
certificate. Update public download links only after the signed release exists.
