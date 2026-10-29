#!/usr/bin/env python3
"""Stage-B host crypto gate. NEVER touches real eFuse, SD or BLE."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = root / "firmware/display-prototype/meshcrossway_storage_crypto.h"
source = header.read_text()
for forbidden in ("esp_efuse_write_", "esp_efuse_burn_", "esp_hmac_calculate(",
                  "mbedtls_ctr_drbg_seed("):
    assert forbidden not in source, "Unsafe API in crypto library: " + forbidden
assert "MCX_VAULT_HOST_TEST" in source
assert "MAX_RECORD_BYTES = 512;" in source
assert "Never use" in source and "HMAC" in source
with tempfile.TemporaryDirectory(prefix="mcx_vault_") as td:
    binary = Path(td) / "crypto_host_test"
    cmd = ["g++", "-std=c++11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
           "-pedantic", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
           str(root / "tests/storage_crypto_test.cpp"), "-o", str(binary), "-lcrypto"]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: v1 storage crypto host gate (no eFuse provisioning)")
