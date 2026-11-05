#!/usr/bin/env python3
"""Offline safety checks before the ESP-IDF compile/deploy stages."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / "firmware" / "display-prototype"
remote = (FW / "meshcrossway_remote.h").read_text()
page = (FW / "meshcrossway_remote_page.h").read_text()
patcher = FW / "apply_to_waveshare.py"
version = (FW / "VERSION").read_text().strip()
errors = []

ui=(FW / "meshcrossway_ui.h").read_text()
if (FW/"meshcrossway_meshcore_ui.h").exists():ui+=(FW/"meshcrossway_meshcore_ui.h").read_text()
if version=="v0.5.4":
    anchor=ui.index("lv_obj_set_align(wifi_keyboard,LV_ALIGN_TOP_LEFT)")
    assert anchor<ui.index("lv_obj_set_pos(wifi_keyboard,4,185)"), "Keyboard must use top-left coordinates"
if not version.startswith(("v0.9.","v0.10.")) and version not in ("v0.5.1", "v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
    errors.append("Unexpected version: " + version)
if "HTTPD_503_SERVICE_UNAVAILABLE" in remote:
    errors.append("Unsupported HTTPD_503_SERVICE_UNAVAILABLE token")
if "\n else" in remote:
    errors.append("Literal backslash-n outside a C++ string")
if 'lv_snapshot.h"' in remote:
    errors.append("Do not include internal LVGL snapshot header directly")
if not all(value in remote for value in ('#include "lvgl.h"', 'lv_snapshot_take(', 'lv_snapshot_free(', 'httpd_resp_set_status(', 'LV_USE_SNAPSHOT', 'lv_indev_drv_register(', '/touch', '/screen.raw')):
    errors.append("Missing LVGL snapshot or HTTP API")
try:
    js = page.split("<script>", 1)[1].split("</script>", 1)[0]
except IndexError:
    errors.append("Missing embedded browser script")
    js = ""
if js:
    p = subprocess.run(["node", "--check", "-"], input=js, text=True, capture_output=True)
    if p.returncode:
        errors.append("Browser JS invalid: " + p.stderr)
try:
    compile(patcher.read_text(), str(patcher), "exec")
except SyntaxError as exc:
    errors.append("Patcher invalid: " + str(exc))

if not errors:
    with tempfile.TemporaryDirectory() as temp:
        vendor = Path(temp) / "ESP32-S3-Touch-LCD-3.5"
        example = vendor / "examples" / "esp-idf" / "02_lvgl_example"
        main = example / "main" / "main.cpp"
        main.parent.mkdir(parents=True)
        main.write_text('''#include "lvgl_ui.h"
#define EXAMPLE_DISPLAY_ROTATION 0
#define LCD_BUFFER_SIZE EXAMPLE_LCD_H_RES *EXAMPLE_LCD_V_RES / 8
void app(){
    esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
    .buff_dma = 0,
    .buff_spiram = 1,
    lvgl_disp = lvgl_port_add_disp(&display_cfg);
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_lock(0)) {
        lv_demo_widgets();
        lvgl_port_unlock();
    }
}
''')
        lcd_fixture=example/"components"/"esp_port"/"esp_3inch5_lcd_port.cpp"
        lcd_fixture.parent.mkdir(parents=True,exist_ok=True)
        lcd_fixture.write_text('#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)\\n'
                               'io_config.trans_queue_depth = 10;\\n')
        (main.parent / "idf_component.yml").write_text('dependencies:\n  espressif/esp_lvgl_port: "^2.5.0"\n')
        panel_fixture=example/"components"/"esp_lcd_st7796"/"esp_lcd_st7796.c"
        panel_fixture.parent.mkdir(parents=True,exist_ok=True)
        panel_fixture.write_text('    esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);\n')
        (main.parent / "CMakeLists.txt").write_text('idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")\n')
        (example / "sdkconfig.defaults").write_text('CONFIG_IDF_TARGET="esp32s3"\nCONFIG_ESP_WIFI_RX_IRAM_OPT=y\nCONFIG_MBEDTLS_HARDWARE_AES=y\n')
        patched_defaults=[]
        for attempt in range(2):
            p = subprocess.run([sys.executable, str(patcher), str(vendor)], text=True, capture_output=True)
            if p.returncode:
                errors.append("Patcher failed: " + p.stdout + p.stderr)
                break
            patched_defaults.append((example / "sdkconfig.defaults").read_text())
        if version in ("v0.9.22","v0.9.23","v0.9.24"):
            assert len(patched_defaults)==2 and patched_defaults[0]==patched_defaults[1], "SDK defaults must be idempotent"
            assert "CONFIG_ESP_WIFI_RX_IRAM_OPT=y" not in patched_defaults[-1]
            assert patched_defaults[-1].count("# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set")==1
            if version in ("v0.9.23","v0.9.24"):
                assert "CONFIG_MBEDTLS_HARDWARE_AES=y" not in patched_defaults[-1]
                assert patched_defaults[-1].count("# CONFIG_MBEDTLS_HARDWARE_AES is not set")==1
        output = main.read_text()
        patched_lcd = lcd_fixture.read_text()
        patched_panel = panel_fixture.read_text()
        patched_manifest = (main.parent / 'idf_component.yml').read_text()
        patched_sdkconfig = (example / "sdkconfig.defaults").read_text()
        if version in ("v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            if (output.count("meshnetwork::start();") != 1 or output.count("meshcrossway::init();") != 1 or "meshremote::start();" in output or "meshremote::register_input();" in output):
                errors.append("Diagnostic patcher must start only Wi-Fi; no remote HTTP or LVGL input")
            if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") and output.count("meshhealth::start();")!=1:
                errors.append("Status HTTP must start once")
        elif (output.count("meshremote::start();") != 1 or output.count("meshcrossway::init();") != 1 or output.count("meshremote::register_input();") != 1):
            errors.append("Patcher not idempotent for main.cpp")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            if output.count("meshcapture::init();")!=1 or '#include "meshcrossway_capture.h"' not in output:
                errors.append("v0.5.10 must initialize one capture timer within LVGL lock")
        if '#define EXAMPLE_DISPLAY_ROTATION 90' not in output:
            errors.append("Display rotation missing")
        if 'CONFIG_LV_USE_SNAPSHOT=y' not in (example / "sdkconfig.defaults").read_text():
            errors.append("Missing snapshot config")
        if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            patched_config=(example / "sdkconfig.defaults").read_text()
            if "CONFIG_FATFS_LFN_HEAP=y" not in patched_config or "CONFIG_FATFS_LFN_NONE=y" in patched_config:
                errors.append("Wi-Fi SD backup requires FATFS heap long filename support")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") and not (main.parent / "meshcrossway_capture.h").is_file():
            errors.append("Manual remote capture header not copied into vendor example")
        if not (main.parent / "meshcrossway_remote.h").is_file():
            errors.append("Remote header not copied")

if errors:
    print("PREFLIGHT FAILED:")
    for message in errors:
        print(" -", message)
    sys.exit(1)
# Compile the actual generated rounder, then check every horizontal rectangle.
# This catches DMA alignment regressions without asserting a copy of its formula.
rounder = re.search(r'display_cfg\.rounder_cb = (\[\]\(lv_area_t \*area\) \{.*?\});', output, re.S)
assert rounder, "Missing generated DMA area rounder"
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / 'rounder.cpp'
    binary = Path(temp) / 'rounder'
    source.write_text('''#include <cassert>
#include <cstdint>
struct lv_area_t { int16_t x1, y1, x2, y2; };
int main() {
    auto rounder = ''' + rounder.group(1) + ';\n' + '''
    for (int left=0; left<480; ++left) {
        for (int right=left; right<480; ++right) {
            lv_area_t area={(int16_t)left, 1, (int16_t)right, 10};
            rounder(&area);
            assert(area.x1>=0 && area.x2<480);
            assert(area.x1<=left && area.x2>=right);
            assert(area.y1==1 && area.y2==10);
            for(int height=1; height<=10; ++height)
                assert(((area.x2-area.x1+1)*height*2)%4==0);
        }
    }
}
''')
    subprocess.run(['c++', '-std=c++17', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: compiled DMA rounder preserves geometry and aligns all 115440 horizontal rectangles")

print("Preflight PASS: version, C++ source patterns, JS syntax, patcher syntax and idempotence")

if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
    health=(FW/"meshcrossway_health.h").read_text()
    assert '#include "meshcrossway_health.h"' in ui
    assert not any(token in health for token in ("lv_snapshot_take(","lvgl_port_lock(","lv_indev_drv_register(","/touch"))
    if version not in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):assert "/screen.raw" not in health
    assert health.count("httpd_register_uri_handler(")==(14 if version in ("v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else 12 if version in ("v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else 8 if version in ("v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else 6 if version in ("v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else 3 if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else 2)
    js=((FW/"meshcrossway_companion_page.h").read_text() if version in ("v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else health).split("<script>",1)[1].split("</script>",1)[0]
    subprocess.run(["node","--check","-"],input=js,text=True,check=True)
    print("PASS status HTTP isolation and browser JavaScript")

if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
    backup=(FW/"meshcrossway_backup.h").read_text()
    assert all(name in backup for name in ("/sdcard/meshcrossway-wifi.json", "/sdcard/meshcrossway-wifi.tmp", "write failed (", "failed(FailureStep"))
    assert "meshbackup::describe(" in ui
    print("PASS: FATFS LFN enabled and write-stage diagnostics available without exposing secrets")

if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
    capture=(FW/"meshcrossway_capture.h").read_text()
    patch=patcher.read_text()
    assert "lv_snapshot_buf_size_needed(" in capture and "lv_snapshot_take_to_buf(" in capture
    assert "lv_timer_create(capture_timer," in capture
    assert "MALLOC_CAP_SPIRAM" in capture
    assert 'MIN_INTERVAL_US=5000000' in capture
    assert 'MAX_CAPTURE_MS=2000' in capture
    assert 'xSemaphoreTake(completion' in capture
    assert "lvgl_port_lock(" not in capture
    assert "lv_indev_drv_register(" not in capture
    assert "meshcapture::screenshot" in health
    # Patcher contains a negative guard mentioning meshremote::start(); textually.
    # The patched vendor main.cpp was already verified above to contain no remote touch/server call.
    assert 'uri="/touch"' not in health
    assert 'X-MCX-Pin' not in capture and 'meshcapture::pin()' not in ui
    assert 'id="pin"' not in health and 'X-MCX-Pin' not in health
    assert 'authorized(' not in capture and '403 Forbidden' not in capture
    assert '"sd_backup"' not in capture
    assert 'meshbackup::describe(' in ui
    assert 'CONFIG_FATFS_LFN_HEAP=y' in patch
    assert version in health
    print("PASS: isolated one-shot manual capture, bounded PSRAM, PIN-free access and SD recovery preserved")

if version in ("v0.6.0","v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
    if version in ("v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        for marker in ("tab_names[6]", ("has_context_rail" if version in ("v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "home_nav_open"), "NODES / MESHCORE",
                       "GROUPS / CHANNELS", "CHAT / NO SELECTION", "active_thread",
                       "nodes_filter", "groups_filter", "make_nav_vector",
                       "open_thread_now", "more_view", (version.lstrip("v") if version in ("v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "0.7.8" if version=="v0.7.8" else "DEV " + version.lstrip("v")) if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else ("DEV 0.6.4" if version=="v0.6.4" else ("DEV 0.6.3" if version=="v0.6.3" else "DEV 0.6.2"))):
            assert marker in ui, "Missing new navigation element: "+marker
        assert version in health
        assert 'remote touch disabled' in ui

    if version=="v0.6.1":
        for marker in ("single_label(context_actions", "lv_obj_set_size(icon,button_width,20)",
                       "lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN)", "lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE)",
                       "lv_obj_scroll_to_y(body", "DEV 0.6.1"):
            assert marker in ui, "Missing adaptive element: "+marker
        assert "DEV 0.6.0" not in ui
    if version in ("v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        ble=(FW/"meshcrossway_ble.h").read_text()
        if version in ("v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):ble+=(FW/"meshcrossway_ble_link.inc").read_text()
        assert all(token in ble for token in ("esp_nimble_init(", "esp_bt_controller_init(", "ble_gap_disc(", "ble_gap_connect(", "ble_gattc_disc_all_svcs(", "ble_sm_inject_io(", "BLE_GAP_EVENT_ENC_CHANGE"))
        if version in ("v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            assert "meshble::start();" not in output
            assert "meshble::request_start()" in ui
            assert "meshble::schedule_auto_resume();" in output
            assert ("TURN BLUETOOTH OFF" in ui if version in ("v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "ENABLE BLE / TEST" in ui)
            assert "static bool request_start()" in ble
            if version=="v0.9.5":
                assert "xTaskNotifyGive(reconnect_worker)" in ble
                assert "ulTaskNotifyTake(pdTRUE" in ble
                assert "static void init_task(void *)" not in ble
            if version=="v0.9.7":
                mtui=(FW/"meshcrossway_meshtastic_ui.h").read_text()
                mcui=(FW/"meshcrossway_meshcore_ui.h").read_text()
                settingsui=(FW/"meshcrossway_settings_ui.h").read_text()
                mainui=(FW/"meshcrossway_ui.h").read_text()
                mc_nodes_groups=mcui.split("if (page == 1 || page == 2)",1)[1].split("} else if (page == 3)",1)[0]
                assert 'action_button(body,"<"' not in mtui and 'action_button(body, "<"' not in mc_nodes_groups
                assert 'action_button(body,">"' not in mtui and 'action_button(body, ">"' not in mc_nodes_groups
                assert '"MT STATUS"' not in mtui and '"TOOLS"' not in mc_nodes_groups
                assert "node_sort_label()" in mtui and "node_sort_label()" in mcui
                assert "SORT: LAST HEARD" in mcui and "SORT: DISTANCE" in mcui
                assert "recent direct chat:" in mtui and "recent direct chat:" in mcui
                assert "unread" in mtui and "unread" in mcui
                assert "add_channel_menu" in mcui and "channel_secret" in mcui
                assert "form(32)" in mcui and "open_mt_channel" in settingsui
                assert "lv_obj_set_scroll_dir(body,LV_DIR_VER)" in mainui
            if version in ("v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
                mtui=(FW/"meshcrossway_meshtastic_ui.h").read_text()
                mcui=(FW/"meshcrossway_meshcore_ui.h").read_text()
                if version in ("v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
                    assert "lv_timer_create(node_batch_tick,25" in mtui and "unsigned budget=3" in mtui
                    assert "list_batch_channels?1:25" in mcui and "int budget=list_batch_channels?8:3" in mcui
                else:
                    assert "lv_timer_create(node_batch_tick,1" in mtui
                    assert "lv_timer_create(list_batch_tick,1" in mcui
                assert "node_batch_tick(node_batch_timer)" in mtui and "list_batch_tick(list_batch_timer)" in mcui
                assert "MeshCore unread:" in mtui and "Unread: %u direct / %u channels" in mcui
                if version=="v0.9.8":
                    assert "lv_obj_set_size(name,145,40)" in mtui and "lv_obj_set_size(name,145,40)" in mcui
                elif version=="v0.9.13":
                    assert "lv_obj_set_size(name,card_width-14,20)" in mtui and "lv_obj_set_size(name,card_width-14,20)" in mcui
                    assert "lv_label_set_long_mode(heard_label,LV_LABEL_LONG_DOT)" in mtui and "lv_label_set_long_mode(chat_label,LV_LABEL_LONG_DOT)" in mtui
                    assert "lv_label_set_long_mode(heard_label,LV_LABEL_LONG_DOT)" in mcui and "lv_label_set_long_mode(chat_label,LV_LABEL_LONG_DOT)" in mcui
                    assert "node_batch_y+=82" in mtui and "list_batch_y+=82" in mcui
                    assert "latest received" not in mtui and "last received" not in mcui
                elif version in ("v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
                    shared=(FW/"meshcrossway_ui.h").read_text()
                    mtproto=(FW/"meshcrossway_meshtastic_protocol.h").read_text()
                    assert "node_heard_age(" in shared
                    assert "Runtime-only User.role cache" in mtproto and "f.tag==7" in mtproto
                    if version in ("v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24"):
                        assert "Meshtastic repeater" in mtui and 'c.type==2?"Repeater"' in mcui
                        assert "NODE_MAX_RECENT=32" in shared and "14u*24u*60u*60u" in shared
                        assert "refresh_nodes_event" in mtui and "refresh_nodes_event" in mcui
                        assert "const int row_y=int(node_batch_rows++*64u)" in mtui
                        assert "const int row_y=(list_batch_cursor-1)*(row_height+6)" in mcui
                        node_batch=mtui.split("static void node_batch_tick",1)[1].split("static void render()",1)[0]
                        heard_helper=mcui.split("static uint32_t contact_last_heard",1)[1].split("static void toggle_node_sort",1)[0]
                        assert "for(const auto &m:c->data.messages)" not in node_batch
                        assert "s.messages" not in heard_helper
                    else:
                        assert "Meshtastic repeater" in mtui and "MeshCore repeater" in mcui
                    assert 'c.path_len==255?"Flood":"Direct"' in mcui
                    assert "ID %04X" in mtui and "ID %02X%02X" in mcui
                    assert "emoji_label_text(name,title)" not in mcui
                    assert "lv_label_set_text(name,title)" in mcui and "lv_label_set_text(name,title)" in mtui
                elif version=="v0.9.15":
                    shared=(FW/"meshcrossway_ui.h").read_text()
                    assert "static lv_obj_t *list_icon(" in shared
                    assert 'mcui::list_icon(b,"N",8,18,40,orange)' in mtui
                    assert 'mcui::list_icon(b,"N",8,18,40,orange)' in mcui
                    assert "lv_obj_set_pos(name,56,5)" in mtui and "lv_obj_set_pos(name,56,5)" in mcui
                    assert 'list_icon(b,"#",8,9,40' in mtui and 'list_icon(b,"#",8,9,40' in mcui
                    assert "ADD CHANNEL" in mcui and "Hashtag channel" in mcui and "Private channel" in mcui
                    assert "JOIN / CREATE" in mcui and "channel_choice" in mcui
                    assert "0.9.15" in shared and "0.9.15" in mtui and "0.9.15" in mcui
                elif version=="v0.9.14":
                    shared=(FW/"meshcrossway_ui.h").read_text()
                    assert "lv_obj_set_layout(view,LV_LAYOUT_FLEX)" in shared
                    assert "lv_obj_set_flex_flow(view,LV_FLEX_FLOW_COLUMN)" in shared
                    assert "lv_obj_set_style_pad_row(view,6,0)" in shared
                    assert 'action_button(node_list_view,"",0,0,node_batch_width,76' in mtui
                    assert 'action_button(list_viewport,"",0,0,list_batch_width,is_channel?48:76' in mcui
                    assert "node_batch_y" not in mtui and "list_batch_y" not in mcui
                    assert "lv_obj_set_size(name,card_width-14,20)" in mtui and "lv_obj_set_size(name,card_width-14,20)" in mcui
                    assert "latest received" not in mtui and "last received" not in mcui
                else:
                    assert "compact_list_label(title)" in mtui and "compact_list_label(title)" in mcui
                    assert "lv_obj_set_size(name,name_width,22)" in mtui and "lv_obj_set_size(name,name_width,22)" in mcui
                    assert "card_width*30/100" in mtui and "card_width*38/100" in mcui
                    assert "latest received" not in mtui and "last received" not in mcui
                if version not in ("v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
                    assert "Heard %s / %u unread\\nChat %u" in mtui and "Heard %s / %u unread\\nChat %u" in mcui
            if version=="v0.9.6":
                map_tiles=(FW/"meshcrossway_map_tiles.h").read_text()
                map_ui=(FW/"meshcrossway_navigation_ui.h").read_text()
                assert '#include "freertos/idf_additions.h"' in map_tiles
                assert "xTaskCreatePinnedToCoreWithCaps" in map_tiles
                assert "MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT" in map_tiles
                worker=map_tiles.split("static void worker(void *)",1)[1].split("static bool start()",1)[0]
                assert "nvs_commit" not in worker and "nvs_open" not in worker
                assert "mcxmap::persist_config();" in map_ui
                assert "Map worker unavailable" in map_ui
                assert "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y" in patcher.read_text()
            if version in ("v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
                assert "meshble::prepare_controller_early();" in output
                assert output.index("meshble::prepare_controller_early();") < output.index("meshnetwork::start();")
                assert "controller_prepare_failed" in ble
        else:
            assert "meshble::start();" in output
        assert '"meshcrossway_ble.h"' in patcher.read_text()
        assert "CONFIG_BT_NIMBLE_ENABLED=y" in patcher.read_text()
    if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        assert "port_cfg.task_affinity = 1;" in output
        lcd=patched_lcd
        assert "#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)" in lcd
        assert "io_config.trans_queue_depth = 6;" in lcd
        for setting in ("CONFIG_BT_CTRL_PINNED_TO_CORE_1=y","CONFIG_BT_NIMBLE_PINNED_TO_CORE_1=y",
                        "CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y"):
            assert setting in patched_sdkconfig
        assert "CONFIG_BT_NIMBLE_NVS_PERSIST=y" in patched_sdkconfig
        assert "CONFIG_BT_NIMBLE_MAX_BONDS=4" in patched_sdkconfig
        assert "state.bonded=desc.sec_state.bonded" in ble
        assert "save_remembered()" in ble and "read_remembered()" in ble
        assert "scan(true)" in ble and "auto_paused.store(true)" in ble
        assert "[MCX-MEM]" in ble
        if version in ("v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            emoji_root=FW/"emoji"
            registry=(emoji_root/"emoji_registry.inc").read_text()
            display=(emoji_root/"emoji_display.h").read_text()
            mcui=(FW/"meshcrossway_meshcore_ui.h").read_text()
            assert r'\U0001F3C1' in registry
            assert (emoji_root/"emoji_flag_asset.h").is_file()
            assert "emoji_flag_asset.h" in patcher.read_text()
            assert "kEmojiRenderedCount" in display
            assert "emoji_label_text(name,title)" not in mcui and "emoji_txt(card, sender" in mcui
            assert "emoji_center_heading(" in mcui
            assert ("home_cards(w)" if version in ("v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "mcui::set_home_name") in ui
        if version in ("v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            assert "CONFIG_LV_USE_IMGFONT=y" in patched_sdkconfig
            assert '#include "emoji/emoji_display.h"' in ui
            assert (FW / "emoji" / "emoji_display.h").is_file()
            assert '"emoji_display.h"' in patcher.read_text()
            assert ("set_message_text(text, display_text)" if version in ("v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else
                    "set_message_text(text, m->text)") in (FW/"meshcrossway_meshcore_ui.h").read_text()
        if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            wifi=(FW/"meshcrossway_network.h").read_text()
            assert all(marker in wifi for marker in ("MAX_PROFILES=5","remember_success(","known_network(","try_saved(","next_saved(","RETRY_KNOWN","WIFI_VAULT_MAGIC","60000000ULL","suppress_user_leave"))
            assert "meshble::schedule_auto_resume();" in output
            assert "matching_remembered(" in ble and "read_remembered()" in ble
            assert "auto_resume_task" in ble and "auto_paused.store(true)" in ble
        assert '#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * 10)' in output
        assert '.buff_dma = 1,' in output and '.buff_spiram = 0,' in output
        assert output.index('meshnetwork::start();') < output.index('mcxoffline::begin();') < output.index('meshble::schedule_auto_resume();')
        assert output.count('mcxoffline::begin();') == 1

        assert 'assert(lvgl_disp != nullptr);' in output
        assert 'display_cfg.rounder_cb = [](lv_area_t *area)' in output
        assert 'espressif/esp_lvgl_port: "2.9.0"' in patched_manifest
        assert 'ESP_ERROR_CHECK(color_result);' in patched_panel
        assert 'CONFIG_ESP_WIFI_TASK_CORE_ID=' not in patched_sdkconfig
    if version in ("v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        brand=(FW/"meshcrossway_brand.h").read_text()
        current=(FW/"meshcrossway_ui.h").read_text()
        assert "official_pixels[2224]" in brand
        assert "official_lockup" in brand
        assert '"meshcrossway_brand.h"' in patcher.read_text()
        assert "lv_img_set_src(brand,&mcxbrand::official_lockup)" in current
        assert "static bool has_context_rail(){return page==3||page==4;}" in current
        assert ("static bool has_left_rail(){return true;}" if version in ("v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2")
                else "static bool has_left_rail(){return page!=5;}") in current
        assert "right_compact?context_glyph(id):choices[i].label" in current
        chat=(FW/"meshcrossway_meshcore_ui.h").read_text()
        assert "static void thread_info()" in chat
        assert "if(id==A_THREAD_INFO){mcui::thread_info();return;}" in current
        assert 'action_button(body, "COMPOSE", 8, 53' not in chat
        assert 'action_button(body, "TOOLS", 16 +' not in chat
        assert "int y = 96;" in chat or "chatview::begin(body)" in chat
        assert ("if(!has_context_rail()){" if version in ("v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "if(!has_context_rail())return;") in current
        if version=="v0.7.6":assert 'action_button(body,"HOME"' in current
        else:assert 'action_button(body,"HOME"' not in current
        assert 'action_button(body,"SETTINGS"' in current
        assert "home_nav_open" not in current
        assert "nodes_actions" not in current and "groups_actions" not in current
        assert (FW/"meshcrossway_brand.h").is_file()
    if version in ("v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        swipe=(FW/"meshcrossway_edge_swipe.h").read_text()
        current=(FW/"meshcrossway_ui.h").read_text()
        assert 'meshcrossway_edge_swipe.h' in patcher.read_text()
        assert "swipe_pointer->proc.state==LV_INDEV_STATE_PRESSED" in current
        assert "lv_timer_create(edge_swipe_poll,20,nullptr)" in current
        assert "static void edge_swipe_poll" in current
        assert "left_handle" not in current and "right_handle" not in current
        assert "toggle_left" not in current and "toggle_right" not in current
        assert ("context_actions,5,0" if version in ("v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "context_actions,0,0") in current
        assert "lv_obj_set_pos(tabs[i],3,7+i*45)" in current
        assert "iabs(dx)<2*iabs(dy)" in swipe
        assert "if(overlay" in swipe
        assert "if(y<34||y>=320" in swipe
        assert (FW/"meshcrossway_edge_swipe.h").is_file()
        power=(FW/"meshcrossway_power_status.h").read_text()
        battery=(FW/"meshcrossway_battery_decode.h").read_text()
        assert "meshcrossway_power_status.h" in patcher.read_text()
        assert "meshcrossway_battery_decode.h" in patcher.read_text()
        assert "meshpower::start();" in current
        assert "meshpower::snapshot()" in current
        assert "i2c_master_transmit_receive(i2c_device" in power
        assert "i2c_master_bus_add_device(" not in power
        assert "i2c_master_transmit(i2c_device" not in power
        assert "i2c_master_transmit_receive(i2c_device" in power
        assert "vTaskDelay(pdMS_TO_TICKS(15000))" in power
        assert "LV_SYMBOL_WIFI" in current and "LV_SYMBOL_BLUETOOTH" in current
        assert "LV_SYMBOL_CHARGE" in current and ("top_battery_fill" if version in ("v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "LV_SYMBOL_BATTERY_EMPTY") in current
        assert 'char label[54];snprintf(label,sizeof(label),"Wi-Fi: %s"' not in current
        assert "static bool has_left_rail(){return true;}" in current
        assert "lv_obj_set_style_border_width(button,0,0)" in current
        assert "selected?bg:orange" in current
        assert "battery_present" in battery and "charging" in battery
        assert (FW/"meshcrossway_power_status.h").is_file()
        assert (FW/"meshcrossway_battery_decode.h").is_file()
        chat=(FW/"meshcrossway_meshcore_ui.h").read_text()
        protocol=(FW/"meshcrossway_meshcore_protocol.h").read_text()
        presenter=(FW/"meshcrossway_chat_presentation.h").read_text()
        assert 'meshcrossway_chat_presentation.h' in patcher.read_text()
        assert '#include "meshcrossway_chat_presentation.h"' in current
        assert "mcxchat::channel_sender(m->text)" in chat
        assert "mcxchat::color_index(sender)" in chat
        assert ("mcxchat::meta(mcxchat::message_stamp" if version in ("v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "mcxchat::meta(m->observed_at") in chat
        assert "lv_obj_t *name = emoji_txt(card, sender" in chat
        assert "m->text = " not in chat
        assert '"RX / signer %s / %lu"' not in chat
        assert '"%s / %s / %lu"' not in chat
        assert "uint32_t id = 0, timestamp = 0, observed_at = 0" in protocol
        assert "m.observed_at = observed" in protocol
        assert "path==0xFFu" in presenter and "path&0x3Fu" in presenter
        assert 'snprintf(out,cap,"%02lu:%02lu UTC"' in presenter
        assert (FW/"meshcrossway_chat_presentation.h").is_file()
        resize=(FW/"emoji"/"emoji_small_sprite.h").read_text()
        emoji=(FW/"emoji"/"emoji_display.h").read_text()
        parser=(FW/"emoji"/"emoji_presentation.h").read_text()
        assert '"emoji_small_sprite.h"' in patcher.read_text()
        assert (FW/"emoji"/"emoji_small_sprite.h").is_file()
        assert "kEmojiDrawSide = 14" in resize
        assert "kEmojiDrawBytes" in emoji and "lv_imgfont_create(16, image_cb)" in emoji
        assert "downsample_emoji(source,sizeof(source)" in emoji
        assert 'const char fallback[] = "$"' in parser
        assert 'memcmp(src + pos, "[emoji]", 7)' in parser
        assert 'transform(original_utf8,strlen(original_utf8)' in emoji
        assert 'n == 1 || n == 5' in protocol
        assert 'n == 1 || n == 2' in protocol
        assert 'valid = n >= 8;' in protocol
        assert 'MeshCore invalid frame 0x%02X len %u' in protocol
        assert 'protocol v%u unsupported' in protocol
        assert "see status on screen" in (FW/"meshcrossway_meshcore.h").read_text()
        assert 'lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP)' in chat
        assert 'mcxemoji::view::set_message_text(peer_name,name)' in current
        assert 'mcxemoji::view::set_message_text(lv_obj_get_child(mc_card,1),s.active_name)' in current
        print("PASS: v0.7.7 emoji sizing/fallback and bounded Companion response validation")
        print("PASS: chat sender, message-time presentation and route hops without RX IDs")
        if version in ("v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
            assert "static void left_tab_touch" in current
            assert "suppress_left_nav_click||edge_swipe_click_in_progress()" in current
            assert "lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_PRESSING,nullptr)" in current
            assert "LV_SCROLLBAR_MODE_OFF" in current
            assert f'color_label(top,"{version.lstrip("v")}",187,11' in current
            assert 'color_label(top,"DEV 0.7.8"' not in current
            assert "top_battery_fill" in current
            assert "CONFIG_LV_FONT_MONTSERRAT_10=y" in patcher.read_text()
            assert "lv_obj_set_width(top_battery_fill" in current
            assert "top_mt_name=color_label(top,\"MT\"" in current
            assert "top_mc_name=color_label(top,\"MC\"" in current
        print("PASS: v0.7.8 UI swipe gesture, adjacent map scrolling and clean battery/header")
    if version in ("v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2"):
        key_header=(FW/"meshcrossway_device_key.h").read_text()
        assert '#include "meshcrossway_device_key.h"' in (FW/"meshcrossway_ui.h").read_text()
        assert '"meshcrossway_device_key.h"' in patcher.read_text()
        assert '"meshcrossway_device_key.h"' in patcher.read_text()
        assert "mcxkey::device_audit()" in (FW/"meshcrossway_ui.h").read_text()
        assert "esp_efuse_get_key_purpose(block)" in key_header
        assert "esp_efuse_write_key(" not in key_header
        print("PASS: read-only eFuse audit connected to Diagnostics without burn")
    # Approved simulator v5: independent collapsible left/right rails and shade.
    for marker in ("left_compact","right_compact",
                   ("edge_swipe_poll" if version in ("v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "toggle_left"),
                   ("mcxswipe::sample(" if version in ("v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "toggle_right"),
                   "context_actions","open_shade","QUICK SETTINGS",
                   "A_POIS","A_TRACKS","A_RECORD","A_SHARE","A_GNSS",
                   "map_view","lv_obj_scroll_to_y(context_actions",
                   "BLE / RADIO PODS" if version in ("v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7","v0.7.8","v0.7.9","v0.7.10","v0.7.11","v0.7.12","v0.7.13","v0.7.14","v0.8.0","v0.8.1","v0.8.2","v0.8.3","v0.8.4","v0.8.5","v0.8.6","v0.8.7","v0.8.8","v0.8.9","v0.8.10","v0.8.11","v0.8.12","v0.8.13","v0.8.14","v0.8.15","v0.8.16","v0.8.17","v0.8.18","v0.8.19","v0.8.20","v0.8.21","v0.8.22","v0.8.23","v0.8.24","v0.8.25","v0.8.26","v0.9.0","v0.9.1","v0.9.2","v0.9.3","v0.9.4","v0.9.5","v0.9.6","v0.9.7","v0.9.8","v0.9.9","v0.9.10","v0.9.11","v0.9.12","v0.9.13","v0.9.14","v0.9.15","v0.9.16","v0.9.17","v0.9.18","v0.9.19","v0.9.20","v0.9.21","v0.9.22","v0.9.23","v0.9.24","v0.10.0","v0.10.1","v0.10.2") else "BLE: NOT CONNECTED","GNSS / OFFLINE","wifi_setup("):
        assert marker in ui, "v0.6.0 side-console element missing: "+marker
    assert 'DEV 0.5.10' not in ui
    assert version in ui
    assert "meshcapture::screenshot" in health
    assert "meshbackup::describe(" in ui
    assert "meshremote::register_input();" not in patcher.read_text().split("if 'meshremote::register_input();'")[0]
    print("PASS: v0.6.0 simulator-to-LVGL dual rail, navigation tools, status shade and manual remote capture")


# Build the bounded UART parser whenever the GNSS module is present.
if (FW / "meshcrossway_gnss_parser.h").exists():
    import tempfile
    with tempfile.TemporaryDirectory() as gnss_tmp:
        gnss_binary = str(Path(gnss_tmp) / "gnss-test")
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror=misleading-indentation", str(FW.parents[1] / "tests/gnss_parser_test.cpp"), "-o", gnss_binary], check=True)
        subprocess.run([gnss_binary], check=True)

if (FW / "meshcrossway_clock.h").exists():
    with tempfile.TemporaryDirectory() as clock_tmp:
        binary = str(Path(clock_tmp) / "clock-test")
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-function", "-Werror=misleading-indentation", str(ROOT / "tests/clock_test.cpp"), "-o", binary], check=True)
        subprocess.run([binary], check=True)

if (FW / "meshcrossway_map_projection.h").exists():
    with tempfile.TemporaryDirectory() as map_tmp:
        binary = str(Path(map_tmp) / "projection-test")
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(ROOT / "tests/map_projection_test.cpp"), "-o", binary], check=True)
        subprocess.run([binary], check=True)

# Local navigation persistence is exercised independently of radio protocols.
if (FW / "meshcrossway_navigation_store.h").exists():
    with tempfile.TemporaryDirectory() as nav_tmp:
        binary=str(Path(nav_tmp)/"navigation-store")
        subprocess.run(["c++","-std=c++17","-Wall","-Wextra","-Werror=misleading-indentation","-I",str(FW),str(ROOT/"tests/navigation_store_test.cpp"),"-o",binary],check=True)
        subprocess.run([binary,str(Path(nav_tmp)/"card")],check=True)

if (FW / "meshcrossway_home_status.h").exists():
    with tempfile.TemporaryDirectory() as directory:
        binary=str(Path(directory)/"home-status")
        subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-I"+str(FW),str(ROOT/"tests/home_status_test.cpp"),"-o",binary],check=True)
        subprocess.run([binary],check=True)

if (ROOT / "tests/navigation_folders_test.cpp").exists():
    with tempfile.TemporaryDirectory() as directory:
        binary=str(Path(directory)/"folders")
        subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror=misleading-indentation","-I"+str(FW),str(ROOT/"tests/navigation_folders_test.cpp"),"-o",binary],check=True)
        subprocess.run([binary,str(Path(directory)/"card")],check=True)
    js=(FW/"meshcrossway_track_map_js.h").read_text().split('R"MCXJS(',1)[1].split(')MCXJS";',1)[0]
    subprocess.run(["node","--check","-"],input=js,text=True,check=True)
    subprocess.run(["node","-e",js+";const m=MCXTrackMap,p=m.project(39.9,-104.9),q=m.unproject(p.x,p.y);if(Math.abs(q.lat-39.9)>1e-8)throw Error('Projection');const a=m.unwrap([[0,179.9,1],[0,-179.9,2]]);if(Math.abs(a[0].x-a[1].x)>.01||a[1].segment!==2)throw Error('Dateline/segments');"],check=True)
