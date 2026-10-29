#!/usr/bin/env python3
"""Compile the real independent protocol core under ASan/UBSan and run wire/session fixtures."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    binary=Path(tmp)/'meshcore-test'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',
                    '-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/meshcore_protocol_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
