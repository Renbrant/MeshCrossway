#!/usr/bin/env python3
"""Check the Web Flasher hardware picker and, optionally, a packaged T-Deck catalog."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--catalog", type=Path)
args = parser.parse_args()

html = (root / "docs/site/flasher/index.html").read_text()
for needle in ("name=\"board\" value=\"waveshare\" checked", "name=\"board\" value=\"tdeck-plus\"",
               "'tdeck-plus':{catalog:'../flasher/firmware/tdeck-plus/catalog.json'",
               "experimentalOnly:true", "if(!board.experimentalOnly)throw Error('No hardware-tested default available');",
               "data-board=\"tdeck-plus\" hidden", "ticket!==boardGeneration"):
    if needle not in html:
        sys.exit(f"ERROR: Web Flasher hardware picker is missing: {needle}")
# The Waveshare line must still refuse to start without a hardware-tested default.
if not re.search(r"'waveshare':\{[^}]*experimentalOnly:false", html):
    sys.exit("ERROR: Waveshare must keep requiring a hardware-tested default")
print("PASS: Web Flasher hardware picker")

if args.catalog:
    catalog = json.loads(args.catalog.read_text())
    assert catalog["board"] == "tdeck-plus", catalog["board"]
    versions = catalog["versions"]
    assert versions, "T-Deck catalog is empty"
    current = (root / "firmware/tdeck-plus/VERSION").read_text().strip()
    assert catalog["latest"] == current, (catalog["latest"], current)
    tested = [v["version"] for v in versions if v["status"] == "hardware-tested"]
    assert catalog["recommended"] == (tested[-1] if tested else None), "Recommended must be the newest tested build"
    for entry in versions:
        assert entry["status"] in ("experimental", "hardware-tested"), entry
        folder = args.catalog.parent / "versions" / entry["version"]
        manifest = json.loads((folder / "manifest.json").read_text())
        assert manifest["new_install_prompt_erase"] is True, "Manifest must offer the erase choice"
        parts = {(p["path"], p["offset"]) for p in manifest["builds"][0]["parts"]}
        assert parts == {("bootloader.bin", 0), ("partition-table.bin", 0x8000), ("application.bin", 0x10000)}, parts
        info = json.loads((folder / "build-info.json").read_text())
        for part in info["flash_parts"]:
            data = (folder / part["path"]).read_bytes()
            assert hashlib.sha256(data).hexdigest() == part["sha256"], part["path"]
        assert hashlib.sha256((folder / "application.bin").read_bytes()).hexdigest() == entry["sha256"]
    print(f"PASS: T-Deck Plus catalog ({len(versions)} builds, recommended={catalog['recommended']})")
