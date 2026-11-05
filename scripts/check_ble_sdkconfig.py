#!/usr/bin/env python3
"""Fail the candidate build if Kconfig silently drops coexistence requirements."""
import argparse
from pathlib import Path

REQUIRED = {
    'CONFIG_LV_USE_PNG': 'y',
    'CONFIG_MBEDTLS_CERTIFICATE_BUNDLE': 'y',
    'CONFIG_MBEDTLS_HAVE_TIME_DATE': 'y',
    'CONFIG_MBEDTLS_TLS_ENABLED': 'y',
    'CONFIG_MBEDTLS_SSL_PROTO_TLS1_2': 'y',
    'CONFIG_MBEDTLS_AES_C': 'y',
    'CONFIG_MBEDTLS_GCM_C': 'y',
    'CONFIG_MBEDTLS_CCM_C': 'y',
    'CONFIG_MBEDTLS_HARDWARE_SHA': 'y',
    'CONFIG_MBEDTLS_HARDWARE_MPI': 'y',
    'CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC': 'y',
    'CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG': 'y',
    'CONFIG_IDF_TARGET': '"esp32s3"',
    'CONFIG_BT_ENABLED': 'y',
    'CONFIG_BT_NIMBLE_ENABLED': 'y',
    'CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE': '3152',
    'CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL': 'y',
    'CONFIG_BT_NIMBLE_ROLE_CENTRAL': 'y',
    'CONFIG_BT_NIMBLE_MAX_CONNECTIONS': '2',
    'CONFIG_BT_NIMBLE_NVS_PERSIST': 'y',
    'CONFIG_BT_NIMBLE_MAX_BONDS': '4',
    'CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU': '517',
    'CONFIG_ESP_COEX_SW_COEXIST_ENABLE': 'y',
    'CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0': 'y',
    'CONFIG_BT_CTRL_PINNED_TO_CORE_1': 'y',
    'CONFIG_BT_NIMBLE_PINNED_TO_CORE_1': 'y',
    # Freeze the v0.9.21 networking budget around the single RX-placement trial.
    'CONFIG_ESP_WIFI_IRAM_OPT': 'y',
    'CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM': '10',
    'CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM': '32',
    'CONFIG_ESP_WIFI_STATIC_TX_BUFFER': 'y',
    'CONFIG_ESP_WIFI_TX_BUFFER_TYPE': '0',
    'CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM': '16',
    'CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM': '32',
    'CONFIG_ESP_WIFI_AMPDU_TX_ENABLED': 'y',
    'CONFIG_ESP_WIFI_TX_BA_WIN': '6',
    'CONFIG_ESP_WIFI_AMPDU_RX_ENABLED': 'y',
    'CONFIG_ESP_WIFI_RX_BA_WIN': '6',
    'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL': '16384',
    'CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL': '32768',
    'CONFIG_FATFS_LFN_HEAP': 'y',
    'CONFIG_LV_USE_SNAPSHOT': 'y',
    'CONFIG_LV_USE_IMGFONT': 'y',
    'CONFIG_LV_MEM_CUSTOM': 'y',
    'CONFIG_LV_MEM_CUSTOM_INCLUDE': '"meshcrossway_lvgl_alloc.h"',
    'CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY': 'y',
    'CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK': 'y',
}

DISABLED = ('CONFIG_ESP_WIFI_RX_IRAM_OPT', 'CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP',
            'CONFIG_MBEDTLS_HARDWARE_AES', 'CONFIG_MBEDTLS_AES_USE_INTERRUPT')

def verify(path):
    config = dict(line.split('=', 1) for line in path.read_text().splitlines()
                  if line.startswith('CONFIG_') and '=' in line)
    errors = [f'{name}: expected {expected}, got {config.get(name, "unset")}'
              for name, expected in REQUIRED.items() if config.get(name) != expected]
    errors.extend(f'{name}: must remain disabled in the software-AES map candidate'
                  for name in DISABLED if config.get(name, 'n') != 'n')
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
    print('PASS: generated sdkconfig enables BLE central, CPU affinity, coexistence, preserved features and RX-flash / software-AES map profile')

