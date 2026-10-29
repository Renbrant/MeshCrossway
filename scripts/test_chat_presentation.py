#!/usr/bin/env python3
"""Compile the new MeshCore chat presentation rules in C++11 and C++20."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/"tests/test_chat_presentation.cpp"
with tempfile.TemporaryDirectory(prefix="mcx-chat-") as work:
    for standard in ("c++11","c++20"):
        binary=Path(work)/("chat-"+standard)
        subprocess.run(["g++","-std="+standard,"-Wall","-Wextra","-Werror",
                        "-pedantic","-O2",str(SRC),"-o",str(binary)],
                       check=True,cwd=ROOT)
        subprocess.run([str(binary)],check=True,cwd=ROOT)
print("PASS: chat metadata helpers C++11/C++20")
