#!/usr/bin/env python3
"""Compile the real two BLE link instances against an event-driven NimBLE test port."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)
 source=(root/'firmware/display-prototype/meshcrossway_ble.h').read_text()
 source='\n'.join(line for line in source.splitlines() if not line.startswith('#include ') or 'meshcrossway_ble_link.inc' in line)
 (p/'ble_under_test.h').write_text(source)
 out=p/'ble-test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-I'+str(p),'-I'+str(root/'tests'),'-I'+str(root/'firmware/display-prototype'),str(root/'tests/ble_dual_test.cpp'),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
