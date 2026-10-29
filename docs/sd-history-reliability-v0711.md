# v0.7.11 — SD history reliability and diagnostics

Status: experimental. Automated validation does not replace physical SD/BLE tests.
The approved recovery remains the exact published v0.7.8 image.

## Device evidence

The owner reported intermittent `SD unavailable / unsaved RAM`, partial history
after restart, and pauses in touch response on v0.7.10. The submitted binary
journal contains four complete, valid transactions and five messages. All record
checksums and complete-state commit checksums validate; the advisory identity
files agree with the archive identity. There is no truncated or corrupt record
in the supplied copy. This establishes what was saved; it does not establish
that every message seen on the screen reached the SD.

`last-radio` and its `.bak` contain a 32-byte radio identity, not messages. The
`.log` is a versioned binary journal, despite the Windows file-type association.
The screenshot's filesystem dates are not reliable evidence of when messages
were received. Message timestamps are stored separately inside the journal.
No private archive or personal message contents are committed to this repository.

## Confirmed code defects and changes

- Previously every save replayed and hashed the full log before appending, even
  when only telemetry changed. The worker now keeps the last confirmed state in
  its existing PSRAM buffer and an identity-scoped cursor. Steady-state writes
  compare against that state and append only differences. An unchanged state
  produces no transaction. File-size changes, write failure or identity changes
  invalidate the cursor; restart still validates the on-disk journal.
- Pending snapshots for the same radio coalesce, with loads preserving their
  position as ordering fences. New captures during a write failure update the
  retry image without bypassing the five-second retry interval. Retry timing is
  measured after an operation completes.
- A connected-radio load during a failed save uses the newer, same-radio retry
  snapshot. Reconnecting BLE must not replace these unsaved messages with an
  older disk archive. Restoring data never queues transmissions.
- A read/open I/O failure is distinguished from a torn transaction. Saving must
  not truncate or replace a file that could not be read. A genuinely incomplete
  tail is still recovered to the last validated commit before appending.
- Chat diagnostics now report the failure stage, errno, failure count and last
  save duration. Serial diagnostics contain no message text, keys or locations.
  The remote status page and `/health` distinguish **Wi-Fi backup** from **chat
  history**. Device Diagnostics shows both. An SD write failure, invalid model
  or allocation failure is no longer described simply as an absent SD card.
- The empty-chat label no longer overlaps the conversation controls.

No schema or retention change: the existing plaintext schema-2 files remain
readable, with 96 messages per radio. Bulk buffers still require PSRAM; Bluetooth
startup priority and the static internal-memory guard remain in place.

The supplied journal cannot identify the exact device-level cause of each write
failure or prove the cause of each UI pause. The additional stage/errno and timing
measurements are intended to resolve that remaining uncertainty on hardware.

## Verification and physical retest

Automated tests exercise the production worker with simulated FreeRTOS timing:
coalescing, continuous incoming edits during sync failure, bounded retry rate,
same-radio RAM recovery, successful retry and zero restored transmissions.
Store tests cover cached append/no-op behavior, cursor invalidation, read and sync
errors, every byte truncation of a delta, quota rotation and backup recovery.
The supplied archive is checked locally for schema compatibility; it is not a
public test fixture.

Install v0.7.11 with **Erase unchecked**. Preserve a copy of the SD as captured.
Send and receive uniquely numbered messages; wait for `SD: journal saved`, then
restart with the Bluetooth radio initially off and compare both DM and channel
history. Reconnect BLE and confirm there are no automatic retransmissions.
During a failure, record the chat error and **Last history operation** on the
remote page or Device Diagnostics. Export serial logs if possible. Test SD absent
only by removing it while the equipment is powered off; reinsert while off and
restart. Check touch response while messages, Wi-Fi and captures are active.

Messages never committed to the supplied archive cannot be recovered from that
archive by a firmware update.
