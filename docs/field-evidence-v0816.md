# MeshCrossway v0.8.16 — screenshots of working features

**Evidence supplied for the project record on 1 November 2026.** These captures show the
v0.8.16 firmware running on the MeshCrossway touchscreen and the device's local
browser companion. They record point-in-time operation; they are not a claim of
long-duration testing or universal compatibility with every radio firmware.

## Touchscreen device

### Home status — both mesh radios connected

![Home screen showing Meshtastic and MeshCore connected together, live node counts, GPS fix and Wi-Fi state](site/media/v0.8.16/01-device-home-radio-gps.png)

The Home screen reports **Meshtastic Connected** and **MeshCore Connected** at
the same time. It also shows live node counts, local GNSS fix/estimated accuracy,
Wi-Fi state and direct settings shortcuts on both radio cards.

### Nodes and network filters

![Nodes and networks screen with All, Meshtastic and MeshCore filters](site/media/v0.8.16/02-device-network-nodes.png)

The Nodes/Networks screen displays the selected network and provides **All / MT /
MC** filters while listing native nodes.

### Device-side messages and conversations

![On-device message feed with message sender, local time and hop count](site/media/v0.8.16/03-device-message-list.png)

The device message view renders sender, message text, local time and hop metadata.

![On-device conversation history with received and unconfirmed messages](site/media/v0.8.16/04-device-conversation.png)

Conversation history is available on the touchscreen and distinguishes received
messages from messages whose delivery is not confirmed.

## Local browser companion

The browser interface is served by the MeshCrossway device and accessed over the
local network. It is operating on the same device data; it is not a simulated
web prototype or cloud service.

### Tracks, folders and map preview

![Remote Tracks and recording page with folders, saved tracks and route map preview](site/media/v0.8.16/05-remote-tracks-map.png)

The Tracks & recording page shows folder selection and controls, a saved-track
list, route details and an interactive route preview. GPX files can be downloaded
from the browser as portable backups while the original track journal stays on
the device's microSD.

### Remote messaging

![Remote Messages page with MeshCore conversations, groups, nodes and message composer](site/media/v0.8.16/06-remote-messages.png)

The Messages page provides separate MeshCore/Meshtastic selection, Conversations,
Groups, Nodes and Network views, message history, a composer and radio send
controls. Radio settings are available from the selected network's Network view.

## What this evidence establishes

- Both Meshtastic and MeshCore were connected concurrently in the v0.8.16 build.
- Network and conversation views were rendering on the physical touchscreen.
- The local browser companion was online and displaying message and track data.
- Track folders, route preview and local GPX backup workflow were available in
  the browser interface.
- Radio settings can be reached from the radio cards on Home and from the
  selected network's remote Network view.

The recommended stable release remains **v0.8.14** until v0.8.16 completes the
project's broader regression, compatibility and stability acceptance.
