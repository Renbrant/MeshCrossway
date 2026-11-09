# Android chat message actions — source implementation

Status: source integrated into **`main`**, **not included in the already published 0.2.5 APK**.

## Visual cleanup

In MeshCore public channels, a received body may have the format `Specter 6: Thank you` while the UI already displays **Specter 6** as the author. `MessageActions.displayBody` hides only the exact redundant prefix when rendering a received MeshCore group message. It also cleans previews in the conversation list. It never rewrites the stored content, the copy of the original packet, or the data exported in backups. Meshtastic messages, direct messages and sent messages are unchanged.

## Press and hold

Long-press a message bubble, sender name, link or reception label to open a scoped menu:

- **Reply mentioning sender** (received group messages with a known display name): insert `@[name]` as editable text, preserving draft.
- **Reply with reception (ACK)**: insert a **plain-text** reception report using only received SNR, RSSI, reported hops and (if available) the *observed* repeater hashes. Unknown data is omitted; there is never a fabricated LoRa measurement or automatic packet transmission.
- **Quote message in reply**: insert a short quotation in the same conversation.
- **Copy message**, **Copy reception information** (where available), **Message information**, and **Share message** (Android chooser).
- **Add/remove sender favorite**, **Sender contact details** only when sender identification matches a single saved node on the same radio and network. A display-name match on a channel prompts a warning because it is not authenticated proof of identity.
- **Show coordinates on map**, when parseable coordinates appear in the message.

Tap behavior remains unchanged: message reception details and coordinate map links remain available. Long press is handled at both bubble and child TextView levels, instead of Android text-selection mode hijacking the gesture.

The Ack here is an **editable text reply**, distinct from protocol delivery acknowledgements; the app never automatically replies or forwards across the two independent mesh networks. For Meshtastic, an `@mention` is text, and no special notification semantics are assumed.

Reference: [WADAMESH docs](https://wadamesh.com/docs.html) and [WADAMESH changelog (beta_8)](https://wadamesh.com/changelog.html).

## Verification

JUnit regression tests cover sender-prefix rendering, unchanged other messages, unknown names, formatted reception data and absent metadata. Run `cd android && ./gradlew testDebugUnitTest assembleDebug lintDebug` in a supported Android SDK environment. Exercise touch gestures with both real BLE radios and check favorites, drafts, theme backgrounds and saved-message history before creating a **new version** signed with the existing permanent release key. Never overwrite 0.2.5.
