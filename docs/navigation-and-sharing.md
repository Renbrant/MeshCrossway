# Navigation, location sharing, POIs and tracks

**Status:** product requirements and interactive browser simulation. No live GNSS, LoRa location-sharing scheduler, file-transfer service or companion app has been implemented or bench-validated.

## User experience

The 480 × 320 touchscreen provides **Navigation** with a map/position view and large touch actions for **POIs**, **Tracks**, **Record**, **Share** and **Live share**. Map graphics and sample positions in the web simulator are illustrative, not geographic data. Detailed offline basemaps remain a future milestone.

### POIs and waypoints

- Create, name, categorize, edit, delete and view saved coordinates on the map.
- Record source, fix timestamp, accuracy/validity when available and optional notes.
- Share a single POI with a selected person, group or channel on a selected native network, where supported.
- Future: multiple-POI packages and proximity notifications. Use compact encoding; never assume a full GPX file fits a LoRa message.

### Tracks and GPX

- Start, pause, resume, stop and persist locally recorded tracks to microSD.
- Present distance, duration, point count, elevation where available, data age and integrity.
- Define recording intervals, movement thresholds, storage limits, coordinate precision and battery-use profiles.
- Plan GPX import/export, track simplification, on-map visualization and management.
- Prefer an authenticated Bluetooth companion connection for exchanging large GPX tracks. The web simulator's example GPX export is not a real recorded track. The Bluetooth file transfer and phone companion software do not exist yet.

### Live location sharing

**Default: OFF.** Requires deliberate user opt-in, an explicit network and recipient/channel selection, and a fresh valid GNSS fix.

| Setting | UI prototype options |
|---|---|
| Audience | One person or selected group/channel |
| Network | Meshtastic or MeshCore, with native identity preserved |
| Time interval | 30 s, 1 min, 5 min, 15 min, 30 min |
| Movement threshold | Any distance, 25 m, 50 m, 100 m, 250 m, 500 m |
| Trigger rule | Interval **OR** movement, or interval **AND** movement |
| Session expiration | 15 min, 30 min, 1 h, 4 h, 8 h |
| Session actions | Start, pause, resume, stop |

The threshold UI expresses a **proposed scheduling policy**, not a promise of a successful radio transmission. In the firmware, evaluate movement against the last eligible shared fix using geodesic distance, qualify GPS accuracy, suppress jitter, apply a minimum safe interval regardless of the OR rule, account for radio congestion and local duty/airtime budgets, and avoid repeated packets. Never substitute a stale fix silently. The session must expire on schedule even without connectivity and must show whether sharing is active, paused, expired, blocked, queued or failed. Reboot and recovery policy must be defined and must not restart sharing without valid consent.

**Privacy and safety:** Show network and audience prominently; warn before sharing to public/broadcast channels; do not imply group encryption or delivery guarantees. Display age of participants' last known position instead of marking stale positions as live. Allow immediate stop, with no guarantee already-transmitted information can be retracted. This is not a certified search-and-rescue or emergency tracker.

### Transport selection

| Payload | Preferred path | Notes |
|---|---|---|
| Current position or individual POI | Selected LoRa network | Protocol-native position or explicit compact message, only when supported |
| Few waypoints | LoRa, if practical | Apply size/rate bounds and potentially send only a summary |
| Track summary | LoRa | Name, end/start or selected coordinates, not raw GPX |
| Full GPX or large waypoint collection | Bluetooth companion (future) | Authenticated local transfer, progress, integrity and interruption recovery |
| Offline map assets | microSD or local bulk transfer | Licensing, formats and rendering remain to be designed |

Meshtastic and MeshCore are **not mutually bridged by default**. Any future cross-network forwarding needs a separate opt-in specification addressing loops, identifiers, security and consent.

## Shared team positions (next milestone)

Plan map markers for participants with identity and network, last fix age, source quality, distance and bearing, opt-in access and an explicit stale/unknown status. Group coordination must use supported network primitives rather than inventing a common security model.

## Implementation boundary and validation

The central ESP32-S3 will eventually run a GNSS service, track logger, POI storage, location scheduler, radio-specific adapters and a transport router. Prove on actual hardware: UART adapter capabilities, update/airtime limits, RF coexistence, radio acknowledgments, GNSS power/accuracy, storage resilience, privacy across restarts, Bluetooth throughput/security and degraded/no-fix behavior.

See the [interactive simulator](https://renbrant.github.io/MeshCrossway/) and [UI prototype notes](ui-prototype/README.md). Demo interactions and settings are not proof of firmware feature completeness.
