# Touch input and RAM history cache

Independent change based on `cdc6e087` (v0.7.12, after Meshtastic PR #62).
No version bump, release catalog change, merge or deployment is part of this PR.

## Problem and behavior

The previous polling path could lose a physical tap entirely while the LVGL
owner task was occupied. Its generation-based chat refresh also deleted all
cards and scrolled to the bottom for unrelated radio status changes. Host
reproduction moved a reading viewport from 800 to 7962 pixels. A simulated
100 ms UI stall lost a 60 ms tap. These are software reproductions, not measured
FT6336 timings or proof of a hardware fault.

The new input adapter samples the existing FT6336 handle on a separate core-0
task. It feeds the SDK's existing LVGL pointer through a bounded 64-sample FIFO
and `continue_reading`; it does not register a second pointer. It preserves SDK
`user_data` and the original feedback callback. LVGL and widgets remain confined
to their owner task. Gesture classification uses every delivered sample, after
LVGL display rotation, with acquisition timestamps. Layout changes wait until
the physical queue is idle and the release grace period has elapsed.

Consecutive movement can coalesce only when the queue is full. DOWN and UP edges
are preserved; edge overflow cancels and quarantines input until physical UP.
One or two failed I2C reads do not synthesize release. Three consecutive failures
cancel safely, as does an acquisition gap over 250 ms during a press. Task
allocation failure retains the original SDK polling path. The requested sampling
interval is 10 ms after each read; it is not a hard latency guarantee.

The chat cache retains up to 96 rendered cards in PSRAM through the existing
LVGL allocator. It reuses unchanged cards, replaces changed cards and creates
only new ones. A reading viewport follows its visible message ID and pixel
offset when old messages leave the bounded ring. A viewport already at the
bottom follows new messages. Protocol refresh waits for queued/physical touch,
pointer release and scroll momentum. Navigation invalidates the view before
deleting its widgets. MeshCore and Meshtastic retain their separate selections,
radio identities, stores and transmission rules.

Composer, room and Meshtastic status labels now update only when their text
changes, avoiding repeated allocations for an unchanged offline reason.

## Startup and storage

The PSRAM models are allocated under the UI lock after network startup has
attempted the SD mount, avoiding an unmounted-card/empty-archive race. The first
MeshCore offline restore can then queue immediately; subsequent unavailable-card
attempts retain the five-second backoff. Meshtastic can restore its separately
remembered BLE identity without starting Bluetooth. Reads/writes continue in the
existing low-priority SD worker. Neither restore path queues a transmission.

Normal chat navigation and scrolling use RAM. PSRAM is volatile: cold startup
still needs one SD restore. This PR removes the unnecessary dependence on the
12-second BLE auto-resume delay; it does not promise instant boot or turn SD into
an unlimited archive. Retention remains 96 messages per radio model. There is no
new storage schema or whole-history NVS copy. A persistent flash checkpoint or
larger paginated archive would require a separate capacity, wear and recovery
design.

The input task uses a 3072-byte internal stack. The existing history worker uses
6144 bytes and now starts earlier, so its internal-RAM cost overlaps Bluetooth
startup. Bulk models, snapshots and widgets remain in PSRAM. Physical minimum
free internal RAM, task stack margins and BLE coexistence must be measured before
release. Input counters (`errors`, `max_gap_ms`, queue `peak` and `overflows`) are
available in `mcxtouch` and logged every 30 seconds with the sampler stack
margin for that validation.

## Validation

- Real LVGL 8.4: a buffered 60 ms tap survives a simulated 100 ms UI stall;
  four rail directions, no unintended icon click, cancellation, modal ownership,
  SDK ownership and sampler allocation fallback.
- Real production UI: status-only refresh allocates no cards and preserves scroll;
  ring eviction and ACK changes preserve reading; bottom follows new messages;
  pending physical input delays structural updates; unchanged composer has zero
  allocations across 100 refreshes.
- ASan/UBSan: FIFO edge preservation, motion coalescing, overflow cancellation,
  quarantine and timestamps, plus existing protocol/history suites.
- Production history/runtime fixtures: offline restoration is queued before BLE,
  SD retry backoff remains bounded, no automatic retransmission, and independent
  MeshCore/Meshtastic destinations and archives.
- Existing firmware preflight builds the PR in ESP-IDF 5.3.2 and retains artifacts
  without publishing Pages. CI status on the PR is authoritative for compilation.

Run the host UI suite with:

```sh
python3 scripts/test_ble_pairing_ui.py --lvgl /path/to/lvgl-8.4.0
python3 scripts/test_edge_swipe.py
python3 scripts/test_conversations_history.py
python3 scripts/test_meshtastic.py
```

Hardware acceptance remains pending: tap/scroll during live traffic and SD errors,
all four rail drags, repeated offline cold starts, absent/slow/reinserted SD,
BLE reconnect and both protocol histories. Record I2C failures, acquisition gaps,
queue overflow, internal heap and task stack margins. The host stall is simulated.

## Integration boundaries

Most new logic is in four new headers. Shared hooks are limited to the UI,
startup patcher and first-offline-restore scheduling. This PR does not edit BLE
transport, protocol codecs, archive schemas, SDK clocks, VERSION, published
binaries or the flasher. Parallel edits to the two UI render/poll functions,
`draw()`, touch initialization or SD `poll()` need normal hunk-level review;
separate branches reduce overlap but cannot guarantee future edits never conflict.

## Public references

- [LVGL 8.4 documentation](https://docs.lvgl.io/8/_downloads/39cea4971f327964c804e4e6bc96bfb4/LVGL.pdf),
  input device interface: buffered reads and `continue_reading`.
- [LVGL v8.4.0 input processing](https://github.com/lvgl/lvgl/blob/v8.4.0/src/core/lv_indev.c)
  and [event feedback ordering](https://github.com/lvgl/lvgl/blob/v8.4.0/src/core/lv_event.c):
  checked against the actual host-tested source.
- [ESP-IDF 5.3.2 external RAM](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/external-ram.html):
  bulk-data PSRAM placement and task-stack/cache restrictions.
- [WADAMESH user guide](https://wadamesh.com/docs.html) and
  [upstream release notes](https://github.com/ALLFATHER-BV/wadamesh/releases):
  behavioral benchmark for responsive chat refresh and visible durable-storage
  status. Its architecture and hardware are not assumed equivalent; no WADAMESH
  source was copied and no relative-performance claim is made.
