# Node age colors and retention review

## Recommendation for discussion

Add a saved **Color by last heard** option independently to Meshtastic and MeshCore. Preserve the manually selected shape and size. Use the same color resolver for all icon shapes and name badges, and show a legend: green under 24 hours, yellow from 24 hours to under 7 days, gray from 7 through 30 days. These colors indicate observations by the attached radio, not an online or reachability guarantee. A 72-hour green threshold can be offered as a more permissive discovery-history preset; it is less useful for a traveler deciding what was heard near the current location.

Remove unprotected local discovery records after more than 30 days without a genuine observation. Protect favorites, the attached/local device identity and contacts with unread or queued traffic. Preserve conversation history independently. Unknown, invalid or future timestamps remain unknown, never automatically expired until a trustworthy time basis exists. Reading cached records during BLE sync must not count as hearing a node now. Contact modification time is not a substitute for radio-observation time.

Apply retention both when restoring SD data and accepting synchronization records; otherwise the radio's persistent node table simply brings old records back. Keep radio firmware data untouched initially: this proposal concerns MeshCrossway's local discovered-node cache. Write compacted snapshots through the existing SD worker, in bounded batches, on idle/periodic maintenance rather than on every draw or packet. A reasonable schedule is once after valid time becomes available and then daily. Preserve atomic replacement/recovery behavior.

## Findings in the current code

- Meshtastic protocol storage reserves 128 node slots; MeshCore reserves 512 contact slots. These are fixed arrays in client state. Removing an entry alone does not return their reserved memory to the heap.
- The combined map overlay reserves slots for 128 + 512 markers and clusters. Its capacity also remains allocated regardless of the populated count.
- The Nodes UI already selects at most 32 recent entries with a 14-day visibility cutoff (favorites bypass the cutoff). Visibility is not persistent database eviction, and the map reads the underlying protocol stores separately.
- Meshtastic SD archives currently serialize a fixed-size Archive. Decreasing node_count alone does not shrink that archive file either. A storage-format or capacity change is needed for real size reduction.
- Name badges already have a limited recency color treatment (5 minutes/60 minutes), but manual/network icon colors and other shapes do not all use the same age-color policy.

Therefore retention should reduce stale UI clutter and iteration costs first. Actual RAM savings require a separate, measured change: a smaller bounded working set with a compact SD representation, keeping the larger discovery history off the UI working set. Protocol capacity changes must preserve stable public identity references, message destinations, radio synchronization and archive compatibility. Do not promise a percentage reduction in internal RAM from record deletion alone.

## Primary-source comparisons

1. [Meshtastic: joining your local mesh](https://meshtastic.org/docs/getting-started/joining-your-local-mesh/) explains that the radio node database survives reboot and LoRa configuration changes. Records from earlier locations/settings remain listed; being in the database does not establish current reachability.
2. [MeshPulse README](https://github.com/maxg10/meshpulse) uses green below one hour, yellow at 1–6 hours and red above six hours; it explicitly limits that interpretation to the observing tracker. Its published default node TTL is 48 hours. Those thresholds suit a live monitor rather than a longer travel history.
3. [MeshCore Home Assistant contact management](https://meshcore-dev.github.io/meshcore-ha/docs/ha/contacts/) separates discovered and stored contacts, uses 12 hours to distinguish fresh/stale adverts, and offers optional daily discovered-contact cleanup with a 30-day default threshold. Its cleanup is disabled by default and preserves intentionally added entries. That app's lastmod-based cleanup is not proof that lastmod is equivalent to a locally heard timestamp.
4. [MeshCore companion firmware](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) provides an overwrite-oldest policy for non-favorites when contact storage is full, plus auto-add type and hop filters. This supports bounded capacity and favorite protection instead of an unlimited contact list.

These are different policies, not a universal standard. The proposed 24-hour / 7-day / 30-day thresholds are a MeshCrossway recommendation for portable use, not an official Meshtastic or MeshCore requirement.

## Acceptance tests for a future implementation

- Exact color/expiry boundaries, missing/future time and clock correction; independent network configuration and every marker format.
- Restore and BLE resync cannot resurrect expired local records or refresh their last-heard age merely by importing them.
- Favorites, local radio identity, unread messages, pending sends and separate conversation history survive cleanup.
- Run cleanup while maps and radios are active without stale indices/pointers or GUI-thread filesystem work; handle no SD, write failure and interrupted replacement.
- Compare populated counts, snapshot sizes, PSRAM/internal free and largest block, allocation failures, map rebuild cost and touch latency before/after. Evaluate memory capacity reduction separately against v0.9.25 stable.

The research policy was approved for implementation after the v0.9.27 orientation hotfix. See [v0.9.28 implementation and test procedure](node-age-v0928.md) for the delivered controls, protection rules and limits.
