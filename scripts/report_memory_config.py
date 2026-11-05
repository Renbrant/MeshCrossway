#!/usr/bin/env python3
"""Retain effective memory, networking, stack and display choices for issue #137."""
import json
import re
import sys
from pathlib import Path

project = Path(sys.argv[1])
output = project / 'build/memory-report'
output.mkdir(parents=True, exist_ok=True)
pattern = re.compile(r'CONFIG_(?:SPIRAM|ESP_WIFI|LWIP|BT_|FREERTOS|ESP_MAIN_TASK|ESP_SYSTEM_EVENT_TASK|MBEDTLS|LV_MEM|LV_COLOR|.*CACHE).*')
settings = {}
for line in (project / 'sdkconfig').read_text().splitlines():
    if line.startswith('# CONFIG_') and line.endswith(' is not set'):
        key, value = line[2:-11], 'n'
    elif '=' in line:
        key, value = line.split('=', 1)
    else:
        continue
    if pattern.fullmatch(key):
        settings[key] = value
report = {'schema': 1, 'units': 'bytes unless configuration help states otherwise',
          'effective_sdkconfig': settings,
          'note': 'Static sizes are in size/components/files.txt. Runtime heap, task presence and watermarks require hardware logs. DMA dimensions must be read from generated main.cpp and the boot log.'}
(output / 'effective-config.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
print('Memory build report:', output)
