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

ui=(FW / "meshcrossway_ui.h").read_text()
if version=="v0.5.4":
    anchor=ui.index("lv_obj_set_align(wifi_keyboard,LV_ALIGN_TOP_LEFT)")
    assert anchor<ui.index("lv_obj_set_pos(wifi_keyboard,4,185)"), "Keyboard must use top-left coordinates"
if version not in ("v0.5.1", "v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9"):
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
        if version in ("v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9"):
            if (output.count("meshnetwork::start();") != 1 or output.count("meshcrossway::init();") != 1 or "meshremote::start();" in output or "meshremote::register_input();" in output):
                errors.append("Diagnostic patcher must start only Wi-Fi; no remote HTTP or LVGL input")
            if version in ("v0.5.7","v0.5.8","v0.5.9") and output.count("meshhealth::start();")!=1:
                errors.append("Status HTTP must start once")
        elif (output.count("meshremote::start();") != 1 or output.count("meshcrossway::init();") != 1 or output.count("meshremote::register_input();") != 1):
            errors.append("Patcher not idempotent for main.cpp")
        if version=="v0.5.9" and (output.count("meshcapture::init();")!=1 or "meshremote::register_input();" in output):
            errors.append("Manual capture must initialize once without remote touch")
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

if version in ("v0.5.7","v0.5.8","v0.5.9"):
    health=(FW/"meshcrossway_health.h").read_text()
    assert '#include "meshcrossway_health.h"' in ui
    assert not any(token in health for token in ("lv_snapshot_take(","lvgl_port_lock(","lv_indev_drv_register(","/screen.raw","/touch"))
    assert health.count("httpd_register_uri_handler(")==(3 if version=="v0.5.9" else 2)
    js=health.split("<script>",1)[1].split("</script>",1)[0]
    subprocess.run(["node","--check","-"],input=js,text=True,check=True)
    print("PASS HTTP isolation and browser JavaScript")
    if version=="v0.5.9":
        capture=(FW/"meshcrossway_capture.h").read_text()
        assert "lv_snapshot_take_to_buf(" in capture
        assert "lv_timer_create(capture_timer," in capture
        assert "lvgl_port_lock(" not in capture
        assert "/touch" not in health
        assert "meshcapture::screenshot" in health
        assert "View PIN: %s" in ui
        print("PASS manual viewer isolation: LVGL timer, bounded buffer, no remote touch")
