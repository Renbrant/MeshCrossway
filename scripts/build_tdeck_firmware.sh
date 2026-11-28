#!/usr/bin/env bash
# Compile the LilyGO T-Deck Plus controller and stage it for the Web Flasher.
# Every build is published EXPERIMENTAL until write_tdeck_firmware.py records a test.
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"
VERSION="$(tr -d '\r\n' < firmware/tdeck-plus/VERSION)"
if [[ ! "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "::error::Invalid T-Deck firmware version $VERSION"; exit 1
fi
OUT="${MESH_TDECK_OUTPUT_DIR:-$ROOT/_site/flasher/firmware/tdeck-plus}"
mkdir -p "$OUT"
if [ -n "${IDF_PATH:-}" ] && command -v idf.py >/dev/null; then
  (cd firmware/tdeck-plus && idf.py set-target esp32s3 && idf.py build)
else
  docker run --rm -v "$ROOT:/workspace" -w /workspace/firmware/tdeck-plus espressif/idf:v5.3.2 \
    /bin/bash -lc "set -e; git config --global --add safe.directory '*'; idf.py set-target esp32s3; idf.py build; idf.py size"
fi
python3 scripts/write_tdeck_firmware.py --build firmware/tdeck-plus/build \
  --version "$VERSION" --ref "$(git rev-parse HEAD)" --output "$OUT"
