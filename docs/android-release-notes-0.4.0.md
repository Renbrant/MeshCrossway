# MeshCrossway Android 0.4.0 beta — Reliable Messaging & LoRa Reception

**Beta / field-testing release · Stage 1 of [enhancement roadmap #229](https://github.com/Renbrant/MeshCrossway/issues/229)**

This is a major beta update to MeshCrossway for Android, focused on making two independent mesh radio connections easier to understand and outgoing messages safer to manage.

## What's new

- **Accurate Bluetooth signal indicators:** each radio has its own phone-to-radio BLE RSSI; readings older than 90 seconds are marked as cached **Last BLE**, with age displayed. Bluetooth signal strength is not LoRa reception quality.
- **Clear LoRa reception details:** incoming message information describes the actual source of reported RSSI, SNR, and hop information. Missing measurements are not guessed. A Meshtastic MQTT flag does not guarantee local RF reception.
- **Reception history by node:** open **Nodes → choose a node → Reception history** for bounded RSSI/SNR charts derived from existing received-message history. Where the sender's full identity is unknown, the app declines to attribute measurements to a particular node.
- **Independent outgoing status indicators:** Home and Chats show separate recent message states for Meshtastic and MeshCore, distinguishing saved locally, sent via Bluetooth, radio acceptance, peer acknowledgement, rejection and unknown delivery. A peer ACK is **not** a human read receipt.
- **Safer recovery from interrupted sends:** for a rejected or uncertain outgoing message, long-press and select **Prepare as new draft (manual)**. You must confirm a possible duplicate, and the app checks the original radio, network, channel/recipient and updated delivery status before copying the text. **Nothing is automatically resent.**

Other existing features remain: simultaneous Meshtastic and MeshCore BLE connections to **separate physical radios**, contacts, channel management and link/QR sharing, chat, local browser access, offline map regions, GPS tracks/GPX, backups, appearance settings, and optional account features. **The networks are never bridged or relayed into each other.**

## Installation and data safety

- Android 8.0+ (ARM64); Android package: `com.meshcrossway.mobile`
- App version: **0.4.0**, Android `versionCode` **34** (previous signed beta `0.3.12` used 33)
- Uses the same permanent release signing certificate, verified against the repository's expected SHA-256 signing fingerprint.
- **Install the new signed APK directly over your existing 0.3.12 (or other compatible permanently signed build). Choose Update; do not uninstall or clear app data.** Your locally stored messages, saved maps, tracks, settings, and backups should remain intact.
- The Android version **0.4.0** and new GitHub release tag `android-v0.4.0-stage1` are intentional: the historical tag `android-v0.4.0` belongs to a mistakenly numbered earlier beta (versionCode 31). That historical artifact is retained unchanged. **Use this release's APK and checksum, not the old `android-v0.4.0` file.**
- APK file: `MeshCrossway-0.4.0.apk`. Verify with included `SHA256SUMS.txt` and `SIGNING-CERT-SHA256.txt`. Do not install unsigned/debug APKs over the permanently signed application.

## Testing and limitations

Android automated unit/fixture tests, Android 29/35 emulator smoke tests, build and lint are release gates. **Live dual-radio, QR interoperability, Bluetooth dropouts and delivery outcomes still require hands-on confirmation on real phones and compatible MT/MC radios.** Some firmware versions may omit RSSI/SNR or ACK details. RF charts are historical message metadata, not a spectrum analyzer, live topology map or guaranteed end-to-end delivery estimate.

This beta is experimental and **not certified for emergency services or life-safety use**. Report reproducible issues at [MeshCrossway Issues](https://github.com/Renbrant/MeshCrossway/issues), including Android version, phone model, both radio models, firmware versions, and the exact action involved. Never include private messages, channel keys, authentication tokens or precise location in a public issue.
