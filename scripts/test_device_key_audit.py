#!/usr/bin/env python3
"""Host gate for the read-only device eFuse inventory; never programs eFuse."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = root / "firmware/display-prototype/meshcrossway_device_key.h"
source = header.read_text()
# Fail CI if any potentially irreversible eFuse API is introduced here.
banned = (
    "esp_efuse_write_", "esp_efuse_set_", "esp_efuse_burn_",
    "ets_efuse_write_", "esp_efuse_batch_write_", "esp_efuse_read_field_blob(",
    "esp_hmac_calculate(", "esp_efuse_write_key(", "HMAC_KEY0",
)
for marker in banned:
    assert marker not in source, f"Unexpected key/secret/write API in audit code: {marker}"
assert "ready_for_storage; // Always false" in source
assert "esp_efuse_get_key_purpose(block)" in source
assert "esp_efuse_key_block_unused(block)" in source

with tempfile.TemporaryDirectory(prefix="mcx_efuse_") as tmp:
    binary = Path(tmp) / "key_audit"
    cmd = ["c++", "-std=c++11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
           "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
           str(root/"tests/device_key_audit_test.cpp"), "-o", str(binary)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)

print("PASS: no irreversible eFuse APIs or key material exposure in diagnostic module")
