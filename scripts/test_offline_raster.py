#!/usr/bin/env python3
"""Host validation of bounded PMTiles and map geometry, with ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile
import os
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='mcx-raster-') as d:
    binary=str(Path(d)/'pmtiles')
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-O1','-g','-fsanitize=address,undefined',str(ROOT/'tests/pmtiles_test.cpp'),'-lz','-o',binary],check=True)
    subprocess.run([binary],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
