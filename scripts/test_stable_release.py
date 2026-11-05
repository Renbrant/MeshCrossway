#!/usr/bin/env python3
"""Stable promotion must retain recovery and reject a rebuilt/relabelled image."""
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import restore_stable_firmware as stable

ROOT = Path(__file__).resolve().parents[1]

class StableReleaseTests(unittest.TestCase):
    def test_builder_pins_match_approved_registry(self):
        text=(ROOT/'scripts/build_firmware_versions.sh').read_text()
        values={k:re.search(r'^'+k+r'="([^"]+)"$',text,re.M)[1]
                for k in ('STABLE_VERSION','STABLE_REF','STABLE_SHA256','RECOVERY_VERSION','RECOVERY_REF')}
        self.assertEqual(values['STABLE_VERSION'],stable.VERSION)
        self.assertEqual((values['STABLE_REF'],values['STABLE_SHA256']),stable.APPROVED[stable.VERSION][:2])
        self.assertEqual(values['RECOVERY_REF'],stable.APPROVED[values['RECOVERY_VERSION']][0])

    def image(self, folder, version, ref):
        folder.mkdir(parents=True,exist_ok=True)
        data=bytearray(b'\xff'*0x18000);data[0]=data[0x10000]=0xe9
        struct.pack_into('<HBBII16sI',data,0x8000,0x50aa,1,2,0x9000,0x6000,b'nvs',0)
        (folder/'meshcrossway-display-dev.bin').write_bytes(data)
        self.write(folder,version,ref)

    def write(self,folder,version,ref):
        subprocess.run(['python3',str(ROOT/'scripts/write_firmware_version.py'),'--folder',str(folder),
                        '--version',version,'--ref',ref],check=True,capture_output=True)

    def test_pinned_image_rejects_consistently_rewritten_metadata(self):
        with tempfile.TemporaryDirectory() as t:
            folder=Path(t);ref='0'*40;self.image(folder,'v0.9.25',ref)
            sha=hashlib.sha256((folder/'meshcrossway-display-dev.bin').read_bytes()).hexdigest()
            with patch.dict(stable.APPROVED,{'v0.9.25':(ref,sha,0,'unused')}):
                stable.verify(folder,'v0.9.25')
                image=folder/'meshcrossway-display-dev.bin';data=bytearray(image.read_bytes());data[0x10020]=42;image.write_bytes(data)
                self.write(folder,'v0.9.25',ref) # All self-reported hashes now agree with the altered image.
                with self.assertRaises(ValueError):stable.verify(folder,'v0.9.25')

    def test_catalog_retains_recovery_and_selects_new_stable(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)
            for version in ('v0.9.16','v0.9.25'):self.image(root/'versions'/version,version,'0'*40)
            subprocess.run(['python3',str(ROOT/'scripts/write_firmware_catalog.py'),'--root',str(root),
                            '--stable','v0.9.25','--latest','v0.9.25'],check=True,capture_output=True)
            c=json.loads((root/'catalog.json').read_text())
            self.assertEqual(c['recommended'],'v0.9.25')
            self.assertEqual({v['version'] for v in c['versions']},{'v0.9.16','v0.9.25'})
            self.assertTrue(all(v['status']=='hardware-tested' for v in c['versions']))

if __name__=='__main__':unittest.main()
