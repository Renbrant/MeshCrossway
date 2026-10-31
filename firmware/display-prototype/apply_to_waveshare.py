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
                            '        lvgl_port_unlock();\n    }\n    meshnetwork::start();\n    meshhealth::start();\n    meshble::schedule_auto_resume();\n}', 1)
    main.write_text(source)

if '#include "meshcrossway_ble.h"' not in source:
    source=source.replace('#include "meshcrossway_ui.h"', '#include "meshcrossway_ui.h"\n#include "meshcrossway_ble.h"',1)
    main.write_text(source)
# Keep the SDK pointer and its user_data; only replace its read path under lock.
if 'mcxtouch::attach(lvgl_touch_indev, touch_handle);' not in source:
    marker = 'meshcrossway::init();'
    if source.count(marker) != 1:
        sys.exit('Missing or duplicate UI initialization marker')
    source = source.replace(marker,
        'mcxtouch::attach(lvgl_touch_indev, touch_handle);\n        ' + marker, 1)
    source = source.replace('#include "meshcrossway_ui.h"',
        '#include "meshcrossway_ui.h"\n#include "meshcrossway_offline_boot.h"', 1)
    main.write_text(source)
# Network startup attempts the SD mount. Restoring earlier can mistake an
# unmounted card for an empty archive. Bootstrap under the UI lock afterwards,
# still independently of Bluetooth and its 12-second auto-resume delay.
source = source.replace('        mcxoffline::begin();\n', '')  # migrate this patcher's earlier hook
if '    // MCX offline restore after SD mount attempt' not in source:
    marker = '    meshnetwork::start();'
    if source.count(marker) != 1:
        sys.exit('Missing or duplicate network startup marker')
    source = source.replace(marker, marker + '\n    // MCX offline restore after SD mount attempt'
        + '\n    if (lvgl_port_lock(0)) { mcxoffline::begin(); lvgl_port_unlock(); }', 1)
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
             "meshcrossway_remote_page.h", "meshcrossway_companion_page.h", "meshcrossway_track_map_js.h", "meshcrossway_remote_chat.h", "meshcrossway_remote_chat_http.h", "meshcrossway_remote_radio.h", "meshcrossway_network.h", "meshcrossway_ble.h", "meshcrossway_meshcore.h",
             "meshcrossway_meshtastic_protocol.h", "meshcrossway_meshtastic.h", "meshcrossway_meshtastic_store.h", "meshcrossway_meshtastic_ui.h",
             "meshcrossway_meshcore_protocol.h", "meshcrossway_meshcore_ui.h", "meshcrossway_sd_history.h", "meshcrossway_history_store.h", "meshcrossway_conversations.h",
             "meshcrossway_device_key.h", "meshcrossway_brand.h", "meshcrossway_edge_swipe.h", "meshcrossway_lvgl_alloc.h", "meshcrossway_runtime.h",
             "meshcrossway_power_status.h", "meshcrossway_battery_decode.h",
             "meshcrossway_chat_presentation.h", "meshcrossway_chat_view.h", "meshcrossway_offline_boot.h",
             "meshcrossway_touch_queue.h", "meshcrossway_touch_input.h", "meshcrossway_gnss.h", "meshcrossway_home_status.h", "meshcrossway_clock.h", "meshcrossway_clock_runtime.h", "meshcrossway_gnss_parser.h", "meshcrossway_gnss_settings.h", "meshcrossway_radio_priority.h", "meshcrossway_map_projection.h", "meshcrossway_map_tiles.h", "meshcrossway_navigation_ui.h", "meshcrossway_navigation.h", "meshcrossway_navigation_store.h", "meshcrossway_navigation_http.h"):
    shutil.copy2(root / "firmware" / "display-prototype" / name,
                 project / "main" / name)

shutil.copytree(root / "firmware" / "display-prototype" / "timezone", project / "main" / "timezone", dirs_exist_ok=True)

# Read-only color emoji sprites for LVGL 8.4; keep device offline.
emoji_root = root / "firmware" / "display-prototype" / "emoji"
emoji_target = project / "main" / "emoji"
emoji_target.mkdir(parents=True, exist_ok=True)
for name in ("emoji_presentation.h", "emoji_registry.inc", "emoji_assets_data.h", "emoji_flag_asset.h", "emoji_small_sprite.h", "emoji_display.h"):
    shutil.copy2(emoji_root / name, emoji_target / name)

sdk = project / "sdkconfig.defaults"
defaults = sdk.read_text()
# Remove the invalid choice name used in the earlier candidate.
defaults = "\n".join(line for line in defaults.splitlines()
                     if not line.startswith(("CONFIG_ESP_WIFI_TASK_CORE_ID=", "CONFIG_ESP_CONSOLE_UART", "CONFIG_ESP_CONSOLE_SECONDARY_", "CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=", "CONFIG_MBEDTLS_DEFAULT_MEM_ALLOC=", "CONFIG_MBEDTLS_IRAM_8BIT_MEM_ALLOC=", "CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=", "CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_DEFAULT=", "CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_IRAM_8BIT="))) + "\n"
sdk.write_text(defaults)
# The Wi-Fi backup uses /sdcard/meshcrossway-wifi.{tmp,json,bak}.
# ESP-IDF FatFs defaults to FATFS_LFN_NONE, so those paths otherwise fail
# to open even on correctly mounted FAT32 cards.
needed=["CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y", "CONFIG_MBEDTLS_HAVE_TIME_DATE=y", "CONFIG_LV_USE_PNG=y", "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y","CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y", "# CONFIG_ESP_CONSOLE_UART_DEFAULT is not set", "# CONFIG_ESP_CONSOLE_UART_CUSTOM is not set", "# CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG is not set","CONFIG_LV_USE_SNAPSHOT=y","CONFIG_LV_USE_IMGFONT=y","CONFIG_LV_FONT_MONTSERRAT_10=y", "CONFIG_LV_FONT_MONTSERRAT_14=y",
         "CONFIG_LV_MEM_CUSTOM=y",'CONFIG_LV_MEM_CUSTOM_INCLUDE="meshcrossway_lvgl_alloc.h"',
         "CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y","CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y",
         "CONFIG_FATFS_LFN_HEAP=y",
        "CONFIG_BT_ENABLED=y","CONFIG_BT_NIMBLE_ENABLED=y","CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y",
        "CONFIG_BT_NIMBLE_ROLE_CENTRAL=y","CONFIG_BT_NIMBLE_MAX_CONNECTIONS=2",
        "CONFIG_BT_NIMBLE_NVS_PERSIST=y","CONFIG_BT_NIMBLE_MAX_BONDS=4",
        "CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=517",
        "CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y",
        "CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y",
        "CONFIG_BT_CTRL_PINNED_TO_CORE_1=y",
        "CONFIG_BT_NIMBLE_PINNED_TO_CORE_1=y"]
missing=[option for option in needed if option not in defaults.splitlines()]
if missing:
    sdk.write_text(defaults.rstrip("\n")+"\n"+"\n".join(missing)+"\n")
# The managed LVGL component must be able to include the allocator header while
# compiling lv_mem.c. Its symbols are static inline; no reverse link dependency.
cmake=project/"main"/"CMakeLists.txt"
allocator_build='''
# MeshCrossway: isolate LVGL allocations from BLE/DMA internal RAM.
idf_component_get_property(mcx_lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_include_directories(${mcx_lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wframe-larger-than=4096 -fstack-usage)
'''
if "idf_component_get_property(mcx_lvgl_lib" not in cmake.read_text():
    cmake.write_text(cmake.read_text()+allocator_build)
print(f"MeshCrossway patched and verified: {project}")

