#!/usr/bin/env python3
"""Exercise the silent-Kconfig-drop regression and wrong-core rejection."""
import tempfile
import unittest
from pathlib import Path
from check_ble_sdkconfig import REQUIRED, verify

class GeneratedConfigTest(unittest.TestCase):
    def check(self, settings, valid):
        with tempfile.TemporaryDirectory() as temp:
            config = Path(temp) / 'sdkconfig'
            config.write_text('\n'.join(f'{k}={v}' for k, v in settings.items()))
            if valid:
                verify(config)
            else:
                with self.assertRaises(ValueError):
                    verify(config)

    def test_valid(self):
        self.check(REQUIRED, True)

    def test_unknown_choice_does_not_select_wifi_core(self):
        config = dict(REQUIRED)
        del config['CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0']
        config['CONFIG_ESP_WIFI_TASK_CORE_ID'] = '0'
        self.check(config, False)

    def test_each_dropped_requirement_blocks_publication(self):
        for name in REQUIRED:
            with self.subTest(name=name):
                self.check({k: v for k, v in REQUIRED.items() if k != name}, False)

    def test_wrong_host_core(self):
        self.check(dict(REQUIRED, CONFIG_BT_NIMBLE_PINNED_TO_CORE_1='n'), False)

if __name__ == '__main__':
    unittest.main()
