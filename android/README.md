# MeshCrossway Android

Native portrait controller, **0.5.0-alpha06**: two independent radio clients, unified inbox and map, local phone GPS/POIs, named offline areas, favorites, pinned/muted conversations, notifications and JSON history backup. Alpha05 adds local GPS tracks with a recording notification, pause/resume, colors/visibility, GPX export, contact-type filters, distance/bearing tools, channel unread/activity order and editable quick replies.

See [installation, limits and hardware tests](../docs/android-apk.md), [functional references](../docs/android-feature-references.md) and [issue #189](https://github.com/Renbrant/MeshCrossway/issues/189).

Alpha04 remains the hardware-tested recovery reference. Alpha05 uses the same package as alpha04; CI signing keys may differ across runs. Export JSON history and any GPX tracks before uninstalling an incompatible build. Disconnect radios in the older app first.

```sh
# JDK 17 + Android SDK 35
./gradlew assembleDebug lintDebug
./gradlew connectedDebugAndroidTest
```

Alpha06 reorganizes navigation into five primary tabs and grouped settings with per-radio pages, consistent Back navigation, compact rows and a full-width map. Alpha05 remains the latest user-reported functional recovery reference.
