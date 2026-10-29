#!/usr/bin/env python3
"""Classify a Git change set for CI: skip ESP-IDF on documentation-only edits.

Used by both PR and Pages workflows; fail closed on ambiguous SHAs.
Never classify firmware sources or published installer binaries as docs-only.
"""
from __future__ import annotations

import argparse
import subprocess
import re


def is_docs_only(paths: list[str]) -> bool:
    if not paths:
        return False
    for p in paths:
        if not isinstance(p, str) or not p or "\n" in p:
            return False
        if p == "README.md":
            continue
        if p.startswith("docs/") and not p.startswith("docs/site/flasher/firmware/"):
            continue
        # Markdown outside docs can be notes; treat as safe to skip firmware
        # only when it is a Markdown document, never a build script or YAML.
        if p.endswith((".md", ".mdx")):
            continue
        return False
    return True


def changed_paths(base: str, head: str) -> list[str]:
    if not all(re.fullmatch(r"[a-fA-F0-9]{40}", value or "") for value in (base, head)):
        raise ValueError("missing or invalid SHA")
    if base == "0" * 40 or head == "0" * 40:
        raise ValueError("zero SHA (new branch or unavailable predecessor)")
    raw = subprocess.check_output(
        ["git", "diff", "--name-only", "-z", "--no-ext-diff", base, head, "--"]
    )
    return [part.decode("utf-8") for part in raw.split(b"\x00") if part]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    args = parser.parse_args()
    try:
        paths = changed_paths(args.base, args.head)
    except (ValueError, OSError, subprocess.CalledProcessError) as err:
        # Conservative default: if history is unavailable, build normally.
        print(f"::warning::Cannot classify diff; full build required: {err}")
        print("docs_only=false")
        return
    docs_only = is_docs_only(paths)
    print(f"::notice::CI change classification: {len(paths)} files, docs_only={str(docs_only).lower()}")
    print("docs_only=" + str(docs_only).lower())


if __name__ == "__main__":
    main()
