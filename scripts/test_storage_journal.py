#!/usr/bin/env python3
"""Exercise encrypted SD journal on Linux; NEVER program eFuse or use device SD."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
header=root/"firmware/display-prototype/meshcrossway_storage_journal.h"
source=header.read_text()
for marker in ("esp_efuse_write_", "esp_efuse_burn_", "esp_hmac_calculate(",
               "esp_vfs_fat_sdspi_mount(", "fopen(\"/sdcard"):
    assert marker not in source, f"Forbidden stage-C side effect: {marker}"
assert "O_CREAT | O_EXCL" in source
assert "fsync(w.fd)" in source
assert "TAIL_TRUNCATED" in source
assert "mcxvault::open" in source and "mcxvault::seal" in source
with tempfile.TemporaryDirectory(prefix="mcx_journal_") as tmp:
    binary=Path(tmp)/"journal"
    cmd=["g++","-std=c++11","-O1","-g","-Wall","-Wextra",
         "-Werror","-pedantic","-fsanitize=address,undefined",
         "-fno-omit-frame-pointer",
         str(root/"tests/storage_journal_test.cpp"),"-o",str(binary),"-lcrypto"]
    subprocess.run(cmd,check=True)
    subprocess.run([str(binary)],check=True)
print("PASS: encrypted journal host gate; no runtime eFuse or SD coupling")
