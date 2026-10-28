#!/usr/bin/env python3
"""Patch a pinned Waveshare LVGL example for MeshCrossway; repeatable and fail-fast."""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
vendor = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "vendor" / "ESP32-S3-Touch-LCD-3.5"
project = vendor / "examples" / "esp-idf" / "02_lvgl_example"
main = project / "main" / "main.cpp"
if not main.is_file():
    sys.exit(f"Missing Waveshare example: {main}")
source = main.read_text()
if '#include "meshcrossway_ui.h"' not in source:
    if '#define EXAMPLE_DISPLAY_ROTATION 0' not in source:
        sys.exit("Unsupported upstream version: rotation marker missing")
    if 'lv_demo_widgets();' not in source or '#include "lvgl_ui.h"' not in source:
        sys.exit("Unsupported upstream version: LVGL markers missing")
    anchor = '        lvgl_port_unlock();\n    }\n}'
    if anchor not in source:
        sys.exit("Unsupported upstream version: app_main() marker missing")
    source = source.replace('#define EXAMPLE_DISPLAY_ROTATION 0',
                            '#define EXAMPLE_DISPLAY_ROTATION 90', 1)
    source = source.replace('#include "lvgl_ui.h"',
                            '#include "lvgl_ui.h"\n#include "meshcrossway_ui.h"\n#include "meshcrossway_health.h"\n#include "meshcrossway_capture.h"', 1)
    source = source.replace('lv_demo_widgets();', 'meshcrossway::init();\n        meshcapture::init();', 1)
    source = source.replace(anchor,
                            '        lvgl_port_unlock();\n    }\n    meshnetwork::start();\n    meshhealth::start();\n}', 1)
    main.write_text(source)

if '#include "meshcrossway_ble.h"' not in source:
    source=source.replace('#include "meshcrossway_ui.h"', '#include "meshcrossway_ui.h"\n#include "meshcrossway_ble.h"',1)
    main.write_text(source)
# Keep the vendor's pinned BSP, but isolate the LCD refresh work from Wi-Fi
# on CPU 0 and lower the peak SPI transfer pressure during BLE coexistence.
# Both adjustments are experimental: they do not prove the root cause.
lvgl_core_marker = 'lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();'
if 'port_cfg.task_affinity = 1;' not in source:
    if source.count(lvgl_core_marker) != 1:
        sys.exit('Missing or duplicate upstream LVGL port config')
    source = source.replace(lvgl_core_marker,
                            lvgl_core_marker + '\\n    port_cfg.task_affinity = 1;'
                            + '\\n    printf("[MCX-LCD] LVGL task pinned to CPU1\\\\n");',1)
    main.write_text(source)

lcd_driver = project / "components" / "esp_port" / "esp_3inch5_lcd_port.cpp"
if not lcd_driver.is_file():
    sys.exit(f'Missing pinned Waveshare LCD SPI driver: {lcd_driver}')
lcd = lcd_driver.read_text()
if '#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)' not in lcd:
    if lcd.count('#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)') != 1:
        sys.exit('Unexpected upstream LCD SPI clock')
    lcd = lcd.replace('#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)',
                      '#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)',1)
if 'io_config.trans_queue_depth = 6;' not in lcd:
    if lcd.count('io_config.trans_queue_depth = 10;') != 1:
        sys.exit('Unexpected upstream SPI queue depth')
    lcd = lcd.replace('io_config.trans_queue_depth = 10;',
                      'io_config.trans_queue_depth = 6;',1)
lcd_driver.write_text(lcd)

if 'meshremote::register_input();' in source or 'meshremote::start();' in source:
    sys.exit('Diagnostic firmware must not activate remote LVGL input or screenshot HTTP')
if source.count('meshcapture::init();') != 1:
    sys.exit('Missing or duplicate single-shot LVGL capture initialization')
if source.count('meshhealth::start();') != 1:
    sys.exit('Missing or duplicate status-only HTTP initialization')
if 'meshble::start();' in source or 'meshble::request_start();' in source:
    sys.exit('BLE must not start automatically at boot')
if source.count('meshnetwork::start();') != 1:
    sys.exit('Missing or duplicate isolated Wi-Fi initialization')
for name in ("meshcrossway_capture.h", "meshcrossway_backup.h", "meshcrossway_health.h", "meshcrossway_ui.h", "meshcrossway_remote.h",
             "meshcrossway_remote_page.h", "meshcrossway_network.h", "meshcrossway_ble.h"):
    shutil.copy2(root / "firmware" / "display-prototype" / name,
                 project / "main" / name)

sdk = project / "sdkconfig.defaults"
defaults = sdk.read_text()
# The Wi-Fi backup uses /sdcard/meshcrossway-wifi.{tmp,json,bak}.
# ESP-IDF FatFs defaults to FATFS_LFN_NONE, so those paths otherwise fail
# to open even on correctly mounted FAT32 cards.
needed=["CONFIG_LV_USE_SNAPSHOT=y","CONFIG_FATFS_LFN_HEAP=y",
        "CONFIG_BT_ENABLED=y","CONFIG_BT_NIMBLE_ENABLED=y",
        "CONFIG_BT_NIMBLE_ROLE_CENTRAL=y","CONFIG_BT_NIMBLE_MAX_CONNECTIONS=2",
        "CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y",
        "CONFIG_ESP_WIFI_TASK_CORE_ID=0",
        "CONFIG_BT_CTRL_PINNED_TO_CORE_1=y",
        "CONFIG_BT_NIMBLE_PINNED_TO_CORE_1=y"]
missing=[option for option in needed if option not in defaults.splitlines()]
if missing:
    sdk.write_text(defaults.rstrip("\n")+"\n"+"\n".join(missing)+"\n")
print(f"MeshCrossway patched and verified: {project}")
