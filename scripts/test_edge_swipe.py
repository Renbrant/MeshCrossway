#!/usr/bin/env python3
"""Run edge-swipe gesture policy tests under C++11 and C++20."""
import pathlib
import subprocess
import tempfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
TEST=ROOT/"tests/test_edge_swipe.cpp"
with tempfile.TemporaryDirectory(prefix="mcx-edge-swipe-") as tmp:
    for standard in ("c++11","c++20"):
        exe=pathlib.Path(tmp)/("edge-"+standard)
        subprocess.run(["g++","-std="+standard,"-Wall","-Wextra","-Werror","-pedantic",
                        "-O2",str(TEST),"-o",str(exe)],check=True,cwd=ROOT)
        subprocess.run([str(exe)],check=True,cwd=ROOT)
print("PASS: edge gestures C++11/C++20")
