#!/usr/bin/env python3
"""Reuse a successful same-repository preflight only when build inputs match HEAD."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]
REPO=os.environ.get('GITHUB_REPOSITORY','Renbrant/MeshCrossway')
INPUTS=['firmware/display-prototype','scripts/build_firmware_versions.sh',
        'scripts/write_firmware_version.py','scripts/check_ble_sdkconfig.py',
        'scripts/test_ble_pairing_ui.py','scripts/test_meshcore.py','tests/meshcore_protocol_test.cpp']

def gh(path):
    return json.loads(subprocess.check_output(['gh','api',f'repos/{REPO}/{path}'],text=True))

def restore():
    version=(ROOT/'firmware/display-prototype/VERSION').read_text().strip()
    runs=gh('actions/workflows/firmware-preflight.yml/runs?status=success&per_page=20')['workflow_runs']
    for run in runs:
        if run['event']!='pull_request' or run.get('head_repository',{}).get('full_name')!=REPO:
            continue
        producer=run['head_sha']
        subprocess.run(['git','fetch','--no-tags','origin',producer],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
        if subprocess.run(['git','diff','--quiet',producer,'HEAD','--','.github/workflows/firmware-preflight.yml'],cwd=ROOT).returncode:
            continue
        artifacts=gh(f"actions/runs/{run['id']}/artifacts")['artifacts']
        for artifact in artifacts:
            if artifact['name']!='firmware-version-archive-preflight' or artifact['expired']:
                continue
            with tempfile.TemporaryDirectory() as temporary:
                temp=Path(temporary); archive=temp/'archive.zip'
                with archive.open('wb') as out:
                    subprocess.run(['gh','api',f"repos/{REPO}/actions/artifacts/{artifact['id']}/zip"],stdout=out,check=True)
                digest=artifact.get('digest')
                if not digest or digest!='sha256:'+hashlib.sha256(archive.read_bytes()).hexdigest():
                    print('Reject artifact: missing/mismatched GitHub digest');continue
                with zipfile.ZipFile(archive) as z:
                    prefix=f'versions/{version}/'
                    try:
                        info=json.loads(z.read(prefix+'build-info.json'))
                    except KeyError:
                        continue
                    sha=info['commit']
                    if len(sha)!=40 or any(c not in '0123456789abcdef' for c in sha):
                        continue
                    subprocess.run(['git','fetch','--no-tags','origin',sha],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
                    if subprocess.run(['git','diff','--quiet',sha,'HEAD','--',*INPUTS],cwd=ROOT).returncode:
                        print(f"Skip run {run['id']}: firmware build inputs differ");continue
                    folder=temp/'firmware'/prefix;folder.mkdir(parents=True)
                    names=['manifest.json','build-info.json','meshcrossway-display-dev.bin']
                    for part in info.get('flash_parts',[]):
                        if part['path'] not in ('bootloader.bin','partition-table.bin','application.bin'):
                            raise ValueError('Unexpected installer component')
                        names.append(part['path'])
                    for name in names:
                        (folder/name).write_bytes(z.read(prefix+name))
                subprocess.run(['python3',str(ROOT/'scripts/verify_firmware_version.py'),'--dir',str(folder),'--version',version,'--ref',sha],check=True)
                subprocess.run(['python3',str(ROOT/'scripts/check_firmware_identity.py'),'--root',str(temp/'firmware')],cwd=ROOT,check=True)
                target=ROOT/'_site/flasher/firmware'/prefix;target.mkdir(parents=True,exist_ok=True)
                for file in folder.iterdir():
                    (target/file.name).write_bytes(file.read_bytes())
                print(f"REUSED reviewed firmware from successful run {run['id']}; compiled source {sha}")
                return
    print('No matching reviewed artifact; normal build remains enabled')

if __name__=='__main__':
    try:
        restore()
    except (OSError,ValueError,KeyError,subprocess.CalledProcessError,zipfile.BadZipFile) as error:
        # Never publish unverified bytes. The existing build/verification follows.
        print(f'Reviewed artifact unavailable: {error}; falling back to compilation')
