#!/usr/bin/env bash
# Rebuild a selectable firmware archive from immutable, previously tested commits.
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

V03_REF="b48145f07ef4d00895f7ccedbdfb240a322dd37a"
V04_REF="5dfe3744ed7b062c629ea5d2811dc434928826b1"
WAVESHARE_REF="d9ff7837148b47b33edadc4b87775c341cf35bbe"
LATEST_VERSION="$(tr -d '\r\n' < firmware/display-prototype/VERSION)"
if [[ ! "$LATEST_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "::error::Invalid current firmware version $LATEST_VERSION"
  exit 1
fi

OUT="$(printenv MESH_FIRMWARE_OUTPUT_DIR || true)"
if [ -z "$OUT" ]; then OUT="$ROOT/_site/flasher/firmware"; fi
mkdir -p "$OUT/versions"
OUT="$(cd "$OUT" && pwd)"
case "$OUT" in
  "$ROOT"/*) ;;
  *) echo "::error::Firmware output must be inside repository root"; exit 1 ;;
esac
OUT_REL="$(realpath --relative-to="$ROOT" "$OUT")"
TEMP="$ROOT/_firmware_version_build"
mkdir -p "$TEMP"
TEMPLATE="$TEMP/waveshare-template"

if [ ! -d "$TEMPLATE/.git" ]; then
  echo "Preparing pinned Waveshare BSP $WAVESHARE_REF"
  mkdir -p "$TEMPLATE"
  git -C "$TEMPLATE" init -q
  git -C "$TEMPLATE" remote add origin https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5.git
  git -C "$TEMPLATE" fetch --depth 1 origin "$WAVESHARE_REF"
  git -C "$TEMPLATE" checkout --detach FETCH_HEAD
  git -C "$TEMPLATE" submodule update --init --recursive --depth 1
fi
test "$(git -C "$TEMPLATE" rev-parse HEAD)" = "$WAVESHARE_REF"

build_version() {
  local version="$1"
  local ref="$2"
  local source_root="$ROOT"
  local image_dir="$OUT/versions/$version"
  local vendor="$TEMP/vendor-$version"
  local image_rel="$OUT_REL/versions/$version/meshcrossway-display-dev.bin"
  mkdir -p "$image_dir"
  # An immutable version can use a previous successful GitHub Actions cache.
  if [ "$ref" != "HEAD" ] && [ -s "$image_dir/meshcrossway-display-dev.bin" ] &&
     [ -s "$image_dir/manifest.json" ] && [ -s "$image_dir/build-info.json" ]; then
    if python3 "$ROOT/scripts/verify_firmware_version.py" --dir "$image_dir" --version "$version" --ref "$ref"; then
      echo "RESTORED: $version from verified immutable cache"
      return
    fi
  fi

  if [ "$ref" != "HEAD" ]; then
    source_root="$TEMP/source-$version"
    git worktree add --detach "$source_root" "$ref"
  fi
  local recorded
  recorded="$(tr -d '\r\n' < "$source_root/firmware/display-prototype/VERSION")"
  if [ "$recorded" != "$version" ]; then
    echo "::error::Expected $version but source VERSION is $recorded"
    exit 1
  fi
  mkdir -p "$vendor"
  cp -a "$TEMPLATE/." "$vendor/"
  python3 "$source_root/firmware/display-prototype/apply_to_waveshare.py" "$vendor"

  local vendor_rel
  vendor_rel="$(realpath --relative-to="$ROOT" "$vendor")"
  echo "Compiling $version from $ref (ESP-IDF 5.3.2)"
  docker run --rm -v "$ROOT:/workspace" \
    -w "/workspace/$vendor_rel/examples/esp-idf/02_lvgl_example" \
    espressif/idf:v5.3.2 /bin/bash -lc "
      set -e
      idf.py set-target esp32s3
      idf.py build
      idf.py merge-bin -o '/workspace/$image_rel'
    "
  test -s "$image_dir/meshcrossway-display-dev.bin"
  python3 "$ROOT/scripts/write_firmware_version.py" \
    --version "$version" --ref "$(git -C "$source_root" rev-parse HEAD)" \
    --folder "$image_dir"
  echo "SUCCESS: $version $(wc -c < "$image_dir/meshcrossway-display-dev.bin") bytes"
}

build_version v0.3.0 "$V03_REF"
build_version v0.4.0 "$V04_REF"
case "$LATEST_VERSION" in
  v0.3.0|v0.4.0) echo "Current version already available in historical archive" ;;
  *) build_version "$LATEST_VERSION" HEAD ;;
esac
python3 "$ROOT/scripts/write_firmware_catalog.py" \
  --root "$OUT" --latest "$LATEST_VERSION"
echo "Firmware version catalog ready in $OUT"
