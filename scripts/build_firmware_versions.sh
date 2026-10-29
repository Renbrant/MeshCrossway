#!/usr/bin/env bash
# Build only the latest tested release and newer supported candidates.
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

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

prepare_waveshare() {
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
  
}

build_version() {
  local version="$1"
  local ref="$2"
  local source_root="$ROOT"
  local image_dir="$OUT/versions/$version"
  local vendor="$TEMP/vendor-$version"
  local image_rel="$OUT_REL/versions/$version/meshcrossway-display-dev.bin"
  mkdir -p "$image_dir"
  # An immutable version can use a previous successful GitHub Actions cache.
  if [ -s "$image_dir/meshcrossway-display-dev.bin" ] &&
     [ -s "$image_dir/manifest.json" ] && [ -s "$image_dir/build-info.json" ]; then
    local verify_args=(--dir "$image_dir" --version "$version")
    if [ "$ref" != "HEAD" ]; then verify_args+=(--ref "$ref"); fi
    if python3 "$ROOT/scripts/verify_firmware_version.py" "${verify_args[@]}" &&
       { [ "$version" != "$STABLE_VERSION" ] ||
         echo "$STABLE_SHA256  $image_dir/meshcrossway-display-dev.bin" | sha256sum -c -; }; then
      echo "RESTORED: $version from verified compiled artifact cache"
      return
    fi
  fi

  # Preserve the exact owner-tested bytes. Rebuilding an old source revision
  # with newly resolved component versions is not an approved rollback image.
  if [ "$version" = "$STABLE_VERSION" ] && [ "$ref" = "$STABLE_REF" ]; then
    local approved_temp
    approved_temp="$(mktemp -d "$TEMP/approved-XXXXXX")"
    local approved_url="https://renbrant.github.io/MeshCrossway/flasher/firmware/versions/$version"
    local file
    for file in build-info.json manifest.json meshcrossway-display-dev.bin bootloader.bin partition-table.bin application.bin; do
      curl --fail --location --retry 3 "$approved_url/$file" -o "$approved_temp/$file"
    done
    python3 "$ROOT/scripts/verify_firmware_version.py" --dir "$approved_temp" --version "$version" --ref "$ref"
    echo "$STABLE_SHA256  $approved_temp/meshcrossway-display-dev.bin" | sha256sum -c -
    cp "$approved_temp/"* "$image_dir/"
    rm -rf -- "$approved_temp"
    echo "RESTORED: immutable owner-tested $version"
    return
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
  prepare_waveshare
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
  # Defaults are requests; only the generated file proves these choices took effect.
  if [ -f "$source_root/scripts/check_ble_sdkconfig.py" ]; then
    python3 "$source_root/scripts/check_ble_sdkconfig.py" \
      "$vendor/examples/esp-idf/02_lvgl_example/sdkconfig"
  fi
  test -s "$image_dir/meshcrossway-display-dev.bin"
  python3 "$ROOT/scripts/write_firmware_version.py" \
    --version "$version" --ref "$(git -C "$source_root" rev-parse HEAD)" \
    --folder "$image_dir"
  echo "SUCCESS: $version $(wc -c < "$image_dir/meshcrossway-display-dev.bin") bytes"
}

# Only one physical rollback and recent candidates are delivered in the Web Flasher.
# Do not rebuild or publish the pre-stable (v0.3 / v0.4 / v0.5) archive.
# Update these two constants whenever the owner validates a newer hardware build.
STABLE_VERSION="v0.6.2"
STABLE_REF="a508c7d9a9eaf580061d2daca6838ecb78f89085"
STABLE_SHA256="11b253c3102c9e7b8d07b8899faf8171e8ddffe9fa6f6c98aaddb288258999a5"

version_gte() {
  # 0 = first argument is at least the second (GNU coreutils version sort).
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | tail -n 1)" = "$1" ]
}

if ! version_gte "$LATEST_VERSION" "$STABLE_VERSION"; then
  echo "::error::LATEST_VERSION is below the approved stable release."
  exit 1
fi
if [ "$LATEST_VERSION" = "v0.6.3" ] || [ "$LATEST_VERSION" = "v0.6.4" ]; then
  echo "::error::v0.6.3/v0.6.4 are known-broken BLE releases and cannot be published."
  exit 1
fi

# Version pin for an owner-approved image. The file and SHA remain immutable.
if [ "$LATEST_VERSION" != "$STABLE_VERSION" ]; then
  build_version "$STABLE_VERSION" "$STABLE_REF"
fi

# Compile or reuse the latest image (may be stable or experimental).
if [ "$LATEST_VERSION" = "$STABLE_VERSION" ]; then
  build_version "$STABLE_VERSION" HEAD
else
  build_version "$LATEST_VERSION" HEAD
fi

# The Pages artifact must contain only the currently supported release line,
# even when a broad Actions cache restored historical directories.
for archive in "$OUT"/versions/*; do
  [ -d "$archive" ] || continue
  version="$(basename "$archive")"
  if [ "$version" = "$STABLE_VERSION" ] || [ "$version" = "$LATEST_VERSION" ]; then
    continue
  fi
  rm -rf -- "$archive"
done

python3 "$ROOT/scripts/write_firmware_catalog.py" \
  --root "$OUT" --latest "$LATEST_VERSION" --stable "$STABLE_VERSION"
echo "Minimal firmware release line ready: stable $STABLE_VERSION, newest $LATEST_VERSION"
