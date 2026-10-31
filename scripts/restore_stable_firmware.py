#!/usr/bin/env python3
"""Restore exact owner-approved v0.8.11; never rebuild a tested image."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import urllib.request
import zipfile

VERSION = 'v0.8.11'
REF = '411c8a3c7b9da6d5703f2f8c1cffa7dde9584c5c'
SHA = 'dbbdf7ed69b04f134517566053cca9eccca80c0d6be5ffc0f4ef53bc636c81b8'
ARTIFACT = 13014125812
ARCHIVE_SHA = 'd5eee7cdf4c614494edab3ebfd6c9025c61166840bd7693ff44d3c28e9f87b87'
NAMES = ('manifest.json', 'build-info.json', 'meshcrossway-display-dev.bin',
         'bootloader.bin', 'partition-table.bin', 'application.bin')
ROOT = Path(__file__).resolve().parents[1]

def verify(folder):
    subprocess.run(['python3', str(ROOT/'scripts/verify_firmware_version.py'),
                    '--dir', str(folder), '--version', VERSION, '--ref', REF], check=True)
    if hashlib.sha256((folder/'meshcrossway-display-dev.bin').read_bytes()).hexdigest() != SHA:
        raise ValueError('Owner-approved binary checksum mismatch')

def restore(destination):
    with tempfile.TemporaryDirectory(prefix='mcx-stable-') as tmp:
        folder = Path(tmp)
        try:
            for name in NAMES:
                url = f'https://renbrant.github.io/MeshCrossway/flasher/firmware/versions/{VERSION}/{name}'
                with urllib.request.urlopen(url, timeout=25) as response:
                    data = response.read(8*1024*1024+1)
                if len(data) > 8*1024*1024:
                    raise ValueError('Artifact too large')
                (folder/name).write_bytes(data)
            verify(folder)
        except (OSError, ValueError, subprocess.CalledProcessError):
            # First promotion may follow a release that removed the older image.
            # Recover the retained successful PR archive, pinned by GitHub digest.
            archive = folder/'archive.zip'
            with archive.open('wb') as out:
                subprocess.run(['gh', 'api', f'repos/Renbrant/MeshCrossway/actions/artifacts/{ARTIFACT}/zip'],
                               stdout=out, check=True)
            if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA:
                raise ValueError('Reviewed archive checksum mismatch')
            with zipfile.ZipFile(archive) as z:
                for name in NAMES:
                    (folder/name).write_bytes(z.read(f'versions/{VERSION}/{name}'))
            verify(folder)
        destination.mkdir(parents=True, exist_ok=True)
        for name in NAMES:
            (destination/name).write_bytes((folder/name).read_bytes())

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', type=Path, required=True)
    restore(parser.parse_args().destination)
