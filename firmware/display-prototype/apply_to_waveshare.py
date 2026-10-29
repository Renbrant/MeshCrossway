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
# Reserve two small DMA buffers at display initialization. The previous PSRAM
# draw buffers required SPI to allocate internal bounce memory during each flush.
# Keep rotation and the UI unchanged; trans_size stays zero (one completion per
# LVGL flush, avoiding the upstream chunked PSRAM completion issue BSP-779).
replacements = {
    '#define LCD_BUFFER_SIZE EXAMPLE_LCD_H_RES *EXAMPLE_LCD_V_RES / 8':
        '#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * 10)',
    'esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);':
        'esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE * sizeof(lv_color_t));',
    '.buff_dma = 0,': '.buff_dma = 1,',
    '.buff_spiram = 1,': '.buff_spiram = 0,',
    'lvgl_disp = lvgl_port_add_disp(&display_cfg);':
        'display_cfg.rounder_cb = [](lv_area_t *area) {\n'
        '        area->x1 &= ~1; area->x2 |= 1; // RGB565 transfers stay 4-byte aligned.\n'
        '    };\n'
        '    lvgl_disp = lvgl_port_add_disp(&display_cfg);\n'
        '    assert(lvgl_disp != nullptr);\n'
        '    printf("[MCX-LCD] reserved DMA draw buffers: 2 x %u bytes; trans_size=0\\n",\n'
        '           (unsigned)(LCD_BUFFER_SIZE * sizeof(lv_color_t)));',
}
for old, new in replacements.items():
    if new in source:
        continue
    if source.count(old) != 1:
        sys.exit('Unsupported upstream DMA display marker: ' + old)
    source = source.replace(old, new, 1)
main.write_text(source)

# Keep the vendor's pinned BSP, but isolate the LCD refresh work from Wi-Fi
# on CPU 0 and lower the peak SPI transfer pressure during BLE coexistence.
# Both adjustments are experimental: they do not prove the root cause.
lvgl_core_marker = 'lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();'
if 'port_cfg.task_affinity = 1;' not in source:
    if source.count(lvgl_core_marker) != 1:
        sys.exit('Missing or duplicate upstream LVGL port config')
    source = source.replace(lvgl_core_marker,
                            lvgl_core_marker + '\n    port_cfg.task_affinity = 1;'
                            + '\n    printf("[MCX-LCD] LVGL task pinned to CPU1\\n");',1)
    main.write_text(source)

# Match the resolved component audited in the successful v0.6.5 build and
# guarantee the rounder_cb API (introduced after the BSP's ^2.5.0 minimum).
component_manifest = main.parent / "idf_component.yml"
manifest = component_manifest.read_text()
old_port = 'espressif/esp_lvgl_port: "^2.5.0"'
new_port = 'espressif/esp_lvgl_port: "2.9.0"'
if new_port not in manifest:
    if manifest.count(old_port) != 1:
        sys.exit('Unsupported upstream LVGL port dependency')
    component_manifest.write_text(manifest.replace(old_port, new_port, 1))

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

# The vendor ST7796 implementation discarded tx_color's result and returned
# ESP_OK even if no completion callback would arrive. Log the real return code
# and reset safely (BLE is off on boot) instead of leaving LVGL spinning forever.
panel_driver = project / "components" / "esp_lcd_st7796" / "esp_lcd_st7796.c"
panel = panel_driver.read_text()
old_color = '    esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);'
new_color = '''    esp_err_t color_result = esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);
    if (color_result != ESP_OK) {
        ESP_LOGE(TAG, "[MCX-LCD] tx_color failed err=%s (0x%x) bytes=%u dma=%u largest_dma=%u source_dma=%d",
                 esp_err_to_name(color_result), (unsigned)color_result, (unsigned)len,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                 (int)esp_ptr_dma_capable(color_data));
        // Do not fake flush-ready: earlier chunks may still reference the buffer.
        ESP_ERROR_CHECK(color_result);
    }'''
if new_color not in panel:
    if panel.count(old_color) != 1:
        sys.exit('Unsupported upstream ST7796 color transfer marker')
    panel = panel.replace(old_color, new_color, 1)
    panel = '#include "esp_heap_caps.h"\n#include "esp_memory_utils.h"\n' + panel
panel_driver.write_text(panel)

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
             "meshcrossway_remote_page.h", "meshcrossway_network.h", "meshcrossway_ble.h", "meshcrossway_meshcore.h",
             "meshcrossway_meshcore_protocol.h", "meshcrossway_meshcore_ui.h"):
    shutil.copy2(root / "firmware" / "display-prototype" / name,
                 project / "main" / name)

sdk = project / "sdkconfig.defaults"
defaults = sdk.read_text()
# Remove the invalid choice name used in the earlier candidate.
defaults = "\n".join(line for line in defaults.splitlines()
                     if not line.startswith("CONFIG_ESP_WIFI_TASK_CORE_ID=")) + "\n"
sdk.write_text(defaults)
# The Wi-Fi backup uses /sdcard/meshcrossway-wifi.{tmp,json,bak}.
# ESP-IDF FatFs defaults to FATFS_LFN_NONE, so those paths otherwise fail
# to open even on correctly mounted FAT32 cards.
needed=["CONFIG_LV_USE_SNAPSHOT=y","CONFIG_FATFS_LFN_HEAP=y",
        "CONFIG_BT_ENABLED=y","CONFIG_BT_NIMBLE_ENABLED=y",
        "CONFIG_BT_NIMBLE_ROLE_CENTRAL=y","CONFIG_BT_NIMBLE_MAX_CONNECTIONS=2",
        "CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=247",
        "CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y",
        "CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y",
        "CONFIG_BT_CTRL_PINNED_TO_CORE_1=y",
        "CONFIG_BT_NIMBLE_PINNED_TO_CORE_1=y"]
missing=[option for option in needed if option not in defaults.splitlines()]
if missing:
    sdk.write_text(defaults.rstrip("\n")+"\n"+"\n".join(missing)+"\n")
print(f"MeshCrossway patched and verified: {project}")
