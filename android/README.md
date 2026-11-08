# MeshCrossway Android

Native portrait controller, **0.3.0-alpha03**: two independent radio clients, unified inbox and map, local phone GPS/POIs, named offline areas, favorites, pinned/muted conversations, notifications and JSON history backup.

See [installation, limits and hardware tests](../docs/android-apk.md), [functional references](../docs/android-feature-references.md) and [issue #189](https://github.com/Renbrant/MeshCrossway/issues/189).

Alpha03 installs alongside alpha02 to preserve existing private history. Disconnect radios in the older app first.

```sh
# JDK 17 + Android SDK 35
./gradlew assembleDebug lintDebug
./gradlew connectedDebugAndroidTest
```
