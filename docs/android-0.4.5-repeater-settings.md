# Android 0.4.5: repeater configuration cards

Version code 39, based on signed Android 0.4.4. The 0.4.4 user test confirmed login,
configuration reads and a successful TX-power command reply. This update addresses
navigation and visibility, not a failure of the basic remote administration transport.

## User flow

Tap a MeshCore repeater in Contacts to open its settings directly. Long-press keeps
contact actions available. Enter its administrator password once. The app loads the
ten public configuration values into cards: name, latitude, longitude, LoRa parameters,
TX power, repeating, local/flood announcement intervals, maximum flood hops and guest
read-only access. The screen shows loading progress and fills cards as replies arrive.

Tap a populated card to edit the current value. Numeric parameters use numeric
keyboards, booleans use switches, and radio parameters have four labeled inputs.
Ordinary edits need one Save action. Radio/routing/access changes retain a brief
confirmation. A successful write reply schedules a read of that setting; the card
keeps its previous value until a valid readback arrives. A different reported value
is shown as such, including when the repeater clamps the requested TX power.

Console, password changes, reboot and reauthentication move under Advanced options.
Diagnostics remain available through an expandable section.

## Reading and traffic

The user explicitly requested automatic reads when entering configuration, replacing
the 0.4.4 requirement to manually read each field. One bounded batch of ten `get`
commands runs after administrator login while this exact repeater's settings view is
visible. It sends only one request at a time, waiting for the reply plus a one-second
gap before the next read. Rendering, incoming status changes and countdown redraws do
not restart the batch. Refresh explicitly starts another batch. Leaving the view or
backgrounding the app clears unsent reads; reopening requests a fresh snapshot.

These scoped configuration reads and explicit card edits bypass the old persisted
30-second administration guard, so loading ten values does not take five minutes.
The diagnostic status/neighbor guard and advanced manual-console path are unchanged.
No periodic configuration polling, password reads, background writes, automatic
write retries, or MT/MC bridging are introduced. Opening a card pauses remaining
reads. Save is handled explicitly, never by the read scheduler.

A missing remote response stops the batch and requires reauthentication; it does not
advance to a different field where a late untagged reply could be mistaken for that
field. Unsupported or malformed configuration values show Unavailable. In-flight
responses can finish after leaving, but do not trigger additional transmissions.
All values and administration rights remain scoped to the current Companion session
and full repeater key and are cleared on disconnect/logout. No new stored password,
chat, backup, or cloud data is introduced. The inherited CLI prefix/timestamp
correlation is not cryptographic request-tag correlation; see the 0.4.4 document.

## Validation

JUnit fixtures exercise one-shot batch loading, serialization/gaps, no hidden writes,
unsupported values, stopping on timeout/navigation, radio isolation, pre-edit validation,
write/readback ordering and clamped values. Instrumentation checks populated cards,
a directly opened prefilled numeric editor, and cancelling without a radio write,
and captures the settings/editor screens on API29/API35.

Signed 0.4.4 remains unchanged. 0.4.5 publication uses the existing manual permanent-key
workflow, after CI and merge. Physical acceptance of this new interaction is pending.
