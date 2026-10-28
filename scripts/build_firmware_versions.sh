#!/usr/bin/env bash
# Rebuild a selectable firmware archive from immutable, previously tested commits.
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

V03_REF="b48145f07ef4d00895f7ccedbdfb240a322dd37a"
V04_REF="5dfe3744ed7b062c629ea5d2811dc434928826b1"
V052_REF="ce221fcf3d0ba1524ab2cea7e6296b584a97ff71"
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
    if python3 "$ROOT/scripts/verify_firmware_version.py" "${verify_args[@]}"; then
      echo "RESTORED: $version from verified compiled artifact cache"
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
  test -s "$image_dir/meshcrossway-display-dev.bin"
  python3 "$ROOT/scripts/write_firmware_version.py" \
    --version "$version" --ref "$(git -C "$source_root" rev-parse HEAD)" \
    --folder "$image_dir"
  echo "SUCCESS: $version $(wc -c < "$image_dir/meshcrossway-display-dev.bin") bytes"
}

build_version v0.3.0 "$V03_REF"
build_version v0.4.0 "$V04_REF"
# Preserve the exact hardware-tested v0.5.2 bytes, rather than recompiling rollback.
python3 - "$OUT/versions/v0.5.2" <<'RESTORE_V052'
import hashlib
import json
import sys
import urllib.request
from pathlib import Path
folder=Path(sys.argv[1])
expected="293c3a363b4aa686ce7e31d1fcb41576f2cd412501fade09c25b4b6287619bbf"
base="https://renbrant.github.io/MeshCrossway/flasher/firmware/versions/v0.5.2/"
folder.mkdir(parents=True,exist_ok=True)
image=folder/"meshcrossway-display-dev.bin"
files=("manifest.json","build-info.json",image.name)
if not (all((folder/name).is_file() for name in files) and
        hashlib.sha256(image.read_bytes()).hexdigest()==expected):
    # Fetch all bytes before replacing anything; unavailable recovery aborts publication.
    data={}
    for name in files:
        with urllib.request.urlopen(base+name,timeout=60) as response:
            data[name]=response.read()
    if hashlib.sha256(data[image.name]).hexdigest()!=expected:
        sys.exit("REJECT: published v0.5.2 differs from hardware-tested checksum")
    for name,content in data.items():
        (folder/name).write_bytes(content)
info=json.loads((folder/"build-info.json").read_text())
assert info["sha256"]==expected
print("Preserved exact display-tested v0.5.2 recovery image:",expected)
RESTORE_V052
python3 "$ROOT/scripts/verify_firmware_version.py" --dir "$OUT/versions/v0.5.2" --version v0.5.2 --ref "$V052_REF"
# Preserve the exact hardware-tested v0.5.4 bytes, rather than recompiling rollback.
python3 - "$OUT/versions/v0.5.4" <<'RESTORE_V054'
import hashlib
import json
import sys
import urllib.request
from pathlib import Path
folder=Path(sys.argv[1])
expected="da225e83bea590498069e408c793b3d633fa827678d197fb622ed0122dc6643c"
base="https://renbrant.github.io/MeshCrossway/flasher/firmware/versions/v0.5.4/"
folder.mkdir(parents=True,exist_ok=True)
image=folder/"meshcrossway-display-dev.bin"
files=("manifest.json","build-info.json",image.name)
if not (all((folder/name).is_file() for name in files) and
        hashlib.sha256(image.read_bytes()).hexdigest()==expected):
    # Fetch all bytes before replacing anything; unavailable recovery aborts publication.
    data={}
    for name in files:
        with urllib.request.urlopen(base+name,timeout=60) as response:
            data[name]=response.read()
    if hashlib.sha256(data[image.name]).hexdigest()!=expected:
        sys.exit("REJECT: published v0.5.4 differs from hardware-tested checksum")
    for name,content in data.items():
        (folder/name).write_bytes(content)
info=json.loads((folder/"build-info.json").read_text())
assert info["sha256"]==expected
print("Preserved exact display-tested v0.5.4 recovery image:",expected)
RESTORE_V054
python3 "$ROOT/scripts/verify_firmware_version.py" --dir "$OUT/versions/v0.5.4" --version v0.5.4 --ref c7de2e97e0d63a3ca9a0e2094de1706cf02d599c
case "$LATEST_VERSION" in
  v0.3.0|v0.4.0|v0.5.2|v0.5.4) echo "Current version already available in historical archive" ;;
  *) build_version "$LATEST_VERSION" HEAD ;;
esac
python3 "$ROOT/scripts/write_firmware_catalog.py" \
  --root "$OUT" --latest "$LATEST_VERSION"
echo "Firmware version catalog ready in $OUT"
