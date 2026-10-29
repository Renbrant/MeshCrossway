#!/usr/bin/env python3
"""Fail the candidate build if Kconfig silently drops coexistence requirements."""
import argparse
from pathlib import Path

REQUIRED = {
    'CONFIG_IDF_TARGET': '"esp32s3"',
    'CONFIG_BT_ENABLED': 'y',
    'CONFIG_BT_NIMBLE_ENABLED': 'y',
    'CONFIG_BT_NIMBLE_ROLE_CENTRAL': 'y',
    'CONFIG_BT_NIMBLE_MAX_CONNECTIONS': '2',
    'CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU': '247',
    'CONFIG_ESP_COEX_SW_COEXIST_ENABLE': 'y',
    'CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0': 'y',
    'CONFIG_BT_CTRL_PINNED_TO_CORE_1': 'y',
    'CONFIG_BT_NIMBLE_PINNED_TO_CORE_1': 'y',
    'CONFIG_FATFS_LFN_HEAP': 'y',
    'CONFIG_LV_USE_SNAPSHOT': 'y',
}

def verify(path):
    config = dict(line.split('=', 1) for line in path.read_text().splitlines()
                  if line.startswith('CONFIG_') and '=' in line)
    errors = [f'{name}: expected {expected}, got {config.get(name, "unset")}'
              for name, expected in REQUIRED.items() if config.get(name) != expected]
    if errors:
        raise ValueError('Generated sdkconfig rejected:\n' + '\n'.join(errors))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sdkconfig', type=Path)
    args = parser.parse_args()
    try:
        verify(args.sdkconfig)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    print('PASS: generated sdkconfig enables BLE central, CPU affinity, coexistence and preserved features')
