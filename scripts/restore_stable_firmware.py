#!/usr/bin/env python3
"""Restore immutable v0.9.16 from its successful reviewed firmware archive."""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "v0.9.16"
REF = "d821b78979b3c2b9d716757f0cff02dbf89e776f"
SHA = "54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a"
ARTIFACT = 13361468018
ARCHIVE_SHA = "71e060213e6e8a8b19fe154f831235b628f99958d6ea8844973db7bc42657017"
NAMES = ("manifest.json", "build-info.json", "meshcrossway-display-dev.bin",
         "bootloader.bin", "partition-table.bin", "application.bin")

def verify(folder):
    subprocess.run(["python3", str(ROOT / "scripts/verify_firmware_version.py"),
                    "--dir", str(folder), "--version", VERSION, "--ref", REF],
                   check=True)
    actual = hashlib.sha256((folder / "meshcrossway-display-dev.bin").read_bytes()).hexdigest()
    if actual != SHA:
        raise ValueError("Stable firmware SHA-256 differs from archived tested v0.9.16")

def restore(destination):
    with tempfile.TemporaryDirectory(prefix="mcx-stable-") as tmp:
        staged = Path(tmp)
        archive = staged / "reviewed.zip"
        with archive.open("wb") as out:
            subprocess.run(["gh", "api",
                            f"repos/Renbrant/MeshCrossway/actions/artifacts/{ARTIFACT}/zip"],
                           stdout=out, check=True)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA:
            raise ValueError("Reviewed firmware archive digest mismatch")
        with zipfile.ZipFile(archive) as z:
            for name in NAMES:
                (staged / name).write_bytes(z.read(f"versions/{VERSION}/{name}"))
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
