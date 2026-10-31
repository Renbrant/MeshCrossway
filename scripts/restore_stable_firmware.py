#!/usr/bin/env python3
"""Restore exact owner-approved v0.8.14; never rebuild a tested image."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import urllib.request
import zipfile

VERSION = 'v0.8.14'
REF = '9a232921ab3543d6221fc6eeee56678ad374dc28'
SHA = '8f3b70fedb2c8c4c2432dbe0d00801197cfe1d08ddf6c52d02668a0835f03c31'
ARTIFACT = 13048058040
ARCHIVE_SHA = 'b1e461549d8d3c122dd15cdb68021d3f8a406ec9bf289a12316d1b3c0209109b'
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
