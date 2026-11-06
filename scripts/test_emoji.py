#!/usr/bin/env python3
"""Offline host tests for MeshCrossway read-only color emoji assets and parser."""
from pathlib import Path
import base64
import hashlib
import json
import re
import sys
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2] if __file__.startswith('/mnt/data/') else Path(__file__).resolve().parents[1]
REGISTRY = ROOT / 'firmware/display-prototype/emoji/emoji_registry.inc'
ATLAS = ROOT / 'firmware/display-prototype/emoji/emoji_assets_data.h'
TEST = ROOT / 'tests/test_emoji_presentation.cpp'
SPRITE_TEST = ROOT / 'tests/test_emoji_sprite.cpp'
FLAG = ROOT / 'firmware/display-prototype/emoji/emoji_flag_asset.h'


def main():
    source = ATLAS.read_text()
    strings = re.findall(r'^\s*"([A-Za-z0-9+/]+)"', source, re.M)
    assert len(strings) > 10, 'missing image atlas or truncated base64'
    data = base64.b64decode(''.join(strings), validate=True)
    assert len(data) == 92 * (16 * 4 + 20 * 20 // 2), len(data)
    expected = '2b1a3721dfb0a5c0f76c534fff071b1fe40f3b149b1a0e61d9a611d90c02fa5a'
    assert hashlib.sha256(data).hexdigest() == expected, 'atlas unexpectedly changed'
    sys.path.insert(0, str(ROOT/'scripts'))
    import build_emoji_catalog as catalog
    rows=catalog.registry()
    report=json.loads((ROOT/'scripts/emoji/coverage.json').read_text())
    assert len(rows)==report['current_glyphs']==750
    assert rows[:93]==catalog.legacy(), 'legacy PUA mappings must remain append-only'
    assert not catalog.audit(rows)['missing_visible_after'], 'reference visible mappings missing'
    packed=b''.join(bytes.fromhex(''.join(re.findall(r'\\x([0-9a-f]{2})', p.read_text())))
                    for p in sorted(REGISTRY.parent.glob('emoji_sprites_*.inc')))
    assert len(packed)==750*164
    sprites=[packed[i:i+162] for i in range(0,len(packed),164)]
    assert all(packed[i+162:i+164]==b'\0\0' for i in range(0,len(packed),164))
    assert hashlib.sha256(b''.join(sprites)).hexdigest()==report['sprite_sha256']
    assert sprites[:93]==catalog.legacy_sprites(), 'legacy display pixels changed'
    assert all(any(v[3:64:4]) and any(v[64:]) for v in sprites), 'empty sprite'
    order=[int(v) for v in re.findall(r'\d+', (REGISTRY.parent/'emoji_lookup_order.inc').read_text().split('\n',1)[1])]
    assert sorted(order)==list(range(750))
    keys=[catalog.normalize(rows[i][0]).encode() for i in order]
    assert keys==sorted(set(keys)), 'lookup order or canonical uniqueness invalid'
    for k in ('flags','tone_sequences','zwj_sequences','sprite_bytes','sprite_storage_bytes'):
        assert report[k]==catalog.audit(rows)[k]
    flag_header=FLAG.read_text()
    match=re.search(r'kCheckeredFlagBase64\[\]\s*=\s*"([A-Za-z0-9+/]+)"',flag_header)
    assert match, 'missing flag asset'
    flag=base64.b64decode(match.group(1),validate=True)
    assert len(flag)==264
    assert hashlib.sha256(flag).hexdigest()=='d2b5f63df3ea2532a2c524b36f1ceb08cffc344c212fb3659dcfd598dc70731a'
    assert any(flag[64:]) and any(flag[i] for i in range(3,64,4)), 'blank flag icon'
    assert r'\U0001F3C1' in REGISTRY.read_text(), 'checkered flag missing from registry'
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
            for test in (TEST, SPRITE_TEST, ROOT/'tests/test_emoji_assets.cpp'):
                build = ['g++', '-std=' + standard, '-Wall', '-Wextra', '-Werror', '-pedantic',
                         '-O2', str(test), '-o', executable]
                subprocess.run(build, cwd=ROOT, check=True)
                subprocess.run([executable], cwd=ROOT, check=True)
    print('PASS: 750 flash sprites, all 272 reference mappings, 93 legacy mappings/pixels preserved, UTF-8/sequence tests C++11 + C++20')


if __name__ == '__main__':
    main()