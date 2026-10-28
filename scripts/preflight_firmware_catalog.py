#!/usr/bin/env python3
"""Validate version selector JS and optionally an assembled firmware archive."""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument("--catalog",type=Path)
args=parser.parse_args()
html=(root/"docs/site/flasher/index.html").read_text()
scripts=re.findall(r"<script(?:\s[^>]*)?>([\s\S]*?)</script>",html)
scripts=[s.strip() for s in scripts if s.strip()]
if not scripts:
    sys.exit("ERROR: no inline Web Flasher JavaScript found")
for n,script in enumerate(scripts):
    p=subprocess.run(["node","--check","-"],input=script,text=True,capture_output=True)
    if p.returncode:
        sys.exit("ERROR: JS script "+str(n)+" invalid:\n"+p.stderr)
if 'firmware-picker' not in html or 'catalog.json' not in html:
    sys.exit("ERROR: version selection UI or catalog loader missing")
for file in ("build_firmware_versions.sh",):
    p=subprocess.run(["bash","-n",str(root/"scripts"/file)],capture_output=True,text=True)
    if p.returncode:sys.exit(p.stderr)
for file in ("write_firmware_catalog.py","write_firmware_version.py","verify_firmware_version.py"):
    compile((root/"scripts"/file).read_text(),file,"exec")
print("PASS: Web Flasher version selector JavaScript and build script syntax")

if args.catalog is not None:
    path=args.catalog.resolve()
    manifest=json.loads(path.read_text())
    versions=manifest["versions"]
    assert manifest["recommended"]=="v0.4.0"
    found={entry["version"] for entry in versions}
    current=(root/"firmware/display-prototype/VERSION").read_text().strip()
    assert manifest["latest"]==current,(manifest["latest"],current)
    assert {"v0.3.0","v0.4.0","v0.5.2",current}.issubset(found),found
    for entry in versions:
        assert entry["status"] in ("hardware-tested","experimental","known-issue")
        assert entry["manifest"].startswith("/MeshCrossway/flasher/firmware/versions/")
        assert entry["binary"].startswith("/MeshCrossway/flasher/firmware/versions/")
        folder=path.parent/"versions"/entry["version"]
        cmd=[sys.executable,str(root/"scripts/verify_firmware_version.py"),
             "--dir",str(folder),"--version",entry["version"]]
        subprocess.run(cmd,check=True)
    print("PASS: compiled catalog, manifests and immutable binary checksums")
