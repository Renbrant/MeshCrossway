#!/usr/bin/env python3
"""Offline safety checks before the ESP-IDF compile/deploy stages."""
from pathlib import Path
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

if version not in ("v0.5.1", "v0.5.2", "v0.5.3"):
    errors.append("Unexpected version: " + version)
if "HTTPD_503_SERVICE_UNAVAILABLE" in remote:
    errors.append("Unsupported HTTPD_503_SERVICE_UNAVAILABLE token")
if "\\n else" in remote:
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
void app(){
    if (lvgl_port_lock(0)) {
        lv_demo_widgets();
        lvgl_port_unlock();
    }
}
''')
        (example / "sdkconfig.defaults").write_text('CONFIG_IDF_TARGET="esp32s3"\n')
        for attempt in range(2):
            p = subprocess.run([sys.executable, str(patcher), str(vendor)], text=True, capture_output=True)
            if p.returncode:
                errors.append("Patcher failed: " + p.stdout + p.stderr)
                break
        output = main.read_text()
        if version in ("v0.5.2", "v0.5.3"):
            if (output.count("meshnetwork::start();") != 1 or output.count("meshcrossway::init();") != 1 or "meshremote::start();" in output or "meshremote::register_input();" in output):
                errors.append("Diagnostic patcher must start only Wi-Fi; no remote HTTP or LVGL input")
        elif (output.count("meshremote::start();") != 1 or output.count("meshcrossway::init();") != 1 or output.count("meshremote::register_input();") != 1):
            errors.append("Patcher not idempotent for main.cpp")
        if '#define EXAMPLE_DISPLAY_ROTATION 90' not in output:
            errors.append("Display rotation missing")
        if 'CONFIG_LV_USE_SNAPSHOT=y' not in (example / "sdkconfig.defaults").read_text():
            errors.append("Missing snapshot config")
        if not (main.parent / "meshcrossway_remote.h").is_file():
            errors.append("Remote header not copied")

if errors:
    print("PREFLIGHT FAILED:")
    for message in errors:
        print(" -", message)
    sys.exit(1)
print("Preflight PASS: version, C++ source patterns, JS syntax, patcher syntax and idempotence")
