#!/usr/bin/env python3
"""Restore the existing hardware-tested v0.9.16 release without rebuilding it.

The already published firmware catalog is the source of the tested binary's
SHA-256. If that release is unavailable or inconsistent, deployment fails closed.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "v0.9.16"
REF = "b225afb57f26196853c57aaad20f85450d69b126"
BASE = "https://renbrant.github.io/MeshCrossway/flasher/firmware/"
NAMES = ("manifest.json", "build-info.json", "meshcrossway-display-dev.bin",
         "bootloader.bin", "partition-table.bin", "application.bin")

def get(path):
    with urllib.request.urlopen(
        urllib.request.Request(BASE + path, headers={"User-Agent": "MeshCrossway-CI/1"}),
        timeout=30,
    ) as response:
        if not response.url.startswith("https://renbrant.github.io/MeshCrossway/"):
            raise ValueError("Unexpected download host")
        data = response.read(8 * 1024 * 1024 + 1)
    if not data or len(data) > 8 * 1024 * 1024:
        raise ValueError("Empty or oversized firmware asset")
    return data

def published_sha():
    catalog = json.loads(get("catalog.json"))
    entries = [v for v in catalog.get("versions", []) if v.get("version") == VERSION]
    if len(entries) != 1:
        raise ValueError("Published v0.9.16 is missing or ambiguous")
    sha = entries[0].get("sha256", "")
    if len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
        raise ValueError("Invalid published v0.9.16 SHA-256")
    return sha

def verify(folder):
    subprocess.run(["python3", str(ROOT / "scripts/verify_firmware_version.py"),
                    "--dir", str(folder), "--version", VERSION, "--ref", REF],
                   check=True)
    actual = hashlib.sha256((folder / "meshcrossway-display-dev.bin").read_bytes()).hexdigest()
    if actual != published_sha():
        raise ValueError("Stable binary differs from published hardware-tested v0.9.16")

def restore(destination):
    with tempfile.TemporaryDirectory(prefix="mcx-stable-") as tmp:
        staged = Path(tmp)
        for name in NAMES:
            (staged / name).write_bytes(get(f"versions/{VERSION}/{name}"))
        verify(staged)
        destination.mkdir(parents=True, exist_ok=True)
        for name in NAMES:
            shutil.copy2(staged / name, destination / name)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--destination", type=Path)
    mode.add_argument("--verify", type=Path)
    args = p.parse_args()
    if args.verify:
        verify(args.verify)
    else:
        restore(args.destination)
