#!/usr/bin/env python3
"""Package existing source-level notices beside firmware without touching images.

This preserves current license texts; it does not claim a complete review of
all resolved binary dependencies (see THIRD_PARTY_NOTICES.md).
"""
import argparse
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
for rel in ['THIRD_PARTY_NOTICES.md',*[str(v.relative_to(ROOT)) for v in sorted((ROOT/'LICENSES').glob('*.txt'))],
            'docs/emoji-coverage-v0929.md','docs/emoji-v0929-validation.md','scripts/emoji/coverage.json']:
    target=a.output/rel
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/rel,target)
assert (a.output/'LICENSES/NotoEmoji-OFL-1.1.txt').read_bytes()==(ROOT/'LICENSES/NotoEmoji-OFL-1.1.txt').read_bytes()
print('PASS: firmware source-level notices and unmodified Noto OFL packaged; binary bytes untouched')
