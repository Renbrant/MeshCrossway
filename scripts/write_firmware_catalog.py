#!/usr/bin/env python3
"""Publish a catalog of verified historical ESP32-S3 images with a safe default."""
import argparse
import json
import re
import shutil
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument("--root",required=True)
p.add_argument("--latest",required=True)
args=p.parse_args()
root=Path(args.root)
latest=args.latest
if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+",latest):
    p.error("Invalid latest version")
tested={
    "v0.6.2":("Owner tested v0.6.2 on Waveshare Rev2.0: touch, Wi-Fi and normal UI operate. v0.6.3 boot freezes; v0.6.2 is confirmed working rollback.", "Last working tested UI / recommended"),
    "v0.5.6":("Owner confirmed usable wider QWERTY and normal Wi-Fi operation; remote screen/touch disabled", "Tested keyboard + Wi-Fi / rollback"),
    "v0.5.7":("Owner confirmed stable physical UI and status HTTP with live uptime/IP/heap; screenshot and remote touch disabled", "Tested HTTP status + Wi-Fi / rollback"),
    "v0.5.9":("Owner verified 32GB FAT32 JSON Wi-Fi backup, successful reinstall without Erase and automatic Wi-Fi reconnect; remote capture disabled", "Tested SD backup + Wi-Fi / rollback"),
    "v0.5.10":("Owner confirmed multiple manual read-only screenshots, responsive pages and stable Wi-Fi/SD on Waveshare Rev2.0", "Tested read-only remote viewer / rollback"),
    "v0.5.4":("Owner confirmed Wi-Fi connection and normal operation; landscape UI", "Tested Wi-Fi / rollback"),
    "v0.3.0":("Tested on Rev2.0: LVGL, touchscreen and live ESP32 diagnostics","Display / touch baseline"),
    "v0.4.0":("Tested on Rev2.0: Wi-Fi access point, screenshot viewing and PNG capture","Recommended recovery"),
}
catalog=[]
for version in ("v0.4.0","v0.3.0","v0.5.2","v0.5.4","v0.5.6","v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.2","v0.6.3",latest):
    if any(entry["version"]==version for entry in catalog):
        continue
    folder=root/"versions"/version
    manifest=folder/"manifest.json"
    info_file=folder/"build-info.json"
    if not manifest.is_file() or not info_file.is_file():
        p.error(f"Missing built firmware or metadata for {version}")
    m=json.loads(manifest.read_text())
    info=json.loads(info_file.read_text())
    if m["version"]!=version or info["version"]!=version:
        p.error(f"Manifest/version mismatch for {version}")
    if version in tested:
        summary,label=tested[version]
        status="hardware-tested"
    elif version=="v0.6.4":
        summary="EXPERIMENTAL, NOT TESTED: post-v0.6.3 recovery candidate. BLE remains OFF at startup to preserve Wi-Fi/touch; enable BLE manually in More > Radios. Wi-Fi / touch must be verified before enabling."
        label="Safe boot + manual BLE enable / UNTESTED"
        status="experimental"
    elif version=="v0.6.3":
        summary="KNOWN CRITICAL ISSUE: owner tested v0.6.3 twice and both boots froze: touch not responding and Wi-Fi stuck at waiting. Reinstall v0.6.2. Do NOT install v0.6.3."
        label="BROKEN BOOT / DO NOT INSTALL"
        status="known-issue"
    elif version=="v0.6.2":
        summary="Six-tab navigation (Home, Nodes, Groups, Chats, Map, More), per-network filters and persistent selected demo chat; compact radio/group icons, hidden Home rail; radio and Wi-Fi preserved. Physical test pending."
        label="Nodes + groups + chats / experimental"
        status="experimental"
    elif version=="v0.6.1":
        summary="Field Console responsive layout update: compact icons centered, scrolling content, improved status cards and contextual actions; Wi-Fi/SD/read-only remote untouched. Physical test pending."
        label="Adaptive console / experimental"
        status="experimental"
    elif version=="v0.6.0":
        summary="Field Console LVGL redesign from approved virtual UI: dual collapsible side rails, Map subviews, contextual actions and quick settings. Wi-Fi/SD/read-only remote preserved; physical test pending."
        label="Side-console navigation / experimental"
        status="experimental"
    elif version=="v0.5.8":
        summary="Plaintext Wi-Fi backup/restore on optional SD; split installer preserves internal settings without Erase. HTTP status only. Hardware test pending."
        label="SD Wi-Fi backup / experimental"
        status="experimental"
    elif version=="v0.5.6":
        summary="Wider landscape QWERTY and one character per completed tap; automatic key repeat disabled. v0.5.5 froze during typing in one owner test; root cause unconfirmed. Hardware test pending."
        label="Wider keys / experimental"
        status="experimental"
    elif version=="v0.5.5":
        summary="Larger full-screen landscape QWERTY editor for Wi-Fi credentials. Hardware test pending; remote display/touch disabled."
        label="Full-screen keyboard / experimental"
        status="experimental"
    elif version in ("v0.5.3","v0.5.4"):
        summary="Touchscreen Wi-Fi setup experiment: asynchronous SSID scan and on-screen password entry. Remote display/touch remains disabled; hardware not tested."
        label="Wi-Fi setup / experimental"
        status="experimental"
    elif version=="v0.5.2":
        summary="Diagnostic build: Wi-Fi AP+STA enabled; remote screen capture and touch disabled. Display and menu navigation passed the owner test on October 28, 2026; Wi-Fi provisioning remains unverified."
        label="Display-tested rollback / Wi-Fi unverified"
        status="experimental"
    elif version in ("v0.5.0","v0.5.1"):

        summary="Known issue: user reported flashing/flickering screen on Rev2.0. Not recommended."
        label="Unstable / do not install"
        status="known-issue"
    else:
        summary="New development firmware: compiled but not yet tested on physical hardware"
        label="Latest experimental"
        status="experimental"
    catalog.append({
        "version":version,
        "label":label,
        "status":status,
        "description":summary,
        "commit":info["commit"][:8],
        "sha256":info["sha256"],
        "size":info["size"],
        "manifest":f"/MeshCrossway/flasher/firmware/versions/{version}/manifest.json",
        "binary":f"/MeshCrossway/flasher/firmware/versions/{version}/meshcrossway-display-dev.bin"
    })
# The Web Flasher default is the newest hardware-tested release.
tested_versions=[entry["version"] for entry in catalog if entry["status"]=="hardware-tested"]
if not tested_versions:
    p.error("No hardware-tested firmware in catalog")
recommended=max(tested_versions,key=lambda v:tuple(map(int,v[1:].split("."))))
catalog_info={
    "target":"Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B",
    "recommended":recommended,
    "latest":latest,
    "versions":catalog
}
(root/"catalog.json").write_text(json.dumps(catalog_info,indent=2)+"\n")

# Backwards compatible paths for previously bookmarked ESP Web Tools links.
current=root/"versions"/latest
for filename in ("manifest.json","build-info.json","meshcrossway-display-dev.bin")+tuple(part["path"] for part in json.loads((current/"manifest.json").read_text())["builds"][0]["parts"] if part["path"]!="meshcrossway-display-dev.bin"):
    shutil.copy2(current/filename,root/filename)
print("Firmware catalog:", ", ".join(v["version"] for v in catalog))
