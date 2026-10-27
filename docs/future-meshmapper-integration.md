# Future Possibility: Official MeshMapper Integration

> **Status:** Parked idea / research note — not committed to implementation.
> **Decision gate:** Do not contact MeshMapper maintainers or begin integration work until a functional, viable MeshCrossway hardware prototype has been built and validated.
> **Recorded:** 2026-10-27.
> **Visibility:** Standalone repository note only. Do not add links from README, site navigation, roadmap, or other documents unless the project owner later requests it.

## Purpose

Explore a future **official partnership and interoperability with MeshMapper**, allowing MeshCrossway to contribute MeshCore wardriving measurements to **MeshMapper's existing community coverage database**, and to display data from that same database. A separate MeshCrossway community coverage database is **not** the goal.

This is an optional, deferred concept, not a promised feature, an authorized API integration, or an active partnership.

## Why this may be useful

MeshCrossway's planned standalone GNSS, MeshCore radio, display and local storage could support coverage surveys during travel without relying on a phone for measurements. The shared MeshMapper dataset could then benefit the wider MeshCore community. A later extension might allow MeshCrossway to compare coverage of MeshCore and Meshtastic locally, but MeshMapper's MeshCore database must not be assumed to support Meshtastic records.

## Current technical assessment (reverify before implementation)

- **MeshMapper Flutter app**: open source (MIT); useful reference for companion protocol, wardriving workflow, session management, offline queues and payloads.
  - https://github.com/MeshMapper/MeshMapper_Flutter_App
- **MeshMapper documentation:** https://wiki.meshmapper.net/
- **Coverage API:** https://wiki.meshmapper.net/coverage-api/
  - Supports coverage-data retrieval under a provisioned integration key, with applicable geographical limits, rate limits and caching constraints.
  - A read-only integration key **does not** imply permission to upload measurements.
- **Wardriving upload:** The official mobile app communicates with MeshMapper's service for session authentication, wardrive measurement batches and device information. The server/backend is not generally open source. The existence of these endpoints **does not establish authorization or a stable third-party upload contract**.
- **Third-party endpoints:** Features allowing the MeshMapper mobile app to forward data to other services are **not the same as** an official third-party-device ingestion API.
- **Wardriving semantics:** Genuine bidirectional coverage may depend on the MeshCore wardriving channel, session identifiers and server-side observers/echo correlation, not merely a GPS point and RSSI/SNR.
- Technical specifics, endpoint paths, payload schemas, licensing, authentication, privacy rules, API limits and regional radio constraints **must be rechecked against current upstream documentation and confirmed with maintainers** at the time of the project.

## Prerequisite: hardware-ready milestone

**No outreach before this milestone.** First build and validate a useful hardware prototype, including:

1. Stable main controller, display, GNSS and MeshCore radio interface.
2. Functional MeshCore transmission/reception, correct region-specific radio settings and safe traffic rate.
3. Reliable timestamps and GPS fixes with field-tested measurements along real routes.
4. A workable local data queue and an identified Internet synchronization path (e.g., Wi-Fi when available).
5. A concise demonstration: photos/video, firmware status, reproducible field results and an explanation of how the device could help their community.

Hardware readiness is a condition for **starting a conversation**, not authorization to implement their upload protocol.

## Proposed collaboration sequence

### Phase 0 — Keep parked

- Preserve this document only; no coding, API key requests, outreach, public feature commitment or new navigation/README links.
- Reassess only after the hardware-ready milestone.

### Phase 1 — Contact the MeshMapper maintainers

After the prototype works, introduce MeshCrossway, share a brief demonstration and ask whether MeshMapper is interested in an **official standalone hardware contribution path**.

Discuss explicitly:

1. Is third-party embedded wardriving ingestion accepted and supported?
2. Is there a public, versioned upload specification, SDK or dedicated integration endpoint?
3. How are session creation, identity, authentication, device registration, keys, throttling and revocation handled?
4. What measurement fields, GNSS accuracy, timing, radio presets and echo/repeater correlation are mandatory?
5. Can data be queued offline and submitted later, and how are retries, de-duplication and clock drift handled?
6. What consent, privacy, attribution, retention, abuse prevention, licensing and community data quality requirements apply?
7. Is a read-only Coverage API key suitable for a global mobile device, and should queries go through a controlled backend to avoid exposing a key in firmware?
8. Is there a sandbox/test environment, review process and preferred channel for open-source contributions?
9. Would maintainers welcome a reusable embedded client/library or specification developed jointly?

**Do not presume approval, negotiate on their behalf or use undocumented private endpoints without permission.**

### Phase 2 — Joint technical design (only if welcomed)

- Agree on a written integration contract and supported credentials.
- Define schemas, session state machine, upload batch limits, retry/backoff, deduplication and provenance.
- Map MeshCore events/echoes to MeshMapper classifications accurately; do not manufacture bidirectional success from a one-way packet.
- Define UI states, data ownership, offline behavior and graceful degradation.
- Keep security credentials out of public firmware; design a suitable token/proxy arrangement.
- Define testing methodology and compliance with local RF regulations.

### Phase 3 — Proof of concept (only after approval)

1. Implement an optional MeshMapper adapter behind a feature flag.
2. Record GPS + timestamp + radio configuration + MeshCore survey events + signal diagnostics, as required by the agreed schema.
3. Queue samples reliably when Internet is unavailable.
4. Authenticate and upload to the **approved** MeshMapper endpoint; honor retry limits and server responses.
5. Read community coverage through the documented Coverage API and cache it responsibly.
6. Verify uploaded tracks actually appear correctly in the official database; compare expected coverage states and detect duplicate submissions.
7. Review security, privacy, performance and device power use with maintainers.

### Phase 4 — Optional release

Only after validated interoperability and explicit upstream agreement, document installation, key handling, user consent and support expectations. Contribute reusable components or documentation upstream if desired by maintainers.

## Go / no-go criteria

**Proceed only if:** hardware is viable; maintainers explicitly welcome or authorize ingestion; a stable interface is available; data quality and privacy requirements are achievable; implementation and ongoing API usage are sustainable.

**Defer or cancel if:** upload remains closed to third-party clients, undocumented endpoints would need to be reverse engineered against operator wishes, credentials cannot be safely deployed, or the integration would require building a competing public coverage database.

## Possible introduction (draft, not to send now)

> Hello MeshMapper team,
>
> We are developing MeshCrossway, an open-source standalone device combining MeshCore and Meshtastic radios, GNSS and an integrated display. Our hardware prototype is now functional, and we would like to explore whether it could contribute MeshCore wardriving observations directly to the official MeshMapper community coverage database and display approved coverage data.
>
> We are not looking to create a competing coverage platform or use private APIs without permission. Would you be open to discussing an officially supported integration for embedded devices, including upload authentication, measurement semantics, offline synchronization and potential open-source contributions?
>
> We can share hardware details, a demo and example field measurements if helpful.

---

**Project-owner direction:** Keep this idea quietly in the repository. **Once a viable hardware version is ready**, consider contacting the people responsible for MeshMapper regarding a possible partnership. Until then, this document is only a future reference.
