#!/usr/bin/env python3
"""Guard against publishing a firmware binary with a stale embedded UI version."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument("--root",default="_site/flasher/firmware")
p.add_argument("--version-file",default="firmware/display-prototype/VERSION")
p.add_argument("--ui-header",default="firmware/display-prototype/meshcrossway_ui.h")
a=p.parse_args()
version=Path(a.version_file).read_text().strip()
header=Path(a.ui_header).read_text()
expected=version.lstrip("v") if tuple(map(int,version.lstrip("v").split("."))) >= (0,7,8) else "DEV "+version.lstrip("v")
if f'"{expected}"' not in header:
    p.error(f"Header does not contain the expected UI label {expected!r}")
image_dir=Path(a.root)/"versions"/version
binary=image_dir/"meshcrossway-display-dev.bin"
info=json.loads((image_dir/"build-info.json").read_text())
if info["version"]!=version:p.error("Metadata version mismatch")
data=binary.read_bytes()
if expected.encode() not in data:
    p.error(f"Embedded UI version {expected!r} is MISSING from the compiled binary")
# Guard against a specific stale-version regression which the user observed.
for older in ("DEV 0.4","DEV 0.5.0"):
    if older!=expected and older.encode()+b"\x00" in data:
        p.error(f"Compiled image unexpectedly contains old UI label {older!r}")
print(f"PASS: {version} binary contains embedded UI label {expected!r} ({len(data)} bytes)")

