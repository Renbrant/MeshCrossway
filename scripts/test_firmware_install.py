#!/usr/bin/env python3
"""Regression tests: a non-erasing installer must not write NVS/PHY sectors."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.folder=Path(self.temp.name)
        self.image=bytearray(b'\xff'*0x18000)
        self.image[0]=self.image[0x10000]=0xe9
        struct.pack_into('<HBBII16sI',self.image,0x8000,0x50aa,1,2,0x9000,0x6000,b'nvs',0)
        (self.folder/'meshcrossway-display-dev.bin').write_bytes(self.image)
    def tearDown(self):self.temp.cleanup()
    def build(self,version='v0.5.8'):
        subprocess.run([sys.executable,str(ROOT/'scripts/write_firmware_version.py'),'--folder',str(self.folder),'--version',version,'--ref','0'*40],check=True,capture_output=True)
    def verify(self,version='v0.5.8'):
        return subprocess.run([sys.executable,str(ROOT/'scripts/verify_firmware_version.py'),'--dir',str(self.folder),'--version',version],capture_output=True).returncode
    def test_preserve_settings_and_verify_parts(self):
        self.build();self.assertEqual(self.verify(),0)
        m=json.loads((self.folder/'manifest.json').read_text());device=bytearray(b'Z'*len(self.image))
        before=device[0x9000:0x10000]
        for p in m['builds'][0]['parts']:
            data=(self.folder/p['path']).read_bytes();device[p['offset']:p['offset']+len(data)]=data
        self.assertEqual(device[0x9000:0x10000],before)
        self.assertEqual(device[0x10000:],self.image[0x10000:])
        self.assertTrue(m['new_install_prompt_erase'])
    def test_reject_corrupt_component(self):
        self.build();p=self.folder/'application.bin';p.write_bytes(p.read_bytes()[:-1]+b'X')
        self.assertNotEqual(self.verify(),0)
    def test_legacy_manifest_unchanged(self):
        self.build('v0.5.6');self.assertEqual(self.verify('v0.5.6'),0)
        m=json.loads((self.folder/'manifest.json').read_text())
        self.assertEqual(m['builds'][0]['parts'],[{'path':'meshcrossway-display-dev.bin','offset':0}])
    def test_reject_unexpected_nvs_layout(self):
        self.image[0x8004]=0;self.image[0x8005]=0xa0
        (self.folder/'meshcrossway-display-dev.bin').write_bytes(self.image)
        with self.assertRaises(subprocess.CalledProcessError):self.build()
    def test_numeric_version_identity_for_current_and_future_candidates(self):
        for version in ('v0.7.12','v0.8.0'):
            with self.subTest(version=version):
                label=version[1:];version_file=self.folder/'VERSION';version_file.write_text(version)
                ui=self.folder/'ui.h';ui.write_text('label("'+label+'");')
                target=self.folder/'versions'/version;target.mkdir(parents=True)
                (target/'build-info.json').write_text(json.dumps({'version':version}))
                binary=target/'meshcrossway-display-dev.bin';binary.write_bytes(b'firmware\x00'+label.encode()+b'\x00')
                command=[sys.executable,str(ROOT/'scripts/check_firmware_identity.py'),'--root',str(self.folder),
                         '--version-file',str(version_file),'--ui-header',str(ui)]
                self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
                binary.write_bytes(b'firmware\x000.7.11\x00')
                self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)
if __name__=='__main__':unittest.main()

