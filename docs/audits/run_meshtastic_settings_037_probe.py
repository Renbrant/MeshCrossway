#!/usr/bin/env python3
"""Run audit-only probes against exact release source; nonzero means nonconformance."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
REF = '380e14acca507656660da3657659b8e18698abbc'
CLASSES = ['ProtocolProbe', 'RadioProfile', 'Wire', 'MeshData', 'ProtocolClient',
           'MtAdmin', 'MtLora', 'MtMetrics', 'GeoMath', 'TrafficBudget',
           'ConnectionGate', 'UiFeed', 'RxInfo', 'FieldTools']

with tempfile.TemporaryDirectory(prefix='mcx-settings-audit-') as scratch:
    directory = Path(scratch)
    inputs = []
    for name in CLASSES + ['ProtocolClientTest']:
        kind = 'test' if name == 'ProtocolClientTest' else 'main'
        path = f'android/app/src/{kind}/java/com/meshcrossway/android/{name}.java'
        target = directory / (name + '.java')
        target.write_bytes(subprocess.check_output(['git', 'show', f'{REF}:{path}'], cwd=ROOT))
        inputs.append(str(target))
    inputs.append(str(ROOT / 'docs/audits/fixtures/MeshtasticSettings037Probe.java'))
    subprocess.run(['javac', '-d', scratch, *inputs], check=True)
    result = subprocess.run(['java', '-cp', scratch, 'com.meshcrossway.android.MeshtasticSettings037Probe'])
    raise SystemExit(result.returncode)
