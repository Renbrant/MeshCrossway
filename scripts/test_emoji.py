#!/usr/bin/env python3
"""Offline host tests for MeshCrossway read-only color emoji assets and parser."""
from pathlib import Path
import base64
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2] if __file__.startswith('/mnt/data/') else Path(__file__).resolve().parents[1]
REGISTRY = ROOT / 'firmware/display-prototype/emoji/emoji_registry.inc'
ATLAS = ROOT / 'firmware/display-prototype/emoji/emoji_assets_data.h'
TEST = ROOT / 'tests/test_emoji_presentation.cpp'


def main():
    source = ATLAS.read_text()
    strings = re.findall(r'^\s*"([A-Za-z0-9+/]+)"', source, re.M)
    assert len(strings) > 10, 'missing image atlas or truncated base64'
    data = base64.b64decode(''.join(strings), validate=True)
    assert len(data) == 92 * (16 * 4 + 20 * 20 // 2), len(data)
    expected = '2b1a3721dfb0a5c0f76c534fff071b1fe40f3b149b1a0e61d9a611d90c02fa5a'
    assert hashlib.sha256(data).hexdigest() == expected, 'atlas unexpectedly changed'
    assert REGISTRY.read_text().count(', "') == 92, 'registry does not match 92 sprites'
    for idx in range(92):
        icon = data[idx*264:(idx+1)*264]
        assert len(icon) == 264
        palette = icon[:64]
        alpha = palette[3::4]
        assert max(alpha) > 0, f'blank palette {idx}'
        assert any(value != 0 for value in icon[64:]), f'blank sprite {idx}'
        assert all(value <= 255 for value in palette)
    # ESP-IDF 5.3.x builds the C++ sources with C++20: UTF-8 literals must
    # remain compatible with const char* and not accidentally become char8_t*.
    registry_source = REGISTRY.read_text()
    assert 'u8"' not in registry_source, "C++20 char8_t* is incompatible with const char*"
    with tempfile.TemporaryDirectory(prefix='mcx-emoji-') as temp:
        executable = str(Path(temp)/'test_emoji')
        for standard in ('c++11', 'c++20'):
            build = ['g++', '-std=' + standard, '-Wall', '-Wextra', '-Werror', '-pedantic',
                     '-O2', str(TEST), '-o', executable]
            subprocess.run(build, cwd=ROOT, check=True)
            subprocess.run([executable], cwd=ROOT, check=True)
    print('PASS: 92 color emojis, atlas SHA256, palette/alpha, and UTF-8 host tests on C++11 + C++20')


if __name__ == '__main__':
    main()