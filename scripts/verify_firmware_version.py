#!/usr/bin/env python3
"""Reject missing, mismatched or corrupted firmware binaries/ESP Web Tools manifests."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument("--dir",required=True)
p.add_argument("--version",required=True)
p.add_argument("--ref")
a=p.parse_args()
folder=Path(a.dir)
try:
    manifest=json.loads((folder/"manifest.json").read_text())
    info=json.loads((folder/"build-info.json").read_text())
    image=folder/"meshcrossway-display-dev.bin"
    assert manifest["version"]==a.version
    assert info["version"]==a.version
    assert manifest["builds"][0]["chipFamily"]=="ESP32-S3"
    assert manifest["builds"][0]["parts"]==[{"path":image.name,"offset":0}]
    if a.ref is not None:
        assert info["commit"]==a.ref
    assert image.is_file() and image.stat().st_size==info["size"]
    assert image.stat().st_size>10000
    assert hashlib.sha256(image.read_bytes()).hexdigest()==info["sha256"]
except (AssertionError,KeyError,ValueError,FileNotFoundError,IndexError) as error:
    print(f"REJECT {a.version}: invalid metadata or binary: {error}",file=sys.stderr)
    sys.exit(1)
print(f"PASS {a.version}: {info['size']} bytes; sha256 {info['sha256'][:12]}")
