#!/usr/bin/env python3
"""Write a version-specific ESP Web Tools manifest after compilation succeeds."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("--version",required=True)
parser.add_argument("--ref",required=True)
parser.add_argument("--folder",required=True)
args=parser.parse_args()
if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+",args.version):
    parser.error("Version must be vMAJOR.MINOR.PATCH")
if not re.fullmatch(r"[a-f0-9]{40}",args.ref):
    parser.error("Ref must be the full immutable Git commit hash")
folder=Path(args.folder)
binary=folder/"meshcrossway-display-dev.bin"
if not binary.is_file() or binary.stat().st_size<10000:
    parser.error(f"Firmware binary missing or suspiciously small: {binary}")
data=binary.read_bytes()
sha256=hashlib.sha256(data).hexdigest()
split=tuple(map(int,args.version[1:].split('.'))) >= (0,5,8)
parts=[{"path":binary.name,"offset":0}]
flash_parts=[]
if split:
    if len(data)<=0x10000 or data[0]!=0xe9 or data[0x10000]!=0xe9:
        parser.error("Invalid merged ESP32-S3 image layout")
    partitions=[]
    for offset in range(0x8000,0x9000,32):
        if data[offset:offset+2]==b'\xaaP':
            _,kind,sub,start,size,label,_=struct.unpack_from('<HBBII16sI',data,offset)
            partitions.append((kind,sub,start,size,label.rstrip(b'\0')))
    if (1,2,0x9000,0x6000,b'nvs') not in partitions:
        parser.error("Unexpected NVS partition: refusing unsafe split image")
    parts=[]
    for name,start,end in [('bootloader.bin',0,0x8000),('partition-table.bin',0x8000,0x9000),('application.bin',0x10000,len(data))]:
        content=data[start:end]
        (folder/name).write_bytes(content)
        parts.append({"path":name,"offset":start})
        flash_parts.append({"path":name,"offset":start,"size":len(content),"sha256":hashlib.sha256(content).hexdigest()})
manifest={
    "name":f"MeshCrossway Display {args.version}",
    "version":args.version,
    # Always offer the erase decision. False silently erases when Improv is absent.
    "new_install_prompt_erase":True,
    "builds":[
        {"chipFamily":"ESP32-S3","parts":parts}
    ]
}
info={
    "version":args.version,
    "commit":args.ref,
    "target":"Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 (non-B)",
    "size":binary.stat().st_size,
    "sha256":sha256
}
if split:info["flash_parts"]=flash_parts
(folder/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
(folder/"build-info.json").write_text(json.dumps(info,indent=2)+"\n")
print(f"Manifest ready: {args.version} / {sha256[:12]}")
