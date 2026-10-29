#!/usr/bin/env python3
"""AXP2101 battery read-only register decoder regression C++11 and C++20."""
import pathlib
import subprocess
import tempfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
TEST=ROOT/"tests/test_battery_decode.cpp"
with tempfile.TemporaryDirectory(prefix="mcx-battery-") as tmp:
    for standard in ("c++11","c++20"):
        exe=pathlib.Path(tmp)/("battery-"+standard)
        subprocess.run(["g++","-std="+standard,"-Wall","-Wextra","-Werror","-pedantic",
                        "-O2",str(TEST),"-o",str(exe)],check=True,cwd=ROOT)
        subprocess.run([str(exe)],check=True,cwd=ROOT)
print("PASS: battery decoder C++11/C++20")
