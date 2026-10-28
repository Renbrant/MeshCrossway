#!/usr/bin/env python3
"""Write a version-specific ESP Web Tools manifest after compilation succeeds."""
import argparse
import hashlib
import json
import re
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("--version",required=True)
parser.add_argument("--ref",required=True)
parser.add_argument("--folder",required=True)
args=parser.parse_args()
if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+",args.version):
    parser.error("Version must be vMAJOR.MINOR.PATCH")
if not re.fullmatch(r"[a-f0-9]{40}",args.ref):
    parser.error("Ref must be the full immutable Git commit hash")
folder=Path(args.folder)
binary=folder/"meshcrossway-display-dev.bin"
if not binary.is_file() or binary.stat().st_size<10000:
    parser.error(f"Firmware binary missing or suspiciously small: {binary}")
sha256=hashlib.sha256(binary.read_bytes()).hexdigest()
manifest={
    "name":f"MeshCrossway Display {args.version}",
    "version":args.version,
    "new_install_prompt_erase":True,
    "builds":[
        {"chipFamily":"ESP32-S3","parts":[
            {"path":"meshcrossway-display-dev.bin","offset":0}
        ]}
    ]
}
info={
    "version":args.version,
    "commit":args.ref,
    "target":"Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 (non-B)",
    "size":binary.stat().st_size,
    "sha256":sha256
}
(folder/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
(folder/"build-info.json").write_text(json.dumps(info,indent=2)+"\n")
print(f"Manifest ready: {args.version} / {sha256[:12]}")
