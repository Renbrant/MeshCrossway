#!/usr/bin/env python3
"""Restore the exact pinned stable v0.8.23 image; never rebuild a tested image."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

VERSION = 'v0.8.23'
REF = 'cd17768eed1cd07830295c6130c245f22e8991c0'
SHA = '26a1ab1a0d2002837d021bee89c0ece70d8e5a13c020bf02cbb60899de38c414'
ARTIFACT = 13160597329
ARCHIVE_SHA = 'a9db284513017c0ea125272176e437ddf40c48e03d94562cb17d6e919bad81dd'
NAMES = ('manifest.json', 'build-info.json', 'meshcrossway-display-dev.bin',
         'bootloader.bin', 'partition-table.bin', 'application.bin')
ROOT = Path(__file__).resolve().parents[1]

def verify(folder):
    subprocess.run(['python3', str(ROOT/'scripts/verify_firmware_version.py'),
                    '--dir', str(folder), '--version', VERSION, '--ref', REF], check=True)
    if hashlib.sha256((folder/'meshcrossway-display-dev.bin').read_bytes()).hexdigest() != SHA:
        raise ValueError('Stable binary checksum mismatch')

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
            # Recover the exact published stable files from the successful
            # Pages deployment artifact, pinned by the GitHub SHA-256 digest.
            archive = folder/'archive.zip'
            with archive.open('wb') as out:
                subprocess.run(['gh', 'api', f'repos/Renbrant/MeshCrossway/actions/artifacts/{ARTIFACT}/zip'],
                               stdout=out, check=True)
            if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA:
                raise ValueError('Reviewed archive checksum mismatch')
            with zipfile.ZipFile(archive) as z:
                with tarfile.open(fileobj=io.BytesIO(z.read('artifact.tar')), mode='r:') as pages:
                    for name in NAMES:
                        member=pages.extractfile(f'./flasher/firmware/versions/{VERSION}/{name}')
                        if member is None:
                            raise ValueError(f'Stable release artifact is missing {name}')
                        (folder/name).write_bytes(member.read())
            verify(folder)
        destination.mkdir(parents=True, exist_ok=True)
        for name in NAMES:
            (destination/name).write_bytes((folder/name).read_bytes())

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', type=Path, required=True)
    restore(parser.parse_args().destination)
