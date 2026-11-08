#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
classes="$(mktemp -d)"
trap 'rm -rf "$classes"' EXIT
src="$root/android/app/src/main/java/com/meshcrossway/android"
tests="$root/android/app/src/test/java/com/meshcrossway/android"
javac -d "$classes" "$src/ProtocolProbe.java" "$src/RadioProfile.java" "$src/Wire.java" "$src/MeshData.java" "$src/ProtocolClient.java" "$tests/ProtocolProbeTest.java" "$tests/ProtocolClientTest.java"
java -cp "$classes" com.meshcrossway.android.ProtocolProbeTest
java -cp "$classes" com.meshcrossway.android.ProtocolClientTest
