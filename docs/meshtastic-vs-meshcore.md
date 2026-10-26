# Meshtastic vs MeshCore: a practical and technical comparison

[Back to MeshCrossway](../README.md) · [Maps and regional checks](mesh-maps-and-regional-check.md) · [Radio integration](radio-firmware-strategy.md)

**MT = Meshtastic. MC = MeshCore. Last technical review: October 26, 2026.**

Choose the network that reaches the people and places you need, with the features and operating procedures your group can maintain. Both projects support useful off-grid communication. Their differences in forwarding, configuration, applications and infrastructure can matter greatly, but neither name guarantees better coverage, battery life or delivery.

This guide combines **documented behavior**, **attributed operator experiences**, and **MeshCrossway engineering recommendations**. Community experiences are paraphrased and linked; they are not controlled benchmarks. MeshCrossway has not performed a comparative RF field test. Its touchscreen and dual-radio integration remain a design and simulator project.

## Contents

1. [Which should you choose?](#which-should-you-choose)
2. [Version baseline](#version-baseline)
3. [Shared radio foundations](#shared-radio-foundations)
4. [Complete comparison table](#complete-comparison-table)
5. [How forwarding actually works](#how-forwarding-actually-works)
6. [Range, speed and congestion](#range-speed-and-congestion)
7. [Security and privacy](#security-and-privacy)
8. [Position, telemetry and message history](#position-telemetry-and-message-history)
9. [What users report](#what-users-report)
10. [Maps and choosing your local network](#maps-and-choosing-your-local-network)
11. [Implications for MeshCrossway](#implications-for-meshcrossway)
12. [A fair test before committing](#a-fair-test-before-committing)
13. [Sources and maintenance](#sources-and-maintenance)

## Which should you choose?

The recommendations below are **engineering judgments based on the mechanisms explained in this guide**, not a ranking measured by MeshCrossway.

| Your actual need | Sensible starting point | What to verify |
|---|---|---|
| Contact people who already operate a reliable local mesh | Join their existing MT or MC network | Exact settings, recent activity and a working return path |
| Carry a small group into an area without installed infrastructure | MT CLIENT relaying is a straightforward option; MC can also work directly or with supported off-grid Repeat Mode | Intermediate devices really forward, and every participant has matching settings |
| Build a regional messaging backbone with maintained elevated sites | Evaluate MC's dedicated-repeater model and explicit DM paths; compare against a correctly configured modern MT deployment | Site availability, failure recovery, channel traffic and maintainer capacity |
| Combine messaging, location, sensors and existing integrations | Examine MT's modules and integrations first, then check MC's current sensor/client capabilities | Support for your exact sensor, app and firmware |
| Keep the radio silent except for deliberate communication | Evaluate forwarding roles, background broadcasts and telemetry policy in either system | Actual traffic capture, including firmware-generated transmissions |
| Use a standalone screen and keyboard | Compare the specific device UI in both ecosystems | Configuration access, map workflow, accessibility and licensing |
| Travel between different communities, or need contacts on both networks | Separate MT and MC radios can be useful | Two configurations, identities and RF paths; this is MeshCrossway's planned approach |

A private group can bring its own compatible radios. A public network adds value only where its infrastructure and users are reachable. One effective relay on a suitable site can matter more than many portable nodes behind terrain or buildings.

## Version baseline

A comparison made in late 2025 can omit important 2026 changes. Firmware, mobile apps, standalone UIs and map services also have separate release cycles.

| Component reviewed | Baseline | How to interpret this guide |
|---|---|---|
| Meshtastic widely published release track | **2.7.26.54e0d8d Beta**, listed as Latest during this review | Do not assume every deployed node has 2.8 features |
| Meshtastic newer feature track | **2.8.1.8e6a88d Alpha**, October 1, 2026 | New signing, optional channel AEAD and other 2.8 behavior are explicitly qualified here |
| MeshCore | **Companion / Repeater / Room Server 1.18.0**, October 15, 2026 | Role and protocol details were checked against the 1.18 source, with client-dependent behavior distinguished |

Sources: [MT releases][MT-releases], [MT 2.8.1][MT-alpha], [MC 1.18 release][MC-release].

The MT documentation currently describes 2.8, while existing meshes may still run 2.7. Revoked MT 2.8.0 builds should not be treated as the baseline. MC's FAQ contains some older descriptions that do not cover newer Companion Repeat Mode and sensor features; dated release notes and versioned source take precedence where they conflict.

## Shared radio foundations

**LoRa is the modulation; Meshtastic and MeshCore are different protocols using it.** Both can operate without cellular service or an Internet connection. A phone typically provides the interface to a nearby radio over Bluetooth or another local connection; the phone alone does not become a LoRa node. These systems also differ from LoRaWAN: an ordinary LoRaWAN gateway does not automatically relay their traffic. See the [MT introduction][MT-intro] and [MC project][MC-project].

There are three separate compatibility questions:

1. **Hardware:** does the board and antenna cover the intended frequency band, and does the firmware support its exact revision?
2. **Radio parameters:** do frequency, bandwidth, spreading factor, coding rate and other required PHY settings match?
3. **Protocol and application:** are the devices running the same ecosystem and compatible channel, identity and security settings?

A label such as “915 MHz” answers only part of the first question. Two devices in the same band can be unable to communicate because they use different presets. A shared chat-channel name does not repair that mismatch. MT's logical channels share the node's radio configuration; MC also has application channels above its radio settings. Sources: [MT LoRa configuration][MT-lora], [MT channels][MT-channels], [MC radio presets][MC-presets].

Changing a preset is a community coordination decision. For example, the MT 2.8.1 release changed the default for **first-time US region selection through the device screen** to LongTurbo. It did not automatically move every existing installation. LongTurbo and LongFast cannot hear each other. That distinction is more useful to a newcomer than assuming a historic default is still universal. [MT release notice][MT-alpha]

## Complete comparison table

The table is divided by topic for readability. Its entries describe upstream capabilities, **not features already implemented in MeshCrossway**. Detailed qualifications and sources follow.

### Network architecture and routing

| Criterion | Meshtastic (MT) | MeshCore (MC) |
|---|---|---|
| Core local operation | Off-grid LoRa messaging and data | Off-grid LoRa messaging and data |
| Internet requirement | Optional for gateways and online services | Optional for observers, maps and integrations |
| Broadcast/group routing | Managed flooding with suppression and role-dependent forwarding | Flood routing; scopes and repeater policies can limit propagation |
| Direct-message routing | Learned next-hop routing since 2.6, with discovery/fallback flooding | Flood discovery when needed; subsequent packets can carry a learned explicit path |
| Ordinary portable-node forwarding | CLIENT can relay; CLIENT_MUTE does not relay others | Normal Companion mode does not relay others |
| Temporary mobile relaying | Available through appropriate roles and settings | Optional Companion Repeat Mode on supported off-grid settings |
| Fixed infrastructure | CLIENT_BASE, ROUTER and ROUTER_LATE serve different purposes | Dedicated Repeater firmware is the normal backbone component |
| Need a repeater for two nearby users? | No, if they have a usable direct link | No, if they have a usable direct link |
| Meaning of “direct message” | Addressed to one recipient; may use relays | Addressed to one recipient; a direct route may contain relays |
| Hop/path limit | Hop counter 0–7; eligible zero-cost infrastructure hops complicate physical-hop counting | Bounded path storage, encoded count and hash width; not unlimited |
| Moving or failed relays | Learned next-hop can fail; rediscovery/fallback costs airtime | Stored path can become stale; retry/reset/rediscovery depends on client and configuration |
| Native MT–MC interoperability | No | No |

Sources: [MT routing][MT-routing], [MT next-hop source][MT-next-hop], [MT roles][MT-roles], [MT zero-cost hops][MT-zero], [MC routing source][MC-mesh], [MC chat source][MC-chat], [MC Repeat Mode][MC-repeat], [MC packet format][MC-packet].

### Messaging, applications and integration

| Criterion | Meshtastic (MT) | MeshCore (MC) |
|---|---|---|
| One-to-one text | Supported | Supported |
| Public and private groups | Logical channels; up to eight channel slots sharing the same radio settings | Public, name-derived and private-secret channel models; limits depend on build/client |
| Message-size budget | Small frames; protobuf, metadata and security overhead reduce useful text | Core text buffer is about 160 bytes, with sender-name/format overhead; not 160 guaranteed Unicode characters |
| Delivery feedback | Distinguishes relay/broadcast evidence and recipient DM acknowledgments | DM acknowledgments and path information; group traffic does not acknowledge every member |
| Human read receipt guaranteed by radio ACK? | No | No |
| Message history after absence | Optional Store & Forward service, with hardware/channel limitations | Room Server posts/history; ordinary repeaters are not general mailboxes |
| GNSS/location | Node/phone position, fixed position and smart broadcasts | Position/adverts, GNSS and telemetry according to board, build and client |
| Sensors and telemetry | Documented modules for device, environmental and other sensor data | Sensor-node examples, telemetry queries and permissions; 1.18 adds push subscriptions |
| Standalone operation | Supported on suitable device/UI builds | Supported on suitable device/UI builds; feature availability differs |
| Phone/computer clients | Android, Apple, web and developer tools | Native, web and community clients; check platform and transport support |
| Host/controller API | Protobuf client API; serial PROTO is planned for MeshCrossway | Companion protocol; custom UART exposure still needs board/build validation |
| BLE, USB and network links | Availability depends on board and build | Interface variants; 1.18 adds BLE+Wi-Fi on selected boards, including Heltec V3 |
| MQTT/online integration | Built-in module and supported client proxy configurations | Observer/upload/bridge projects; behavior depends on the integration |
| TAK and other integrations | Published integrations including ATAK and CalTopo/SARTopo | Assess the specific third-party integration; do not assume native feature parity |

Sources: [MT software][MT-software], [MT client API][MT-api], [MT integrations][MT-integrations], [MT position][MT-position], [MT telemetry][MT-telemetry], [MT history][MT-history], [MT MQTT][MT-mqtt], [MC Companion API][MC-api], [MC text source][MC-chat-header], [MC sensor introduction][MC-sensor], [MC 1.18 notes][MC-release-notes], [MC Home Assistant integration][MC-ha].

### Security and privacy

| Criterion | Meshtastic (MT) | MeshCore (MC) |
|---|---|---|
| Private DM key model | Pairwise secret derived from public/private node keys | Pairwise secret derived from public/private node keys |
| Conventional group protection | Shared-key AES-CTR; optional AES-CCM/AEAD in 2.8.1 Alpha | Shared-secret encrypted group payloads |
| Default public channel confidentiality | Published key; assume public readability | Published key; assume public readability |
| Current payload cryptography reviewed | Modern DMs use X25519 and AES-256-CCM | 1.18 source uses AES-128 block encryption and a truncated HMAC |
| Authenticated broadcasts | 2.8 adds signatures, with size/hardware/policy qualifications | Signed adverts; a group sender name is not an individual signature |
| Exposed information | Routing headers; public/shared-channel data according to configuration | Routing/path metadata; adverts can include readable name and location |
| History-service trust | Server can retain the traffic it is able to receive/decrypt | Room Server processes posts and is a trusted content endpoint |
| Complete anonymity or future key-compromise protection | Do not assume either | Do not assume either |
| Public-map privacy | Publishing and MQTT choices affect visibility | Advert/map/observer choices affect visibility |

Sources: [MT encryption][MT-crypto], [MT limitations][MT-security-limits], [MT versioned crypto][MT-crypto-code], [MC identity][MC-identity], [MC crypto implementation][MC-utils], [MC payloads][MC-payloads], [MC room source][MC-room].

### Hardware, operation and project fit

| Criterion | Meshtastic (MT) | MeshCore (MC) |
|---|---|---|
| Guaranteed range | None from the protocol name alone | None from the protocol name alone |
| Practical capacity | Depends on airtime, topology, traffic and configuration | Same dependencies; explicit DM paths do not eliminate group floods |
| Battery runtime | Board, GNSS, display, host links and radio workload dependent | Same dependencies; default non-forwarding may reduce one source of work |
| Hardware selection | Check exact board/revision and firmware target | Check exact board/revision and Companion/Repeater/UI target |
| Firmware openness | Core firmware repository is GPL-3.0 | Core firmware/library is MIT |
| All ecosystem software under one license? | No; inspect each client and dependency | No; native apps and Ripple/T-Deck GUI have different terms/source availability |
| Software cost | Core open-source software; hardware and operating costs remain | Core messaging is free; some client/UI extras use optional paid unlocks |
| Switching an existing radio | Possible when both projects support that exact hardware; back up settings/keys | Same; changing firmware does not preserve protocol identity or chat interoperability automatically |
| MeshCrossway role | Independent native radio and adapter | Separate independent native radio and adapter |

Sources: [MT hardware][MT-hardware], [MT firmware license][MT-license], [MC firmware license][MC-license], [MC FAQ on clients and optional features][MC-faq], [MT flasher][MT-flasher], [MC flasher][MC-flasher]. Range/capacity/energy rows are engineering implications, not measured winner claims.

## How forwarding actually works

### Meshtastic: selective rebroadcasting and learned next hops

For group broadcasts, eligible nodes wait before forwarding and can cancel a redundant transmission after hearing another relay. Role and reception conditions influence those decisions. “Every node repeats every message” is therefore a poor description of managed flooding. [Official algorithm][MT-routing]

For a DM, successful exchanges can teach the forwarding nodes which neighbor to try next. This is **hop-by-hop learning**: each stage uses its own next-hop information. Unknown or failing routes can fall back to flooding, so mobility recovery and extra network traffic are linked. This behavior is present in the [versioned next-hop implementation][MT-next-hop].

A broadcast's implicit ACK means the sender heard another node retransmit it, even if that relay cannot decrypt the channel. It does not confirm receipt by every group member. A recipient DM ACK is a different, end-to-end protocol event; losing the return ACK can leave the sender uncertain even when the message arrived. [Acknowledgment behavior][MT-routing]

Roles are operational choices. A handheld inside a building should not become a high-priority infrastructure router simply because the name sounds more powerful. CLIENT_BASE can help a household's weaker nodes; ROUTER and ROUTER_LATE have different infrastructure uses. REPEATER is a historical role, deprecated since 2.7.11. Follow the [role documentation][MT-roles] and [deployment guidance][MT-tips].

### MeshCore: learned paths and deliberate relay roles

When a contact has no usable stored path, a DM can be flooded to find it. A return exchange supplies a path; later messages can embed the sequence of repeater identifiers. This is **source/path routing**. It avoids asking every available repeater to disseminate each successful targeted exchange, but it does not establish a globally optimal route. The path can stop working when a relay moves or fails. [Versioned chat implementation][MC-chat]

Group channels are a different case: public and private channel messages use flood forwarding. A private key changes who can read the message, not the fact that repeaters may retransmit it. Scopes provide a way to constrain dissemination when participating infrastructure is configured accordingly; they are not encryption boundaries. [Forwarding implementation][MC-mesh], [region filtering][MC-regions], [default scope][MC-scope]

The normal division between Companion and Repeater helps separate user endpoints from forwarding infrastructure. A Room Server is another service, for stored posts; it is not a requirement for a two-person chat. [Project applications][MC-project]

### The 2026 exception: Companion Repeat Mode

MC now has an optional mode for temporary off-grid relaying. The middle device can remain a Companion and forward after the setting is saved. Ordinary public-mesh Companion operation still defaults to no forwarding. Supported off-grid radio settings must be coordinated across the group; selecting a different frequency on a single radio also means leaving the previous public network. [Developer explanation][MC-repeat], [1.18 Companion implementation][MC-companion], [default preferences][MC-prefs]

**Field report, not a comparison benchmark:** Seeed Studio's October 23, 2026 article describes three Wio Tracker L1 Pro devices. Moving the intermediate device to a higher, more open position improved delivery in its small test, and forwarding continued without the phone connection. The manufacturer authored the report, it tested only one setup, and it did not compare MT under matched conditions. It illustrates placement and supported behavior; it does not establish an expected success rate for other trips. [Read the report][MC-repeat-test]

### Why “7 hops versus 64 hops” is incomplete

In MT, the hop field is bounded, but eligible **zero-cost hops** between deliberately configured infrastructure nodes can preserve its value. The counter is not a universal count of physical retransmitters. A zero-cost hop still consumes airtime and adds a point where a message may be lost. [Feature explanation][MT-zero], [versioned routing rules][MT-router]

MC allocates up to **64 path bytes**. Current encoding also has a six-bit path count, and 1-, 2- or 3-byte path entries change the usable route budget. Wider hashes help distinguish repeaters, but require compatible firmware and spend more bytes per entry. Its FAQ's shorthand “64 hops” is not a promise of 64 working relay stages. Prefer the actual [packet encoding][MC-packet] and [versioned Packet source][MC-packet-code] when implementing or testing.

A longer permissible route does not make every link reliable. Even a path that works well individually at each stage can suffer accumulated delay, retries and loss. Use observed end-to-end delivery and recovery behavior to judge a regional route.

## Range, speed and congestion

### Separate physical reach from network reach

**Physical reach** is whether one transmitter can reach one receiver. **Network reach** is whether a usable sequence of links reaches the destination. An operator reporting a very long regional conversation may have used multiple elevated repeaters; it is not evidence that a handheld reaches that distance directly.

Frequency, bandwidth, spreading factor, coding rate, antenna efficiency, height, obstructions, interference and receiver implementation affect the RF link. A protocol comparison using different presets is simultaneously comparing different radio settings. Faster settings may reduce airtime while sacrificing some link margin. Extreme range records describe particular hardware and paths, not an everyday urban coverage radius. [MT radio settings][MT-radio], [documented range tests][MT-range]

| Radio term | Meaning for the decision |
|---|---|
| Bandwidth (BW) | The spectral width used by the signal; changing it affects throughput, sensitivity and occupied spectrum. |
| Spreading factor (SF) | A modulation setting that trades symbol duration and data rate against reception margin. Higher SF is not a free range upgrade. |
| Coding rate (CR) | Error-correction redundancy; more redundancy spends additional airtime. |
| RSSI | Received signal strength. A strong signal alone does not establish successful decoding or a usable return path. |
| SNR | Signal relative to noise. Interpret it with the modem settings and actual delivery results. |

Definitions and preset relationships: [official radio overview][MT-radio].

**Operator experience:** a user testing EU868 settings with four Heltec V3 devices and two sites 2.7 km apart reported asymmetric MC delivery while MT worked in both directions. They tried other presets and antennas, investigated unequal noise floors and had no demonstrated final diagnosis in the linked discussion. That is a reason to test both directions, not proof of an inherent protocol sensitivity difference. [User's update][USER-asymmetric], [follow-up][USER-asymmetric-followup]

Increasing transmit power on your own node cannot, by itself, fix a weak return link from the other endpoint. Test replies and acknowledgments, not merely reception of a distant advertisement.

### Airtime is the common bottleneck

A packet occupies the channel while transmitted. Discovery, relays, acknowledgments, retries, adverts, positions and telemetry all spend that resource. A low message rate per person can still become a large combined load.

**Illustrative calculation, not a measured MT/MC result:**

- Assume 20 people send one position every 30 seconds: **40 original packets/minute**.
- Assume each packet also triggers three relay transmissions: **160 transmissions/minute**.
- Assume each transmission takes 0.5 seconds: **80 seconds of aggregate airtime per minute**, before acknowledgments and retries.

Under the simplified assumption that all of this traffic competes in the same interference area, the offered load cannot be serialized into one minute. Real networks have spatial reuse and different frame durations; the example demonstrates why aggressive tracking cannot be approved merely because each user sends “only twice a minute.”

MC's reusable DM paths can reduce unnecessary distribution for targeted traffic. Its group floods still need limits. MT has both duplicate suppression and newer traffic-management mechanisms; 2.8 changes position/telemetry defaults to opt-in. Neither architecture removes the need to measure and control traffic. [MC flood controls][MC-cli], [MT 2.8 changes][MT-alpha]

For a group deployment, first remove unnecessary automatic traffic, coordinate appropriate forwarding roles, and set conservative location intervals. Prefer movement thresholds and session expiry over continuous updates. Limit retries and keep ordinary messages usable. A private channel still shares RF airtime with other traffic using the same radio settings.

### Energy follows the whole device

Neither protocol provides a fixed battery duration. An ESP32 with an illuminated touchscreen and continuous GNSS may have a different power budget from a sleeping nRF52 node even before routing matters. Wi-Fi, Bluetooth, power amplifiers, receive duty, retransmissions and battery conversion losses add further differences. Hardware selection and configuration therefore belong in any runtime comparison. [MT hardware overview][MT-hardware], [MC power-management documentation][MC-power]

For MeshCrossway, measure the display controller, each radio and GNSS separately, then measure the assembled system. Default MC endpoint behavior can avoid forwarding work, but that alone cannot predict the dual-radio terminal's runtime.

## Security and privacy

### Public, private and individually addressed are different

Both ecosystems have published default channel keys. Treat their public channels as readable by strangers. A name-derived or widely distributed group key should not be presented as confidential. A private group needs a strong secret distributed through a trusted method; anyone holding it is inside that group's content boundary. [MT channels][MT-channels], [MC channel types][MC-api]

DM encryption uses node identities and pairwise secrets. That does not automatically verify the person's real-world identity. Confirm important contacts through an independent trusted exchange, particularly before sending sensitive locations.

### Meshtastic's version-dependent protections

Modern MT DMs use X25519-derived AES-256-CCM. Conventional channel traffic uses shared-key AES-CTR; **2.8.1 Alpha** adds optional channel AEAD, with compatible configuration required on every participant that needs to read that channel. The 2.8 track also introduces broadcast signatures, subject to size, hardware and policy limitations. Headers remain visible. [Encryption overview][MT-crypto]

The versioned implementation includes authenticated DM acknowledgment proof. This is stronger evidence of protocol receipt, but is not a human read receipt. Do not assume every 2.7 peer offers all 2.8 protections. [Crypto implementation][MT-crypto-code], [reliable routing][MT-reliable]

Key compromise, first-contact trust and stored plaintext still matter. MT documents the absence of forward secrecy; a newly generated strong key does not retroactively protect recorded traffic encrypted under a compromised old key. [Forward-secrecy explanation][MT-crypto], [identity and key-trust limitations][MT-security-limits]

### MeshCore's security model and tradeoffs

MC uses public/private node identities, pairwise DM secrets and shared channel secrets. The reviewed 1.18 implementation applies AES-128 independently to 16-byte blocks (**ECB-style**), followed by **HMAC-SHA256 truncated to two bytes**, for standard encrypted payloads. Repeated identical plaintext blocks under the same key can therefore produce identical ciphertext blocks. The short integrity tag is a separate design tradeoff. This construction should not be equated with a modern messaging protocol's full authentication design simply because both use AES. [Identity code][MC-identity], [crypto code][MC-utils], [protocol constants][MC-constants]

Adverts are signed but can expose name, public key and coordinates. Group sender names do not independently authenticate each human participant. A Room Server receives and processes posts, so its operator/device is part of the content trust boundary. An ordinary relay and a message-history server should not be treated as equivalent privacy roles. [Payload definitions][MC-payloads], [room-server implementation][MC-room]

No complete security winner is claimed here. AES key length alone does not compare identity verification, integrity tags, metadata exposure, replay handling, forward secrecy, endpoint storage and update discipline. Neither ecosystem should be assumed to provide all properties of an audited modern private messenger.

### The terminal also matters

MeshCrossway's controller will receive application data from both radio nodes. Stored conversations, diagnostic exports, local access and key backups therefore matter in addition to radio encryption. Keep the selected audience visible, avoid putting coordinates into public diagnostics, and preserve protocol identity.

Automatic bridging would decrypt and re-encrypt content at a trusted gateway. It would not create native MT-to-MC end-to-end encryption. Any future bridge would need explicit forwarding rules, identity mapping, duplicate and loop prevention, and handling of private messages and location audiences. The initial design does not automatically bridge networks.

## Position, telemetry and message history

### Location: receiving a fix is not publishing it

MT supports node- or phone-provided positions, fixed locations and movement-aware broadcasts. Position sampling, local display updates and radio publication are separate activities. Its supported fields can carry timing and quality-related information; configuration controls what is sent. [Position documentation][MT-position]

MC also supports location. It can appear in adverts and telemetry, with behavior depending on hardware, build and client. Sensor examples and 1.18 push subscriptions expand the available workflows. “MeshCore has no GPS or telemetry” is outdated. Public advert coordinates, however, are not a private live-tracking session. [Sensor introduction][MC-sensor], [telemetry payloads][MC-payloads], [1.18 release notes][MC-release-notes]

MT's telemetry modules cover a documented set of sensor types and metrics. MC provides its own sensor architecture and integrations. Compare the actual measurement and delivery workflow you need, including permissions and client display, rather than marking both with a feature checkbox and assuming interchangeability. [MT sensor support][MT-telemetry], [MC integration example][MC-ha]

**Direct Tracking in MeshCrossway remains a proposal.** “Direct” in either protocol can mean a routed DM. The project needs verified zero-repeater behavior, explicit recipient consent, bounded sessions and a radio budget before offering a strict Direct Only mode. Group transmission needs separate validation. See [navigation and sharing](navigation-and-sharing.md) and [firmware gates](radio-firmware-strategy.md).

### Offline history: specify which component was online

MT has an optional Store & Forward server. The documented server requires suitable ESP32 hardware with PSRAM, can return heard text history, and has channel and request limits. It is not a promise that every private DM is retained while its recipient is off. Replaying history can also create a burst of radio traffic. [MT Store & Forward][MT-history]

MC's Room Server provides a different, BBS-like post/history service. It must be available to accept the post and later serve it; storage is bounded. Ordinary channel chat and ordinary repeaters do not become persistent mailboxes simply because the network is called a mesh. [Room-server source][MC-room]

For either system, distinguish a phone disconnected from a still-powered radio from a radio that was fully switched off. Also distinguish history cached in a local client from history actually recoverable over RF. Those are different user experiences.

### Internet services are optional additions

MT's MQTT module can connect radio traffic to online services under configured uplink/downlink policies. MC has observer, upload and bridge integrations, including an integration in its official GitHub organization. Their capabilities and traffic directions are not identical. [MT MQTT][MT-mqtt], [MC Home Assistant][MC-ha], [community MC MQTT bridge][MC-mqtt]

An online map can keep showing yesterday's position during an Internet outage. Conversely, an offline private group can communicate successfully without ever appearing on a map. Online visibility and local radio delivery are separate questions.

## What users report

### The supplied video: a useful migration perspective

[**Why we're switching from Meshtastic to MeshCore — The Comms Channel**](https://www.youtube.com/watch?v=guDoKGs02Us), published December 6, 2025, is the reference suggested for this guide.

In its published description, the creator says he consulted active MC communities in Boston, Southern California, the Pacific Northwest, Austin and Omaha. He summarizes reports favoring MC for regional, repeater-based messaging, while recognizing MT use cases for smaller mobile groups. The description also promotes the TennMesh community.

**Evidence boundary:** this review accessed the video's published description and metadata, not its full transcript or YouTube comment thread. No quotation, timestamp or individual comment is presented as if verified from the video. Its selected migration experiences are valuable context, but do not constitute a randomized or matched comparison, and predate the newer features described above.

### Colorado: local coordination can decide the outcome

**AJ Quick, August 22, 2026:** running two Heltec V4 radios, he found an active MC network and ultimately preferred it locally. His MT contacts disappeared after a preset change that was not shared by all nearby users. He explicitly acknowledges that his MT trial was short and that coordination contributed to the result.

**How to use the report:** ask which settings nearby operators actually use. It does not establish a universal MC range advantage or a reliable regional user-count ratio. [Read AJ Quick's firsthand account][USER-colorado]

### Tennessee: the same video led to a different choice

An operator near Chattanooga describes being motivated by the same video to investigate MC and repeater sites. The page's later update says the Seeed solar node's MT interface better served the operator's needs, so the group would stay with MT for the time being.

**How to use the report:** configuration and the actual device UI can outweigh an architectural preference. The page is organized under 2026; it is a dated personal experience, not a comparison of every current client. [Read the operator's account][USER-tennessee]

### Pittsburgh and Phoenix: two useful reasons to keep both

In an August 2026 discussion, **u/Snoo_22440** reports using both systems in Pittsburgh, with MT reaching the desired contacts more effectively at that time. **u/georged486** reports choosing MC around Phoenix and MT for trips where repeater infrastructure is inconsistent.

These are geographically specific preferences. They support evaluating **who is reachable and how the equipment will be used**, not declaring a national winner. They also illustrate the motivation for a terminal that can access both networks. [Pittsburgh comment][USER-pittsburgh], [Phoenix comment][USER-phoenix]

### How to interpret enthusiastic or frustrated comments

Treat “it just works here” as evidence about that person's setup, not as an unqualified protocol property. Ask:

- Was the comparison made at the same locations with the same antennas and suitable settings?
- Were both directions tested, and were ACKs distinguished from received/read messages?
- Was there a maintained repeater network on one side but little infrastructure on the other?
- Were firmware and app versions recorded, including later changes?
- Did the author publish packet counts and failure cases, or just memorable successes?

The asymmetric Heltec report above is another reminder that contrary experiences exist. Neither an enthusiastic migration story nor one unresolved failure is enough to predict your own route.

## Maps and choosing your local network

Start with the [**map directory and regional evaluation guide**](mesh-maps-and-regional-check.md). It includes:

- [LetsMesh centered on the Colorado view supplied for this project](https://analyzer.letsmesh.net/map?lat=39.73477&long=-105.7708&zoom=6).
- [Official MC map](https://map.meshcore.io/) and [official MT map](https://map.meshtastic.org/).
- [MeshMapper worldwide](https://meshmapper.net/), [Denver](https://den.meshmapper.net/) and [Colorado](https://co.meshmapper.net/).
- [MeshMap](https://meshmap.net/) and [Liam Cottle's MT map](https://meshtastic.liamcottle.net/).

The companion guide explains the difference between a registered node, an observer report, a sampled coverage point and a calculated prediction. It also provides a local test log. Do not choose a system solely by counting map markers.

## Implications for MeshCrossway

The planned architecture keeps the upstream protocols on **separate radios**, with one controller providing a common interface:

```mermaid
flowchart TD
    UI["MeshCrossway touchscreen and local storage"] --> HOST["Controller and separate adapters"]
    GNSS["Shared u-blox GNSS"] --> HOST
    HOST --> MT["Meshtastic radio and identity"]
    HOST --> MC["MeshCore radio and identity"]
    MT --> MTNET["Meshtastic peers"]
    MC --> MCNET["MeshCore peers"]
```

The dedicated MT node is planned to use the [serial PROTO client API][MT-serial]. The MC node uses the [Companion interface][MC-api]. The selected LoRa V3 boards still need firmware-target and external-UART validation; stock USB or BLE availability does not prove the intended pin-level connection. The upstream ESP32 implementation exposes build-time UART choices, but the build must be checked on the delivered board. [Companion entry point][MC-main], [hardware and wiring](hardware-wiring.md)

This arrangement supports an interface that can show both ecosystems while preserving their differences:

1. **Separate identity and audience.** A person's MT contact and MC contact are distinct; matching display names do not prove they are the same person.
2. **Accurate message state.** Queued, accepted by radio, transmitted, relay heard, recipient acknowledged and read are different claims.
3. **Independent queues and recovery.** A stalled adapter must not block the other, navigation or local history.
4. **Visible position age and source.** An old advert or cached fix must not look like a live teammate.
5. **Per-radio congestion policy.** Tracking and retries need budgets even when the terminal can use two networks.
6. **RF coexistence tests.** Nearby transmitters can impair reception. Two radios are not a guarantee of uninterrupted simultaneous service.
7. **Versioned build records.** Keep firmware versions, targets and configuration reproducible; do not silently rely on an Alpha feature.

These are project requirements and engineering judgments. No working cross-protocol bridge, tested zero-hop tracking or operational field coverage is claimed. See the [firmware strategy](radio-firmware-strategy.md).

## A fair test before committing

There are two different questions worth testing: **which existing network serves me better?** and **how do the protocols behave under comparable conditions?** The first includes local infrastructure as part of the result. The second must control it.

For a useful local evaluation:

1. Record exact hardware, antenna, firmware/app, frequency, BW/SF/CR, power and forwarding role.
2. Use the actual places and carrying positions that matter: house, vehicle, trail, workplace and destination.
3. Agree on a small number of numbered messages with a peer. Record receipt at the far end and return replies separately.
4. Compare direct RF and relayed operation. If testing offline paths, disable Internet bridges in the test setup and document what infrastructure remains.
5. Repeat after moving or removing a relay and at another time of day. Record recovery time and missed messages, not just best-case successes.
6. Evaluate the UI and battery under the same practical workload.

A 20-message trial can be a starting sample, not a guarantee of reliability. Space traffic conservatively and coordinate with the community rather than load-testing a public mesh unannounced. Useful measures are application delivery ratio in each direction, latency distribution, retries, RF airtime, route recovery and energy use. A single average or a successful long-distance message hides too much.

For rescue or remote-field planning, evaluate the specific route, backup communications and operating procedure. Neither community map nor this unvalidated prototype establishes emergency-service availability. This follows the project's existing [operating boundaries](project-reference.md#operating-boundaries).

## Sources and maintenance

Links throughout the guide point to official documentation, versioned source or the original operator report. The following are useful starting points:

| Source family | Main references |
|---|---|
| MT releases and internals | [Releases][MT-releases], [routing][MT-routing], [roles][MT-roles], [security][MT-crypto] |
| MC releases and internals | [1.18 release][MC-release], [release notes][MC-release-notes], [packet format][MC-packet], [versioned chat code][MC-chat] |
| Apps and integrations | [MT software][MT-software], [MT API][MT-api], [MC Companion protocol][MC-api], [MC sensor examples][MC-sensor] |
| Community evidence | [Supplied video description](https://www.youtube.com/watch?v=guDoKGs02Us), [Colorado][USER-colorado], [Tennessee][USER-tennessee], [Pittsburgh][USER-pittsburgh], [Phoenix][USER-phoenix], [asymmetric-link report][USER-asymmetric] |
| Coverage evidence | [Maps, methods and regional checks](mesh-maps-and-regional-check.md) |

When revising this page, update the review date and release baseline, check default-role behavior, routing and security changes, and distinguish stock firmware from optional builds and app features. Preserve the date and context of historical reports. Do not turn a personal opinion into a specification or an estimated path into guaranteed coverage.

MeshCrossway is independent of both upstream projects. Third-party code and documentation retain their respective licenses; MeshCrossway's MIT license does not relicense them.

[MT-intro]: https://meshtastic.org/docs/introduction/
[MT-releases]: https://github.com/meshtastic/firmware/releases
[MT-alpha]: https://github.com/meshtastic/firmware/releases/tag/v2.8.1.8e6a88d
[MT-routing]: https://meshtastic.org/docs/overview/mesh-algo/
[MT-next-hop]: https://github.com/meshtastic/firmware/blob/v2.8.1.8e6a88d/src/mesh/NextHopRouter.cpp
[MT-roles]: https://meshtastic.org/docs/configuration/radio/device/
[MT-tips]: https://meshtastic.org/docs/configuration/tips/
[MT-zero]: https://meshtastic.org/blog/zero-cost-hops-favorite-routers/
[MT-router]: https://github.com/meshtastic/firmware/blob/v2.8.1.8e6a88d/src/mesh/Router.cpp
[MT-lora]: https://meshtastic.org/docs/configuration/radio/lora/
[MT-channels]: https://meshtastic.org/docs/configuration/radio/channels/
[MT-radio]: https://meshtastic.org/docs/overview/radio-settings/
[MT-range]: https://meshtastic.org/docs/overview/range-tests/
[MT-crypto]: https://meshtastic.org/docs/overview/encryption/
[MT-security-limits]: https://meshtastic.org/docs/about/overview/encryption/limitations/
[MT-crypto-code]: https://github.com/meshtastic/firmware/blob/v2.8.1.8e6a88d/src/mesh/CryptoEngine.cpp
[MT-reliable]: https://github.com/meshtastic/firmware/blob/v2.8.1.8e6a88d/src/mesh/ReliableRouter.cpp
[MT-position]: https://meshtastic.org/docs/configuration/radio/position/
[MT-telemetry]: https://meshtastic.org/docs/configuration/module/telemetry/
[MT-history]: https://meshtastic.org/docs/configuration/module/store-and-forward-module/
[MT-mqtt]: https://meshtastic.org/docs/configuration/module/mqtt/
[MT-software]: https://meshtastic.org/docs/software/
[MT-api]: https://meshtastic.org/docs/development/device/client-api/
[MT-serial]: https://meshtastic.org/docs/configuration/module/serial/
[MT-integrations]: https://meshtastic.org/docs/software/integrations/
[MT-hardware]: https://meshtastic.org/docs/hardware/devices/
[MT-license]: https://github.com/meshtastic/firmware/blob/v2.8.1.8e6a88d/LICENSE
[MT-flasher]: https://flasher.meshtastic.org/
[MC-project]: https://github.com/meshcore-dev/MeshCore
[MC-release]: https://github.com/meshcore-dev/MeshCore/releases/tag/companion-v1.18.0
[MC-release-notes]: https://blog.meshcore.io/2026/10/15/release-1-18-0
[MC-faq]: https://docs.meshcore.io/faq/
[MC-chat]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/helpers/BaseChatMesh.cpp
[MC-chat-header]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/helpers/BaseChatMesh.h
[MC-mesh]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/Mesh.cpp
[MC-companion]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/examples/companion_radio/MyMesh.cpp
[MC-main]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/examples/companion_radio/main.cpp
[MC-prefs]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/examples/companion_radio/NodePrefs.h
[MC-room]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/examples/simple_room_server/MyMesh.cpp
[MC-packet]: https://docs.meshcore.io/packet_format/
[MC-packet-code]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/Packet.h
[MC-constants]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/MeshCore.h
[MC-payloads]: https://docs.meshcore.io/payloads/
[MC-identity]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/Identity.cpp
[MC-utils]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/Utils.cpp
[MC-api]: https://docs.meshcore.io/companion_protocol/
[MC-presets]: https://docs.meshcore.io/radio_presets/
[MC-cli]: https://docs.meshcore.io/cli_commands/
[MC-regions]: https://blog.meshcore.io/2026/01/20/region-filtering
[MC-scope]: https://blog.meshcore.io/2026/04/17/default-scope
[MC-repeat]: https://blog.meshcore.io/2026/02/13/off-grid-client-repeat-mode
[MC-repeat-test]: https://blog.meshcore.io/2026/10/23/repeat-mode-tracker-l1-pro
[MC-sensor]: https://blog.meshcore.io/2026/09/14/sensor-intro
[MC-power]: https://docs.meshcore.io/nrf52_power_management/
[MC-ha]: https://github.com/meshcore-dev/meshcore-ha
[MC-mqtt]: https://github.com/ipnet-mesh/meshcore-mqtt
[MC-license]: https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/license.txt
[MC-flasher]: https://flasher.meshcore.io/
[USER-colorado]: https://www.ajquick.com/articles/meshcore-vs-meshtastic-what-i-found-running-both
[USER-tennessee]: https://www.dornbush.net/01-06-meshtasticcore.html
[USER-pittsburgh]: https://www.reddit.com/r/meshcore/comments/1vsu15b/comment/p4ov1rt/
[USER-phoenix]: https://www.reddit.com/r/meshcore/comments/1vsu15b/comment/p4o7vmw/
[USER-asymmetric]: https://www.reddit.com/r/meshcore/comments/1r7yaa1/comment/o9mvl4a/
[USER-asymmetric-followup]: https://www.reddit.com/r/meshcore/comments/1r7yaa1/comment/obzc4ep/


