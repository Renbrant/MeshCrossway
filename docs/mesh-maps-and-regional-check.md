# Mesh maps and regional checks

[Back to README](../README.md) · [Meshtastic vs MeshCore](meshtastic-vs-meshcore.md)

**Reviewed: October 26, 2026.** MT means Meshtastic; MC means MeshCore.

The most useful network is one that reaches the people and places you need. Start with maps, confirm the local configuration, then arrange a small radio test. A marker identifies something to investigate; it does not establish a working connection from your location. Map services collect different evidence, including uploaded node records, packets heard by observers, and measurements made while travelling. Their totals are not directly comparable. See the providers' methods: [MeshCore map uploads](https://github.com/meshcore-dev/MeshCore/blob/main/docs/faq.md#512-q-how-do-i-add-a-node-to-the-meshcore-map), [MeshMap](https://github.com/brianshea2/meshmap.net/blob/main/README.md), and [MeshMapper observations](https://wiki.meshmapper.net/app_tabs/).

## Map directory

| Map and link | Project | Data and visibility | Useful purpose and limitations |
| --- | --- | --- | --- |
| [LetsMesh Analyzer: supplied Colorado view](https://analyzer.letsmesh.net/map?lat=39.73477&long=-105.7708&zoom=6) · [General map](https://analyzer.letsmesh.net/map) | MC | Observer radios report heard packets through MQTT. Repeaters, room servers and companions can contribute. | Investigate observed activity around your region. Visibility depends on participating observers and what they share. [Provider methodology](https://analyzer.letsmesh.net/observer/onboard). |
| [Official MeshCore node map](https://map.meshcore.io/) | MC | The app supports uploading personal nodes and repeater or room-server records. The map offers type, frequency and update filters. | Locate candidate infrastructure and contacts. An uploaded record is not a successful radio test. [Upload instructions](https://github.com/meshcore-dev/MeshCore/blob/main/docs/faq.md#512-q-how-do-i-add-a-node-to-the-meshcore-map). |
| [MeshMapper: worldwide region selector](https://meshmapper.net/) | MC | Community measurements combined with observer reports, organized by participating region. | Find sampled locations and distinguish different kinds of reception evidence. Unmeasured places remain unknown. [Observation definitions](https://wiki.meshmapper.net/app_tabs/). |
| [MeshMapper Denver](https://den.meshmapper.net/) · [MeshMapper Colorado](https://co.meshmapper.net/) | MC | Regional views of the MeshMapper system. Denver's region information links to the statewide view. | Compare your actual route with available samples; inspect their date and settings. Neither view guarantees coverage between measured points. [Filters and layers](https://wiki.meshmapper.net/layers/). |
| [Official Meshtastic map](https://map.meshtastic.org/) | MT | The project's public map interface; consult the metadata available for each record. | Start a regional search, then check recency and configuration. The site's availability is also listed on [Meshtastic service status](https://status.meshtastic.org/). |
| [MeshMap](https://meshmap.net/) | MT | Nodes observed through the official Meshtastic MQTT server; the service does not accept manual node registrations. | Find reported nodes and their details. A drawn connection may represent a multihop path, rather than a direct radio link. [Owner's explanation](https://github.com/brianshea2/meshmap.net/blob/main/README.md). |
| [Liam Cottle's Meshtastic map](https://meshtastic.liamcottle.net/) | MT | Valid position packets reaching this independent map's own MQTT feed. Its published policy removes nodes not heard for seven days. | Explore reported positions, telemetry and traceroutes. Privacy settings and reporting participation limit visibility; its feed differs from MeshMap's. [Map FAQ](https://meshtastic.liamcottle.net/). |

Use the supplied LetsMesh URL as a starting view and move to your region. For MeshMapper, choose the appropriate region first. Compare the locations that matter: home, destination, roads, trails, valleys and meeting points. City-wide activity alone does not answer whether your particular route works.

## Read the evidence behind the colours

MeshMapper distinguishes its phone's local observations from the combined backend view. In the **coverage overlay**, `RX` means traffic was heard without a transmission test; `TX` means a packet reached a backend observer but the traveller did not hear a repeat; `BIDIR` combines an observed repeat with delivery to an observer. Discovery and trace replies are separate evidence. A local green transmission marker can mean only that a repeat was heard. None of these categories proves delivery to every intended recipient. [MeshMapper's exact definitions](https://wiki.meshmapper.net/app_tabs/).

Check sample age, count, preset and antenna conditions before interpreting a coloured square. MeshMapper offers filters for these distinctions and separate tools for estimated antenna coverage. Keep measurements and propagation estimates separate: both help planning, but answer different questions. [Layers, filters and estimation tools](https://wiki.meshmapper.net/layers/).

Likewise, a line on MeshMap may connect an originating node to the gateway that reported it without revealing all intermediate hops. Do not read every line as line of sight or direct reception. [MeshMap connection semantics](https://github.com/brianshea2/meshmap.net/blob/main/README.md).

## Six steps to choose locally

1. **Define the job.** Write down who must communicate, where, whether people move, and whether you can place and maintain infrastructure. Separate occasional text exchanges from location sharing or frequent telemetry. Compare both systems against this same requirement.

2. **Inspect both ecosystems.** Open an MT and an MC map around those locations. Look for recent activity and useful infrastructure, not just the largest count. Check more than one reporting service when the first view is sparse: their collection methods differ, as the directory explains.

3. **Confirm settings with local operators.** Ask which firmware, radio frequency, bandwidth, spreading factor, coding rate, channels and keys the intended group uses. Geographic proximity does not establish compatible configuration. The [MeshCore FAQ](https://github.com/meshcore-dev/MeshCore/blob/main/docs/faq.md) describes regional presets and choosing settings with the community. Colorado readers can also start with [Colorado Mesh](https://coloradomesh.org/).

4. **Arrange a bidirectional test.** Coordinate with an actual person at the destination. Send a modest, numbered batch of short messages in each direction. Record what arrived and what was acknowledged. Pace transmissions with local operators; repeated discovery requests and rapid retries would change the conditions you are trying to assess.

5. **Repeat at relevant positions.** Check the intended indoor location, an outdoor position and representative points along the route. Record antenna placement and movement. If testing depends on a particular repeater, include that dependency in your conclusion instead of assuming equivalent service everywhere nearby.

6. **Choose from the results.** Prefer the system that meets the requirement with practical equipment and configuration. If neither does, consider a better location, antenna installation or a coordinated relay before buying more handhelds. Using both can be reasonable when their local strengths differ; make the operational reason explicit.

## Keep the radio test fair

Decide what you are comparing. A **deployment test** uses each community's actual settings and infrastructure: it answers which service works for you. A **protocol experiment** attempts to match hardware, antennas, power, LoRa parameters, payload and placement: it asks a narrower engineering question. Results from the first test cannot isolate the protocol from all the other differences. This distinction is a testing recommendation, not a claimed benchmark result.

**Online maps and offline radio are separate.** Viewing current public map data requires Internet access. MT and MC radio communication can operate without it, provided a compatible, reachable radio path exists. [Meshtastic introduction](https://meshtastic.org/docs/introduction/) · [MeshCore overview](https://github.com/meshcore-dev/MeshCore).

If evaluating radio-only communication, document any Internet transport and disable bridging for that test where appropriate. An online observer can record a packet that arrived over LoRa; its reporting connection is different from an Internet bridge carrying your message between distant meshes. LetsMesh describes observers as reporters of heard packets, while Meshtastic documents MQTT as an optional integration. [LetsMesh observers](https://analyzer.letsmesh.net/observer/onboard) · [Meshtastic MQTT](https://meshtastic.org/docs/software/integrations/mqtt/).

### Reusable validation log

| Record | Fill in before or during the test |
| --- | --- |
| Purpose and date | Intended use, date, local time, location labels |
| Equipment | Device model/revision, firmware and app versions |
| Radio setup | Protocol, region, frequency, BW, SF, CR, TX power |
| Installation | Antenna, cable, height, indoor/outdoor, fixed/mobile |
| Network | Roles, expected relays, channel, Internet bridging state |
| Trial | Direction A→B or B→A, message numbers, payload size, interval |
| Outcomes | Sent / received / acknowledged, elapsed delivery times |
| Conditions | Route observed, RSSI/SNR if available, interference or changes |
| Interpretation | What worked, what failed, dependencies, unresolved causes |

Record both the delivery count and denominator. A failed acknowledgment does not by itself establish that the message was never received; compare the destination log. Report observations as a result of this particular trial, preserving failures and changes between runs. Avoid publishing private channel keys or precise personal locations in a shared log.

*Method note: links and provider explanations were checked for this revision. Several maps render their data dynamically, and LetsMesh restricted automated retrieval. No live regional node census, measured coverage survey or guarantee of service is claimed here.*
