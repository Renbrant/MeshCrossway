#!/usr/bin/env python3
"""Guard navigation stack headroom using the real ESP-IDF compiler reports.

This is a conservative frame budget, not a complete call-graph/stack proof.
Keep 4 KiB for filesystem/newlib calls beneath navigation's own frames.
Runtime high-water marks and physical-device testing remain necessary.
"""
import argparse
from pathlib import Path
import re


def check(reports, header):
    match = re.search(r'WORKER_STACK_BYTES\s*=\s*(\d+)', header)
    if not match:
        raise ValueError('Missing navigation task stack budget')
    budget = int(match[1])
    frames = {}
    for line in reports.splitlines():
        fields = line.split('\t')
        if len(fields) != 3 or 'mcxnav::' not in fields[0]:
            continue
        name = fields[0].split(':', 3)[-1]
        if fields[2] not in ('static', 'dynamic,bounded'):
            raise ValueError(f'Unbounded navigation stack frame: {name}')
        frames[name] = max(frames.get(name, 0), int(fields[1]))
    def frame(name):
        values = [size for symbol, size in frames.items() if name in symbol]
        if not values:
            raise ValueError(f'Missing compiler stack report for {name}')
        return max(values)
    worker, command = frame('mcxnav::worker('), frame('mcxnav::execute(')
    storage = sorted((size for name, size in frames.items() if 'mcxnav::Store::' in name), reverse=True)
    if not storage:
        raise ValueError('Missing navigation storage compiler frames')
    # Cover worker + command dispatch + three large storage frames, allowing
    # functions that GCC has already inlined into their caller.
    required = worker + command + sum(storage[:3]) + 4096
    if required > budget:
        raise ValueError(f'Navigation stack headroom failed: reserve={required}, task={budget} bytes')
    return f'PASS: navigation stack reserve {required}/{budget} bytes (includes 4096 filesystem/newlib bytes)'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--header', type=Path, required=True)
    args = parser.parse_args()
    reports = list(args.build.rglob('*.su'))
    if not reports:
        parser.exit(1, 'Missing target compiler .su files\n')
    try:
        print(check('\n'.join(p.read_text() for p in reports), args.header.read_text()))
    except ValueError as error:
        parser.exit(1, f'{error}\n')
