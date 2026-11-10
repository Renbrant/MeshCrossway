#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
classes="$(mktemp -d)"
trap 'rm -rf "$classes"' EXIT
src="$root/android/app/src/main/java/com/meshcrossway/android"
tests="$root/android/app/src/test/java/com/meshcrossway/android"
javac -d "$classes" "$src/ProtocolProbe.java" "$src/RadioProfile.java" "$src/Wire.java" "$src/MeshData.java" "$src/ProtocolClient.java" "$src/MtAdmin.java" "$src/MtLora.java" "$src/MtMetrics.java" "$src/GeoMath.java" "$src/TrafficBudget.java" "$src/ConnectionGate.java" "$src/UiFeed.java" "$src/RxInfo.java" "$src/FieldTools.java" "$tests/ProtocolProbeTest.java" "$tests/ProtocolClientTest.java" "$tests/FieldFeaturesTest.java" "$tests/FieldToolsTest.java" "$tests/RxInfoTest.java" "$tests/MtSettingsTest.java"
java -cp "$classes" com.meshcrossway.android.ProtocolProbeTest
java -cp "$classes" com.meshcrossway.android.ProtocolClientTest

java -cp "$classes" com.meshcrossway.android.FieldFeaturesTest

java -cp "$classes" com.meshcrossway.android.FieldToolsTest

java -cp "$classes" com.meshcrossway.android.RxInfoTest

java -cp "$classes" com.meshcrossway.android.MtSettingsTest
