#!/usr/bin/env python3
"""Archive exact tested images as permanent releases; never replace an existing asset."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path
from restore_stable_firmware import APPROVED, NAMES, VERSION, restore, verify

REPO = "Renbrant/MeshCrossway"

def archive(version):
    ref, sha, _, _ = APPROVED[version]
    with tempfile.TemporaryDirectory(prefix="mcx-release-") as tmp:
        folder = Path(tmp)
        exists = subprocess.run(["gh", "release", "view", version, "--repo", REPO], capture_output=True)
        if exists.returncode == 0:
            subprocess.run(["gh", "release", "download", version, "--repo", REPO, "--dir", str(folder),
                            *[arg for name in NAMES for arg in ("--pattern", name)]], check=True)
            verify(folder, version)
            print(f"VERIFIED: permanent {version} release already exists; assets unchanged")
            return
        restore(folder, version)
        # Preserve credentials/bonds even when an older archived manifest predates this guard.
        manifest = folder / "manifest.json"
        data = json.loads(manifest.read_text())
        if data.get("new_install_prompt_erase") is not True:
            data["new_install_prompt_erase"] = True
            manifest.write_text(json.dumps(data, indent=2) + "\n")
        verify(folder, version)
        notes = (f"Hardware-tested image retained byte-for-byte. "
                 f"{'Recommended stable.' if version == VERSION else 'Previous stable recovery.'}\n\n"
                 f"Source commit: `{ref}`\n\nCombined-image SHA-256: `{sha}`\n\n"
                 "Installer preserves NVS settings by default. Hardware acceptance record: "
                 "https://github.com/Renbrant/MeshCrossway/blob/main/docs/stable-" + version.replace(".", "") + ".md")
        subprocess.run(["gh", "release", "create", version, "--repo", REPO, "--target", ref,
                        "--title", f"{version} — {'Stable' if version == VERSION else 'Previous stable recovery'}",
                        "--notes", notes, f"--latest={'true' if version == VERSION else 'false'}",
                        *[str(folder / name) for name in NAMES]], check=True)
        print(f"ARCHIVED: exact tested {version} firmware as permanent release assets")

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--version", choices=APPROVED, required=True)
    archive(p.parse_args().version)
