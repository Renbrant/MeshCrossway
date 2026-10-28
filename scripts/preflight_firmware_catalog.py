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
if "select.value=recommended.version" not in html or "recommended.status!=='hardware-tested'" not in html:
    sys.exit("ERROR: The Web Flasher must select only a hardware-tested default")
if "entry.status!=='hardware-tested'" not in html:
    sys.exit("ERROR: Experimental installs must require opt-in")
for file in ("build_firmware_versions.sh",):
    p=subprocess.run(["bash","-n",str(root/"scripts"/file)],capture_output=True,text=True)
    if p.returncode:sys.exit(p.stderr)
for file in ("write_firmware_catalog.py","write_firmware_version.py","verify_firmware_version.py"):
    compile((root/"scripts"/file).read_text(),file,"exec")
print("PASS: Web Flasher version selector JavaScript and build script syntax")

if args.catalog is None:
    subprocess.run([sys.executable,str(root/"scripts/test_firmware_install.py")],check=True)

if args.catalog is not None:
    path=args.catalog.resolve()
    manifest=json.loads(path.read_text())
    versions=manifest["versions"]
    selected=next((entry for entry in versions if entry["version"]==manifest["recommended"]),None)
    assert selected and selected["status"]=="hardware-tested"
    tested=[entry["version"] for entry in versions if entry["status"]=="hardware-tested"]
    assert manifest["recommended"]==max(tested,key=lambda v:tuple(map(int,v[1:].split("."))))
    found={entry["version"] for entry in versions}
    current=(root/"firmware/display-prototype/VERSION").read_text().strip()
    assert manifest["latest"]==current,(manifest["latest"],current)
    # The flasher is a rolling release line, not a full historical archive.
    assert {manifest["recommended"],current}.issubset(found),found
    assert "v0.6.3" not in found,"Known-broken boot must never appear in selector"
    assert all(tuple(map(int,v[1:].split("."))) >= tuple(map(int,manifest["recommended"][1:].split("."))) for v in found),found
    assert not any(v in found for v in ("v0.3.0","v0.4.0","v0.5.10")),"Old archives must not appear"
    physical={p.name for p in (path.parent/"versions").iterdir() if p.is_dir()}
    assert physical==found,("Archived binaries not in selectable catalog", physical-found,found-physical)
    for entry in versions:
        assert entry["status"] in ("hardware-tested","experimental","known-issue")
        assert entry["manifest"].startswith("/MeshCrossway/flasher/firmware/versions/")
        assert entry["binary"].startswith("/MeshCrossway/flasher/firmware/versions/")
        folder=path.parent/"versions"/entry["version"]
        cmd=[sys.executable,str(root/"scripts/verify_firmware_version.py"),
             "--dir",str(folder),"--version",entry["version"]]
        subprocess.run(cmd,check=True)
    print("PASS: compiled catalog, manifests and immutable binary checksums")
