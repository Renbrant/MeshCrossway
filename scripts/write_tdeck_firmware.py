#!/usr/bin/env python3
"""Package a compiled T-Deck Plus build for the Web Flasher.

Copies the three ESP-IDF images (bootloader, partition table, application),
refuses a layout that would overwrite NVS (saved BLE bonds), and writes the
version manifest, build-info and the board catalog. Every T-Deck build stays
EXPERIMENTAL until a hardware test is recorded in TESTED below.
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

BOARD = "tdeck-plus"
TARGET = "LilyGO T-Deck Plus (ESP32-S3FN16R8)"
# version -> (label, description) once a physical test is recorded.
TESTED = {}
SUMMARY = {
    "v0.1.0": (
        "First T-Deck Plus build / EXPERIMENTAL",
        "EXPERIMENTAL / NOT HARDWARE TESTED: first T-Deck Plus controller. Two independent BLE radios "
        "(Meshtastic PhoneAPI + MeshCore Companion), keyboard and trackball navigation, chats, channels, "
        "contacts and microSD history. No map or Wi-Fi yet; the onboard LoRa radio stays off.",
    ),
}

parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path, help="ESP-IDF build directory")
parser.add_argument("--version", required=True)
parser.add_argument("--ref", required=True)
parser.add_argument("--output", required=True, type=Path, help="flasher/firmware/tdeck-plus directory")
args = parser.parse_args()
if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", args.version):
    parser.error("Version must be vMAJOR.MINOR.PATCH")
if not re.fullmatch(r"[a-f0-9]{40}", args.ref):
    parser.error("Ref must be the full immutable Git commit hash")
if args.version not in SUMMARY and args.version not in TESTED:
    parser.error(f"Add a release description for {args.version} in write_tdeck_firmware.py")

build = args.build
sources = {
    "bootloader.bin": build / "bootloader" / "bootloader.bin",
    "partition-table.bin": build / "partition_table" / "partition-table.bin",
    "application.bin": build / "meshcrossway_tdeck_plus.bin",
}
offsets = {"bootloader.bin": 0x0, "partition-table.bin": 0x8000, "application.bin": 0x10000}
for name, path in sources.items():
    if not path.is_file() or path.stat().st_size < 1000:
        parser.error(f"Missing or suspiciously small {name}: {path}")
    if name != "partition-table.bin" and path.read_bytes()[0] != 0xE9:
        parser.error(f"{name} is not an ESP image")

table = sources["partition-table.bin"].read_bytes()
partitions = []
for offset in range(0, min(len(table), 0xC00), 32):
    if table[offset:offset + 2] == b"\xaaP":
        _, kind, sub, start, size, label, _ = struct.unpack_from("<HBBII16sI", table, offset)
        partitions.append((kind, sub, start, size, label.rstrip(b"\0")))
if (1, 2, 0x9000, 0x6000, b"nvs") not in partitions:
    parser.error("Unexpected NVS partition: refusing an image that could erase BLE bonds")
app = next((p for p in partitions if p[0] == 0 and p[2] == 0x10000), None)
if not app or sources["application.bin"].stat().st_size > app[3]:
    parser.error("Application does not fit the factory partition at 0x10000")

folder = args.output / "versions" / args.version
folder.mkdir(parents=True, exist_ok=True)
parts, flash_parts, total = [], [], hashlib.sha256()
for name, path in sources.items():
    data = path.read_bytes()
    shutil.copyfile(path, folder / name)
    parts.append({"path": name, "offset": offsets[name]})
    flash_parts.append({"path": name, "offset": offsets[name], "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()})
    total.update(data)
application = (folder / "application.bin").read_bytes()
manifest = {
    "name": f"MeshCrossway T-Deck Plus {args.version}",
    "version": args.version,
    # Always offer the erase decision. False silently erases when Improv is absent.
    "new_install_prompt_erase": True,
    "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
}
info = {
    "version": args.version,
    "board": BOARD,
    "commit": args.ref,
    "target": TARGET,
    "size": len(application),
    "sha256": hashlib.sha256(application).hexdigest(),
    "flash_parts": flash_parts,
}
(folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
(folder / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")

versions = []
for path in sorted((args.output / "versions").iterdir(), key=lambda p: tuple(map(int, p.name[1:].split(".")))):
    meta = json.loads((path / "build-info.json").read_text())
    version = meta["version"]
    tested = version in TESTED
    label, description = TESTED.get(version) or SUMMARY[version]
    versions.append({
        "version": version,
        "label": label,
        "status": "hardware-tested" if tested else "experimental",
        "description": description,
        "commit": meta["commit"][:8],
        "sha256": meta["sha256"],
        "size": meta["size"],
        "manifest": f"/MeshCrossway/flasher/firmware/{BOARD}/versions/{version}/manifest.json",
        "binary": f"/MeshCrossway/flasher/firmware/{BOARD}/versions/{version}/application.bin",
    })
tested_versions = [v["version"] for v in versions if v["status"] == "hardware-tested"]
catalog = {
    "board": BOARD,
    "target": TARGET,
    "latest": versions[-1]["version"],
    # No default until a physical test is recorded: the flasher never preselects experimental builds.
    "recommended": tested_versions[-1] if tested_versions else None,
    "versions": versions,
}
(args.output / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
print(f"T-Deck Plus {args.version} packaged: {info['sha256'][:12]} ({len(application)} bytes)")
