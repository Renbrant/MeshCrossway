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
if version not in ("v0.5.1", "v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
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
void app(){
    if (lvgl_port_lock(0)) {
        lv_demo_widgets();
        lvgl_port_unlock();
    }
}
''')
        (main.parent / "CMakeLists.txt").write_text('idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")\n')
        (example / "sdkconfig.defaults").write_text('CONFIG_IDF_TARGET="esp32s3"\n')
        for attempt in range(2):
            p = subprocess.run([sys.executable, str(patcher), str(vendor)], text=True, capture_output=True)
            if p.returncode:
                errors.append("Patcher failed: " + p.stdout + p.stderr)
                break
        output = main.read_text()
        if version in ("v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
            if (output.count("meshnetwork::start();") != 1 or output.count("meshcrossway::init();") != 1 or "meshremote::start();" in output or "meshremote::register_input();" in output):
                errors.append("Diagnostic patcher must start only Wi-Fi; no remote HTTP or LVGL input")
            if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3") and output.count("meshhealth::start();")!=1:
                errors.append("Status HTTP must start once")
        elif (output.count("meshremote::start();") != 1 or output.count("meshcrossway::init();") != 1 or output.count("meshremote::register_input();") != 1):
            errors.append("Patcher not idempotent for main.cpp")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
            if output.count("meshcapture::init();")!=1 or '#include "meshcrossway_capture.h"' not in output:
                errors.append("v0.5.10 must initialize one capture timer within LVGL lock")
        if '#define EXAMPLE_DISPLAY_ROTATION 90' not in output:
            errors.append("Display rotation missing")
        if 'CONFIG_LV_USE_SNAPSHOT=y' not in (example / "sdkconfig.defaults").read_text():
            errors.append("Missing snapshot config")
        if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
            patched_config=(example / "sdkconfig.defaults").read_text()
            if "CONFIG_FATFS_LFN_HEAP=y" not in patched_config or "CONFIG_FATFS_LFN_NONE=y" in patched_config:
                errors.append("Wi-Fi SD backup requires FATFS heap long filename support")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3") and not (main.parent / "meshcrossway_capture.h").is_file():
            errors.append("Manual remote capture header not copied into vendor example")
        if not (main.parent / "meshcrossway_remote.h").is_file():
            errors.append("Remote header not copied")

if errors:
    print("PREFLIGHT FAILED:")
    for message in errors:
        print(" -", message)
    sys.exit(1)
print("Preflight PASS: version, C++ source patterns, JS syntax, patcher syntax and idempotence")

if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
    health=(FW/"meshcrossway_health.h").read_text()
    assert '#include "meshcrossway_health.h"' in ui
    assert not any(token in health for token in ("lv_snapshot_take(","lvgl_port_lock(","lv_indev_drv_register(","/touch"))
    if version not in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):assert "/screen.raw" not in health
    assert health.count("httpd_register_uri_handler(")==(3 if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3") else 2)
    js=health.split("<script>",1)[1].split("</script>",1)[0]
    subprocess.run(["node","--check","-"],input=js,text=True,check=True)
    print("PASS status HTTP isolation and browser JavaScript")

if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
    backup=(FW/"meshcrossway_backup.h").read_text()
    assert all(name in backup for name in ("/sdcard/meshcrossway-wifi.json", "/sdcard/meshcrossway-wifi.tmp", "write failed (", "failed(FailureStep"))
    assert "meshbackup::describe(" in ui
    print("PASS: FATFS LFN enabled and write-stage diagnostics available without exposing secrets")

if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3"):
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
    assert 'X-MCX-Pin' in capture and 'VIEW PIN: %s' in ui
    assert '"sd_backup"' not in capture
    assert 'meshbackup::describe(' in ui
    assert 'CONFIG_FATFS_LFN_HEAP=y' in patch
    assert ('v0.6.3' if version=='v0.6.3' else ('v0.6.2' if version=='v0.6.2' else ('v0.6.1' if version=='v0.6.1' else ('v0.6.0' if version=='v0.6.0' else 'v0.5.10')))) in health
    print("PASS: isolated one-shot manual capture, bounded PSRAM, serial-safe PIN and SD recovery preserved")

if version in ("v0.6.0","v0.6.1","v0.6.2","v0.6.3"):
    if version in ("v0.6.2","v0.6.3"):
        for marker in ("tab_names[6]", "home_nav_open", "NODES / OFFLINE",
                       "GROUPS / CHANNELS", "CHAT / NO SELECTION", "active_thread",
                       "nodes_filter", "groups_filter", "make_nav_vector",
                       "open_thread_now", "more_view", "DEV 0.6.3" if version=="v0.6.3" else "DEV 0.6.2"):
            assert marker in ui, "Missing new navigation element: "+marker
        assert version in health
        assert 'remote touch disabled' in ui

    if version=="v0.6.1":
        for marker in ("single_label(context_actions", "lv_obj_set_size(icon,button_width,20)",
                       "lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN)", "lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE)",
                       "lv_obj_scroll_to_y(body", "DEV 0.6.1"):
            assert marker in ui, "Missing adaptive element: "+marker
        assert "DEV 0.6.0" not in ui
    if version=="v0.6.3":
        ble=(FW/"meshcrossway_ble.h").read_text()
        assert all(token in ble for token in ("nimble_port_init(", "ble_gap_disc(", "ble_gap_connect(", "ble_gattc_disc_all_svcs(", "ble_sm_inject_io(", "BLE_GAP_EVENT_ENC_CHANGE"))
        assert "meshble::start();" in output
        assert (main.parent/"meshcrossway_ble.h").is_file()
        assert "CONFIG_BT_NIMBLE_ENABLED=y" in (example/"sdkconfig.defaults").read_text()
    # Approved simulator v5: independent collapsible left/right rails and shade.
    for marker in ("left_compact","right_compact","toggle_left","toggle_right",
                   "context_actions","open_shade","QUICK SETTINGS",
                   "A_POIS","A_TRACKS","A_RECORD","A_SHARE","A_GNSS",
                   "map_view","lv_obj_scroll_to_y(context_actions",
                   "BLE: NOT CONNECTED","GNSS / OFFLINE","wifi_setup("):
        assert marker in ui, "v0.6.0 side-console element missing: "+marker
    assert 'DEV 0.5.10' not in ui
    assert version in ui
    assert "meshcapture::screenshot" in health
    assert "meshbackup::describe(" in ui
    assert "meshremote::register_input();" not in patcher.read_text().split("if 'meshremote::register_input();'")[0]
    print("PASS: v0.6.0 simulator-to-LVGL dual rail, navigation tools, status shade and protected remote capture")
