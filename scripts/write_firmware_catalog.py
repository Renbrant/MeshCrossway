#!/usr/bin/env python3
"""Publish ONLY the newest owner-tested image and newer supported builds."""
import argparse
import json
import re
import shutil
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument("--root",required=True)
p.add_argument("--latest",required=True)
p.add_argument("--stable",required=True)
args=p.parse_args()
root=Path(args.root)
latest=args.latest
stable=args.stable

def version_key(version):
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+",version):
        p.error(f"Invalid firmware version {version!r}")
    return tuple(map(int,version[1:].split(".")))

if version_key(stable)>version_key(latest):
    p.error("Approved stable cannot be newer than latest build")

# Only include releases with explicit owner approval.
approved={
    "v0.6.8":("Owner confirmed normal operation and PIN-free manual web screenshots. Exact published image retained. MeshCore application traffic was not implemented in this release.", "Hardware-tested UI / screenshot rollback"),
    "v0.6.2":("Owner confirmed touchscreen, Wi-Fi and normal UI work on the Waveshare Rev2.0; confirmed working after v0.6.3 failed.", "Last hardware-tested / recommended"),
}
# Do not expose a known-broken image through the installer.
blocked={"v0.6.3","v0.6.4"}
if stable not in approved:
    p.error(f"Approved stable version {stable} has no recorded hardware validation")
if latest in blocked:
    p.error(f"Latest version {latest} is known broken; cannot publish")
if not (root/"versions"/stable).is_dir():
    p.error(f"Missing tested rollback image {stable}")
if not (root/"versions"/latest).is_dir():
    p.error(f"Missing latest compiled image {latest}")

catalog=[]
for folder in sorted((root/"versions").iterdir(),key=lambda x:version_key(x.name)):
    if not folder.is_dir():
        continue
    version=folder.name
    if version_key(version)<version_key(stable) or version_key(version)>version_key(latest) or version in blocked:
        continue
    manifest=folder/"manifest.json"
    info_file=folder/"build-info.json"
    if not manifest.is_file() or not info_file.is_file():
        p.error(f"Missing compiled binary or metadata for {version}")
    m=json.loads(manifest.read_text())
    info=json.loads(info_file.read_text())
    if m["version"]!=version or info["version"]!=version:
        p.error(f"Manifest/version mismatch for {version}")
    # Archived owner-tested binaries retain exact bytes, but their historical
    # manifest set new_install_prompt_erase=false. ESP Web Tools interprets
    # that as an automatic full-chip erase when Improv is unavailable.
    # Fix only the published manifest, never the tested firmware or checksums.
    if m.get("new_install_prompt_erase") is not True:
        m["new_install_prompt_erase"]=True
        manifest.write_text(json.dumps(m,indent=2)+"\n")
  
    if version==stable:
        summary,label=approved[version]
        status="hardware-tested"
    elif version=="v0.7.3":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: Color emojis in MeshCore chat, 92 local LVGL 8.4 image glyphs, read-only UTF-8 display. Retains v0.7.2 automatic Wi-Fi profile fallback and last bonded BLE-radio reconnect. Never converts mesh traffic. Leave Erase unchecked."
        label="Color chat emojis + Wi-Fi/BLE reconnect / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.7.2":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: Wi-Fi reconnects to the last successful network and falls back to four other saved networks. The last bonded BLE radio is discovered after safe boot with bounded retry scans; manual disconnect suppresses retry. BLE currently operates one radio session. Leave Erase unchecked."
        label="Wi-Fi and last BLE radio auto-reconnect / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.7.1":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: MeshCore Companion with persistent Bluetooth per-radio bonds in NVS, so PIN should be required only on first successful pairing. Tested rollback v0.6.8 retained."
        label="MeshCore + saved Bluetooth pairing / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.7.0":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: MeshCore Companion BLE session, real contacts/channels, direct/group messaging, delivery ACKs and typed radio tools. Board/firmware capabilities apply. PIN-free manual screenshots retained."
        label="MeshCore Companion / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.6.8":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: manual web screenshots without a View PIN; remote touch remains disabled. Retains the Bluetooth numeric PIN keypad and DMA/BLE stability changes."
        label="Web capture without PIN / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.6.7":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: fixes Bluetooth PIN keyboard visibility with a full-screen numeric keypad; retains v0.6.6 DMA/BLE stability changes. Pairing/encryption still requires owner testing."
        label="Bluetooth PIN keyboard fix / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.6.6":
        summary="EXPERIMENTAL / NOT HARDWARE TESTED: reserves small DMA LCD buffers before BLE startup, reports and aborts on SPI color failure instead of waiting forever. LVGL core1, LCD 40MHz / queue6, Wi-Fi core0 and NimBLE core1; generated sdkconfig verified. Do not use for critical communication."
        label="DMA display + BLE diagnostic / EXPERIMENTAL"
        status="experimental"
    elif version=="v0.6.4":
        summary="KNOWN CRITICAL ISSUE: v0.6.4 boots but enabling Bluetooth freezes the touchscreen/device before radio discovery. Do not install; return to hardware-tested v0.6.2."
        label="FREEZES ON BLE ENABLE / DO NOT INSTALL"
        status="known-issue"
    else:
        summary="EXPERIMENTAL, NOT HARDWARE TESTED: development firmware. May freeze or require rollback to tested firmware."
        label="Newest candidate / EXPERIMENTAL"
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
        "binary":f"/MeshCrossway/flasher/firmware/versions/{version}/meshcrossway-display-dev.bin",
    })

if not any(v["version"]==stable and v["status"]=="hardware-tested" for v in catalog):
    p.error("Approved stable image missing from published catalog")
if not any(v["version"]==latest for v in catalog):
    p.error("Latest image missing from published catalog")

catalog_info={
    "target":"Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 non-B",
    "recommended":stable,
    "latest":latest,
    "versions":catalog,
}
(root/"catalog.json").write_text(json.dumps(catalog_info,indent=2)+"\n")

# Keep legacy direct URLs pointing at the newest binary only (no old archives).
current=root/"versions"/latest
parts=tuple(part["path"] for part in json.loads((current/"manifest.json").read_text())["builds"][0]["parts"] if part["path"]!="meshcrossway-display-dev.bin")
for filename in ("manifest.json","build-info.json","meshcrossway-display-dev.bin")+parts:
    shutil.copy2(current/filename,root/filename)
print("Flasher versions:",", ".join(v["version"] for v in catalog),"; default:",stable)
