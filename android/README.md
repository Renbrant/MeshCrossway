# MeshCrossway Android

Native portrait controller, **0.2.0-alpha02**: independent Meshtastic/MeshCore text clients, nodes/channels, private persistent history, offline drafts, unread counters, search and automatic reconnect. Maps/GPS/tracks remain planned.

See [installation, limits and hardware tests](../docs/android-apk.md) and [issue #187](https://github.com/Renbrant/MeshCrossway/issues/187).

```sh
# JDK 17 + Android SDK 35
./gradlew assembleDebug lintDebug
./gradlew connectedDebugAndroidTest  # emulator/device attached
```
