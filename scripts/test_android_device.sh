#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
set +e
(cd android && ./gradlew --no-daemon connectedDebugAndroidTest)
test_status=$?
set -e
mkdir -p android-device-screenshots
for capture in home chat-draft chat-restored; do
    adb pull "/data/local/tmp/mcx-$capture.png" "android-device-screenshots/$capture.png" || true
done
exit "$test_status"
