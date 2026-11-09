#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
set +e
(cd android && ./gradlew --no-daemon connectedDebugAndroidTest)
test_status=$?
set -e
mkdir -p android-device-screenshots
for capture in map-layers map-offline map-tracks home chat-draft chat-restored dual-map settings-data settings-meshtastic settings-meshcore settings-connections settings-notifications settings-map settings-data settings-about; do
    adb pull "/data/local/tmp/mcx-$capture.png" "android-device-screenshots/$capture.png" || true
done
exit "$test_status"
