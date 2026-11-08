#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
classes="$(mktemp -d)"
trap 'rm -rf "$classes"' EXIT
javac -d "$classes" "$root/android/app/src/main/java/com/meshcrossway/android/ProtocolProbe.java" "$root/android/app/src/test/java/com/meshcrossway/android/ProtocolProbeTest.java"
java -cp "$classes" com.meshcrossway.android.ProtocolProbeTest
