#!/usr/bin/env python3
"""Audit the exact published Android 0.3.8 source; failures are intentional findings."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
REF = 'dcc0a56c81e86d956f2af7a124ecc0e6ba3fde23'
CLASSES = ['ProtocolProbe', 'RadioProfile', 'Wire', 'MeshData', 'ProtocolClient',
           'MtAdmin', 'MtLora', 'MtMetrics', 'GeoMath', 'TrafficBudget',
           'ConnectionGate', 'UiFeed', 'RxInfo', 'FieldTools']
with tempfile.TemporaryDirectory(prefix='mcx-meshcore-audit-') as scratch:
    inputs = []
    for name in CLASSES:
        path = f'android/app/src/main/java/com/meshcrossway/android/{name}.java'
        target = Path(scratch) / (name + '.java')
        target.write_bytes(subprocess.check_output(['git', 'show', f'{REF}:{path}'], cwd=ROOT))
        inputs.append(str(target))
    inputs.append(str(ROOT / 'docs/audits/fixtures/MeshCore038Probe.java'))
    subprocess.run(['javac', '-d', scratch, *inputs], check=True)
    result = subprocess.run(['java', '-cp', scratch, 'com.meshcrossway.android.MeshCore038Probe'])
    raise SystemExit(result.returncode)
