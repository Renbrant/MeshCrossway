#!/usr/bin/env python3
"""Restore the exact pinned stable v0.8.17 image; never rebuild a tested image."""
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

VERSION = 'v0.8.17'
REF = '6619b9ffaf51972f6f927431732caa7faab2f822'
SHA = '0f13bc213ee07729a917ba73426cc3fb85813eef69575ea78fe03f5af677b18b'
ARTIFACT = 13108534134
ARCHIVE_SHA = 'd912d217e3643d0f2d4c184215eb274e6390a79897db386c512996a0ebcb0157'
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
