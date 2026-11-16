#!/usr/bin/env bash
# Fails if any 64-bit native library in an APK is not 16 KB page-size aligned.
# Google Play requires 16 KB support for apps targeting Android 15+ (API 35+).
# Usage: scripts/check_android_16kb.sh path/to/app.apk
set -euo pipefail

APK="${1:?usage: check_android_16kb.sh app.apk}"
test -s "$APK" || { echo "APK not found: $APK"; exit 1; }
command -v readelf >/dev/null || { echo "readelf (binutils) is required"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
unzip -q -o "$APK" 'lib/arm64-v8a/*.so' 'lib/x86_64/*.so' -d "$WORK" 2>/dev/null || true

shopt -s nullglob
libs=("$WORK"/lib/arm64-v8a/*.so "$WORK"/lib/x86_64/*.so)
if [ ${#libs[@]} -eq 0 ]; then
  echo "No 64-bit native libraries in $APK; nothing to check."
  exit 0
fi

failed=0
for lib in "${libs[@]}"; do
  name="${lib#"$WORK"/}"
  bad=0
  # Every PT_LOAD segment must be aligned to at least 2**14 (0x4000).
  while read -r align; do
    value=$((align))
    if [ "$value" -lt 16384 ]; then
      echo "NOT 16 KB aligned: $name (LOAD align $align)"
      bad=1
      break
    fi
  done < <(readelf -lW "$lib" | awk '$1=="LOAD"{print $NF}')
  if [ "$bad" -eq 0 ]; then echo "ok  $name"; else failed=1; fi
done

if [ "$failed" -ne 0 ]; then
  echo "One or more native libraries do not support 16 KB page sizes."
  exit 1
fi
echo "All 64-bit native libraries are 16 KB aligned."
