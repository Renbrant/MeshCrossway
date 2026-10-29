#!/usr/bin/env python3
"""Reuse previously published, cryptographically verified firmware on docs-only Pages pushes.

If the existing web archive is unavailable or does not match the *current*
firmware build inputs, leave the normal build path enabled. No stale or
unverified firmware can reach Pages.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_BASE = "https://renbrant.github.io/MeshCrossway/flasher/firmware/versions"
BUILDER = ROOT / "scripts" / "build_firmware_versions.sh"
INPUTS = [
    "firmware/display-prototype",
    "scripts/build_firmware_versions.sh",
    "scripts/write_firmware_version.py",
    "scripts/check_ble_sdkconfig.py",
    "scripts/test_ble_pairing_ui.py",
    "scripts/test_meshcore.py",
    "tests/meshcore_protocol_test.cpp",
]
NAMES = (
    "manifest.json", "build-info.json", "meshcrossway-display-dev.bin",
    "bootloader.bin", "partition-table.bin", "application.bin",
)


def builder_constant(name: str) -> str:
    match = re.search(r'^' + name + r'="([^"]+)"$', BUILDER.read_text(), re.M)
    if not match:
        raise ValueError(f"Missing pinned stable build constant {name}")
    return match.group(1)


def download(version: str, name: str, directory: Path) -> None:
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Unexpected version path")
    if name not in NAMES:
        raise ValueError("Unexpected artifact path")
    req = urllib.request.Request(
        f"{PUBLIC_BASE}/{version}/{name}", headers={"User-Agent": "MeshCrossway-CI/1"}
    )
    with urllib.request.urlopen(req, timeout=25) as response:
        # All paths are fixed by our code. Never follow redirects to arbitrary origins.
        if not response.url.startswith("https://renbrant.github.io/MeshCrossway/"):
            raise ValueError("Unexpected artifact host")
        data = response.read(8 * 1024 * 1024 + 1)
    if not data or len(data) > 8 * 1024 * 1024:
        raise ValueError("Published artifact empty or over size limit")
    (directory / name).write_bytes(data)


def same_firmware_source(published_commit: str) -> bool:
    if not re.fullmatch(r"[a-f0-9]{40}", published_commit):
        return False
    try:
        return subprocess.run(
            ["git", "diff", "--quiet", published_commit, "HEAD", "--", *INPUTS],
            cwd=ROOT, check=False,
        ).returncode == 0 and subprocess.run(
            ["git", "cat-file", "-e", published_commit + "^{commit}"],
            cwd=ROOT, check=False,
        ).returncode == 0
    except OSError:
        return False


def verify(directory: Path, version: str, expected_ref: str | None = None) -> None:
    subprocess.run([
        "python3", str(ROOT / "scripts/verify_firmware_version.py"),
        "--dir", str(directory), "--version", version,
        *([] if expected_ref is None else ["--ref", expected_ref]),
    ], cwd=ROOT, check=True)


def restore() -> None:
    latest = (ROOT / "firmware/display-prototype/VERSION").read_text().strip()
    stable = builder_constant("STABLE_VERSION")
    ref = builder_constant("STABLE_REF")
    expected_sha = builder_constant("STABLE_SHA256")
    versions = list(dict.fromkeys([stable, latest]))
    with tempfile.TemporaryDirectory(prefix="mcx-published-") as tmp:
        staged = Path(tmp)
        for version in versions:
            dest = staged / version
            dest.mkdir()
            for name in NAMES:
                download(version, name, dest)
            info = json.loads((dest / "build-info.json").read_text())
            recorded_commit = info.get("commit", "")
            if version == stable:
                verify(dest, version, ref)
                actual_sha = hashlib.sha256((dest / "meshcrossway-display-dev.bin").read_bytes()).hexdigest()
                if actual_sha != expected_sha:
                    raise ValueError("Approved stable SHA256 mismatch")
            else:
                if not same_firmware_source(recorded_commit):
                    raise ValueError("Published candidate does not match checked-out firmware sources")
                verify(dest, version, recorded_commit)
        # Commit only after ALL versions pass; build script re-verifies on reuse.
        for version in versions:
            target = ROOT / "_site/flasher/firmware/versions" / version
            target.mkdir(parents=True, exist_ok=True)
            for source in (staged / version).iterdir():
                shutil.copy2(source, target / source.name)
        print(f"RESTORED: {', '.join(versions)} from previously deployed Pages; verified against current sources")


if __name__ == "__main__":
    try:
        restore()
    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            urllib.error.URLError, subprocess.CalledProcessError) as exc:
        # Fail safe: normal builder checks cache, immutable stable and source revision.
        print(f"::notice::No reusable published firmware: {exc}; normal build is enabled")
