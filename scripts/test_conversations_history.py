#!/usr/bin/env python3
"""Run the production conversation + journal code with ASan/UBSan on the host."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    binary=Path(temp)/'history-test'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(ROOT/'firmware/display-prototype'),
                    str(ROOT/'tests/conversations_history_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(Path(temp)/'card')],check=True)
