#!/usr/bin/env python3
"""Restore checksum-pinned hardware-tested firmware without recompilation."""
import argparse
import hashlib
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "v0.9.25"
REF = "65f4214b15fe9486a7f07e7471380114bcbf8826"
SHA = "daeebb735024fcdcb474acdfe0887c3d72e608ba794ba1343e1b60749f3d6b23"
APPROVED = {
    VERSION: (REF, SHA, 13461246425, "ac2bb8afc73245cc8d062df3237dd41cb424699ecb0945a4a142db1401896dd0"),
    "v0.9.16": ("d821b78979b3c2b9d716757f0cff02dbf89e776f",
                "54c99f8c30d352c602f4e4d662701e10f2bd85d5a38af14d8190b3cc4693858a",
                13361468018, "71e060213e6e8a8b19fe154f831235b628f99958d6ea8844973db7bc42657017"),
}
NAMES = ("manifest.json", "build-info.json", "meshcrossway-display-dev.bin",
         "bootloader.bin", "partition-table.bin", "application.bin")

def verify(folder, version=VERSION):
    ref, sha, _, _ = APPROVED[version]
    subprocess.run(["python3", str(ROOT / "scripts/verify_firmware_version.py"),
                    "--dir", str(folder), "--version", version, "--ref", ref], check=True)
    actual = hashlib.sha256((folder / "meshcrossway-display-dev.bin").read_bytes()).hexdigest()
    if actual != sha:
        raise ValueError(f"Tested firmware SHA-256 differs from approved {version}")

def restore(destination, version=VERSION):
    _, _, artifact, archive_sha = APPROVED[version]
    with tempfile.TemporaryDirectory(prefix="mcx-stable-") as tmp:
        staged = Path(tmp)
        # Permanent release assets take precedence over expiring Actions artifacts.
        release = subprocess.run(["gh", "release", "download", version,
                                  "--repo", "Renbrant/MeshCrossway", "--dir", str(staged),
                                  *[arg for name in NAMES for arg in ("--pattern", name)]],
                                 capture_output=True)
        if release.returncode == 0:
            verify(staged, version)  # A corrupt existing release must fail closed.
        else:
            for name in NAMES:
                (staged / name).unlink(missing_ok=True)
            archive = staged / "reviewed.zip"
            with archive.open("wb") as out:
                subprocess.run(["gh", "api", f"repos/Renbrant/MeshCrossway/actions/artifacts/{artifact}/zip"],
                               stdout=out, check=True)
            if hashlib.sha256(archive.read_bytes()).hexdigest() != archive_sha:
                raise ValueError("Reviewed firmware archive digest mismatch")
            with zipfile.ZipFile(archive) as z:
                for name in NAMES:
                    (staged / name).write_bytes(z.read(f"versions/{version}/{name}"))
            verify(staged, version)
        destination.mkdir(parents=True, exist_ok=True)
        for name in NAMES:
            shutil.copy2(staged / name, destination / name)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--version", choices=APPROVED, default=VERSION)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--destination", type=Path)
    mode.add_argument("--verify", type=Path)
    args = p.parse_args()
    if args.verify:
        verify(args.verify, args.version)
    else:
        restore(args.destination, args.version)
