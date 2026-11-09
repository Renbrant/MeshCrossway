#!/usr/bin/env python3
"""Restore checksum-pinned hardware-tested firmware without recompilation."""
import argparse
import hashlib
import shutil
import subprocess
import tempfile
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "v0.10.5"
REF = "4afece0daa608dc2675f1d84eb383bf13931c0ff"
SHA = "04ff93cba20f09d53a338a15d8557b40474bb38089a78512cb9e8ae099dc8f10"
APPROVED = {
    VERSION: (REF, SHA, 13680863905, "8dcc2cbed7b8e94c324055cc127ca6ef1fe56d1697a3d17f0ee791b568a79f05"),
    "v0.9.32": ("c83f21adc5a3b35486ed7bd70fa6e5260cc13cce",
                "87abaeb2b6bb9e954e752f9e610cd1150e3ebe7c8d11fe0c2652b98aa06daf2c",
                13551122742, "6278a40dfcedca12c421265e8d6cd27141fa3cbf6d80610e50d6b3a10cb1c35c"),
    "v0.9.28": ("37a8caca1ce57e2795be78e7a40012890a6b7147",
                "a36970339c00cc327515eac2648c5421e1b9aab460484919a6ed8f57cc040c61",
                13471144384, "1e6d628a0cb3c3a3508818349c05b03a3f4726d8ce1a032cba8cdc5fe350c58a"),
    "v0.9.25": ("65f4214b15fe9486a7f07e7471380114bcbf8826",
                "daeebb735024fcdcb474acdfe0887c3d72e608ba794ba1343e1b60749f3d6b23",
                13461246425, "ac2bb8afc73245cc8d062df3237dd41cb424699ecb0945a4a142db1401896dd0"),
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
                if "artifact.tar" in z.namelist():
                    with z.open("artifact.tar") as nested:
                        with tarfile.open(fileobj=nested, mode="r:*") as pages:
                            members=pages.getmembers()
                            for name in NAMES:
                                member=next((m for m in members if m.name.endswith(f"flasher/firmware/versions/{version}/{name}")),None)
                                if member is None:
                                    raise ValueError(f"Missing {name} in reviewed Pages artifact")
                                source=pages.extractfile(member)
                                if source is None:
                                    raise ValueError(f"Unreadable {name} in reviewed Pages artifact")
                                (staged / name).write_bytes(source.read())
                else:
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
