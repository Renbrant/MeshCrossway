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
    # Historical stable archives can carry false before safe publication rewrite.
    # Final deploy verification requires true (see preflight_firmware_catalog.py).
    assert isinstance(manifest.get("new_install_prompt_erase"),bool)
    data=image.read_bytes()
    if tuple(map(int,a.version[1:].split('.'))) >= (0,5,8):
        expected=[("bootloader.bin",0,0x8000),("partition-table.bin",0x8000,0x9000),("application.bin",0x10000,len(data))]
        assert manifest["builds"][0]["parts"]==[{"path":name,"offset":start} for name,start,end in expected]
        assert manifest["new_install_prompt_erase"] is False
        assert len(info["flash_parts"])==3
        for metadata,(name,start,end) in zip(info["flash_parts"],expected):
            content=(folder/name).read_bytes()
            assert content==data[start:end] and len(content)==end-start
            assert end<=0x9000 or start>=0x10000
            assert metadata=={"path":name,"offset":start,"size":len(content),"sha256":hashlib.sha256(content).hexdigest()}
    else:
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
