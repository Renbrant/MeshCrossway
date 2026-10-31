#!/usr/bin/env python3
"""Conservative BLE resume frame budget, including 4 KiB for NimBLE/newlib.

Compiler reports cover our frames, not every SDK/ROM function. Runtime minimum
free-stack diagnostics and repeated physical reconnection tests remain required.
"""
import argparse
from pathlib import Path
import re

NAMES = ('auto_resume_task', 'reconnect_remembered', 'connect', 'connect_peer',
         'connect_selected', 'scan', 'request_start', 'read_remembered',
         'read_enabled', 'phase', 'copy_peer', 'resume_stack_diagnostic')

def check(reports, header):
    match = re.search(r'RESUME_STACK_BYTES\s*=\s*(\d+)', header)
    if not match:
        raise ValueError('Missing BLE resume stack budget')
    budget = int(match[1])
    frames = {}
    for line in reports.splitlines():
        fields = line.split('\t')
        if len(fields) != 3:
            continue
        for name in NAMES:
            if f'meshble::{name}(' not in fields[0]:
                continue
            if fields[2] not in ('static', 'dynamic,bounded'):
                raise ValueError(f'Unbounded BLE resume frame: {name}')
            frames[name] = max(frames.get(name, 0), int(fields[1]))
    if 'auto_resume_task' not in frames:
        raise ValueError('Missing BLE resume target compiler report')
    # Sum even mutually exclusive control paths conservatively. GCC-inlined
    # callees are already counted inside their emitted caller's frame.
    required = sum(frames.values()) + 4096
    if required > budget:
        raise ValueError(f'BLE resume stack headroom failed: reserve={required}, task={budget} bytes; {frames}')
    if any(size > 256 for size in frames.values()):
        raise ValueError(f'BLE resume control frame exceeds 256-byte limit: {frames}')
    return f'PASS: BLE resume stack reserve {required}/{budget} bytes, SDK/newlib reserve=4096; {frames}'

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--header', type=Path, required=True)
    args = parser.parse_args()
    reports = list(args.build.rglob('*.su'))
    try:
        print(check('\n'.join(p.read_text() for p in reports), args.header.read_text()))
    except ValueError as error:
        parser.exit(1, str(error)+'\n')
