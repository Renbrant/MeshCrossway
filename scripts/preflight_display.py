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
if version not in ("v0.5.1", "v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
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
        (example / "sdkconfig.defaults").write_text('CONFIG_IDF_TARGET="esp32s3"\n')
        for attempt in range(2):
            p = subprocess.run([sys.executable, str(patcher), str(vendor)], text=True, capture_output=True)
            if p.returncode:
                errors.append("Patcher failed: " + p.stdout + p.stderr)
                break
        output = main.read_text()
        patched_lcd = lcd_fixture.read_text()
        patched_panel = panel_fixture.read_text()
        patched_manifest = (main.parent / 'idf_component.yml').read_text()
        patched_sdkconfig = (example / "sdkconfig.defaults").read_text()
        if version in ("v0.5.2", "v0.5.3", "v0.5.4", "v0.5.5", "v0.5.6", "v0.5.7", "v0.5.8", "v0.5.9", "v0.5.10", "v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            if (output.count("meshnetwork::start();") != 1 or output.count("meshcrossway::init();") != 1 or "meshremote::start();" in output or "meshremote::register_input();" in output):
                errors.append("Diagnostic patcher must start only Wi-Fi; no remote HTTP or LVGL input")
            if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7") and output.count("meshhealth::start();")!=1:
                errors.append("Status HTTP must start once")
        elif (output.count("meshremote::start();") != 1 or output.count("meshcrossway::init();") != 1 or output.count("meshremote::register_input();") != 1):
            errors.append("Patcher not idempotent for main.cpp")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            if output.count("meshcapture::init();")!=1 or '#include "meshcrossway_capture.h"' not in output:
                errors.append("v0.5.10 must initialize one capture timer within LVGL lock")
        if '#define EXAMPLE_DISPLAY_ROTATION 90' not in output:
            errors.append("Display rotation missing")
        if 'CONFIG_LV_USE_SNAPSHOT=y' not in (example / "sdkconfig.defaults").read_text():
            errors.append("Missing snapshot config")
        if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            patched_config=(example / "sdkconfig.defaults").read_text()
            if "CONFIG_FATFS_LFN_HEAP=y" not in patched_config or "CONFIG_FATFS_LFN_NONE=y" in patched_config:
                errors.append("Wi-Fi SD backup requires FATFS heap long filename support")
        if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7") and not (main.parent / "meshcrossway_capture.h").is_file():
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

if version in ("v0.5.7","v0.5.8","v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
    health=(FW/"meshcrossway_health.h").read_text()
    assert '#include "meshcrossway_health.h"' in ui
    assert not any(token in health for token in ("lv_snapshot_take(","lvgl_port_lock(","lv_indev_drv_register(","/touch"))
    if version not in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):assert "/screen.raw" not in health
    assert health.count("httpd_register_uri_handler(")==(3 if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7") else 2)
    js=health.split("<script>",1)[1].split("</script>",1)[0]
    subprocess.run(["node","--check","-"],input=js,text=True,check=True)
    print("PASS status HTTP isolation and browser JavaScript")

if version in ("v0.5.9","v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
    backup=(FW/"meshcrossway_backup.h").read_text()
    assert all(name in backup for name in ("/sdcard/meshcrossway-wifi.json", "/sdcard/meshcrossway-wifi.tmp", "write failed (", "failed(FailureStep"))
    assert "meshbackup::describe(" in ui
    print("PASS: FATFS LFN enabled and write-stage diagnostics available without exposing secrets")

if version in ("v0.5.10","v0.6.0", "v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
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

if version in ("v0.6.0","v0.6.1","v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
    if version in ("v0.6.2","v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
        for marker in ("tab_names[6]", ("has_context_rail" if version in ("v0.7.6","v0.7.7") else "home_nav_open"), "NODES / MESHCORE",
                       "GROUPS / CHANNELS", "CHAT / NO SELECTION", "active_thread",
                       "nodes_filter", "groups_filter", "make_nav_vector",
                       "open_thread_now", "more_view", "DEV " + version.lstrip("v") if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7") else ("DEV 0.6.4" if version=="v0.6.4" else ("DEV 0.6.3" if version=="v0.6.3" else "DEV 0.6.2"))):
            assert marker in ui, "Missing new navigation element: "+marker
        assert version in health
        assert 'remote touch disabled' in ui

    if version=="v0.6.1":
        for marker in ("single_label(context_actions", "lv_obj_set_size(icon,button_width,20)",
                       "lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN)", "lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE)",
                       "lv_obj_scroll_to_y(body", "DEV 0.6.1"):
            assert marker in ui, "Missing adaptive element: "+marker
        assert "DEV 0.6.0" not in ui
    if version in ("v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
        ble=(FW/"meshcrossway_ble.h").read_text()
        assert all(token in ble for token in ("nimble_port_init(", "ble_gap_disc(", "ble_gap_connect(", "ble_gattc_disc_all_svcs(", "ble_sm_inject_io(", "BLE_GAP_EVENT_ENC_CHANGE"))
        if version in ("v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            assert "meshble::start();" not in output
            assert "meshble::request_start()" in ui
            assert "meshble::schedule_auto_resume();" in output
            assert "ENABLE BLE / TEST" in ui
            assert "static bool request_start()" in ble
        else:
            assert "meshble::start();" in output
        assert '"meshcrossway_ble.h"' in patcher.read_text()
        assert "CONFIG_BT_NIMBLE_ENABLED=y" in patcher.read_text()
    if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
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
        if version in ("v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            emoji_root=FW/"emoji"
            registry=(emoji_root/"emoji_registry.inc").read_text()
            display=(emoji_root/"emoji_display.h").read_text()
            mcui=(FW/"meshcrossway_meshcore_ui.h").read_text()
            assert r'\U0001F3C1' in registry
            assert (emoji_root/"emoji_flag_asset.h").is_file()
            assert "emoji_flag_asset.h" in patcher.read_text()
            assert "kEmojiRenderedCount" in display
            assert "emoji_txt(b, title" in mcui
            assert "emoji_center_heading(" in mcui
            assert "mcui::set_home_name" in ui
        if version in ("v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            assert "CONFIG_LV_USE_IMGFONT=y" in patched_sdkconfig
            assert '#include "emoji/emoji_display.h"' in ui
            assert (FW / "emoji" / "emoji_display.h").is_file()
            assert '"emoji_display.h"' in patcher.read_text()
            assert "set_message_text(text, m->text)" in (FW/"meshcrossway_meshcore_ui.h").read_text()
        if version in ("v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7"):
            wifi=(FW/"meshcrossway_network.h").read_text()
            assert all(marker in wifi for marker in ("MAX_PROFILES=5","remember_success(","known_network(","try_saved(","next_saved(","RETRY_KNOWN","WIFI_VAULT_MAGIC","60000000ULL","suppress_user_leave"))
            assert "meshble::schedule_auto_resume();" in output
            assert "matching_remembered(" in ble and "read_remembered()" in ble
            assert "auto_resume_task" in ble and "auto_paused.store(true)" in ble
        assert '#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * 10)' in output
        assert '.buff_dma = 1,' in output and '.buff_spiram = 0,' in output
        assert 'assert(lvgl_disp != nullptr);' in output
        assert 'display_cfg.rounder_cb = [](lv_area_t *area)' in output
        assert 'espressif/esp_lvgl_port: "2.9.0"' in patched_manifest
        assert 'ESP_ERROR_CHECK(color_result);' in patched_panel
        assert 'CONFIG_ESP_WIFI_TASK_CORE_ID=' not in patched_sdkconfig
    if version in ("v0.7.6","v0.7.7"):
        brand=(FW/"meshcrossway_brand.h").read_text()
        current=(FW/"meshcrossway_ui.h").read_text()
        assert "official_pixels[2224]" in brand
        assert "official_lockup" in brand
        assert '"meshcrossway_brand.h"' in patcher.read_text()
        assert "lv_img_set_src(brand,&mcxbrand::official_lockup)" in current
        assert "static bool has_context_rail(){return page==3||page==4;}" in current
        assert ("static bool has_left_rail(){return true;}" if version=="v0.7.7"
                else "static bool has_left_rail(){return page!=5;}") in current
        assert "right_compact?context_glyph(id):choices[i].label" in current
        chat=(FW/"meshcrossway_meshcore_ui.h").read_text()
        assert "static void thread_info()" in chat
        assert "if(id==A_THREAD_INFO){mcui::thread_info();return;}" in current
        assert 'action_button(body, "COMPOSE", 8, 53' not in chat
        assert 'action_button(body, "TOOLS", 16 +' not in chat
        assert "int y = 57;" in chat
        assert "if(!has_context_rail())return;" in current
        if version=="v0.7.6":assert 'action_button(body,"HOME"' in current
        else:assert 'action_button(body,"HOME"' not in current
        assert 'action_button(body,"SETTINGS"' in current
        assert "home_nav_open" not in current
        assert "nodes_actions" not in current and "groups_actions" not in current
        assert (FW/"meshcrossway_brand.h").is_file()
    if version=="v0.7.7":
        swipe=(FW/"meshcrossway_edge_swipe.h").read_text()
        current=(FW/"meshcrossway_ui.h").read_text()
        assert 'meshcrossway_edge_swipe.h' in patcher.read_text()
        assert "swipe_pointer->proc.state==LV_INDEV_STATE_PRESSED" in current
        assert "lv_timer_create(edge_swipe_poll,20,nullptr)" in current
        assert "static void edge_swipe_poll" in current
        assert "left_handle" not in current and "right_handle" not in current
        assert "toggle_left" not in current and "toggle_right" not in current
        assert "context_actions,0,0" in current
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
        assert "LV_SYMBOL_CHARGE" in current and "LV_SYMBOL_BATTERY_EMPTY" in current
        assert 'char label[54];snprintf(label,sizeof(label),"Wi-Fi: %s"' not in current
        assert "static bool has_left_rail(){return true;}" in current
        assert "lv_obj_set_style_border_color(button,orange,0)" in current
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
        assert "mcxchat::meta(m->observed_at" in chat
        assert "lv_obj_t *name = emoji_txt(card, sender" in chat
        assert "m->text = " not in chat
        assert '"RX / signer %s / %lu"' not in chat
        assert '"%s / %s / %lu"' not in chat
        assert "uint32_t id = 0, timestamp = 0, observed_at = 0" in protocol
        assert "m.observed_at = observed" in protocol
        assert "path==0xFFu" in presenter and "path&0x3Fu" in presenter
        assert 'snprintf(out,cap,"%02lu:%02lu UTC"' in presenter
        assert (FW/"meshcrossway_chat_presentation.h").is_file()
        print("PASS: v0.7.7 chat sender, UTC observed time and route hops without RX IDs")
        print("PASS: v0.7.7 gesture, permanent Settings navigation, real status icons and read-only AXP2101")
    if version in ("v0.7.5","v0.7.6","v0.7.7"):
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
                   ("edge_swipe_poll" if version=="v0.7.7" else "toggle_left"),
                   ("mcxswipe::sample(" if version=="v0.7.7" else "toggle_right"),
                   "context_actions","open_shade","QUICK SETTINGS",
                   "A_POIS","A_TRACKS","A_RECORD","A_SHARE","A_GNSS",
                   "map_view","lv_obj_scroll_to_y(context_actions",
                   "BLE / RADIO PODS" if version in ("v0.6.3","v0.6.4","v0.7.2","v0.7.3","v0.7.4","v0.7.5","v0.7.6","v0.7.7") else "BLE: NOT CONNECTED","GNSS / OFFLINE","wifi_setup("):
        assert marker in ui, "v0.6.0 side-console element missing: "+marker
    assert 'DEV 0.5.10' not in ui
    assert version in ui
    assert "meshcapture::screenshot" in health
    assert "meshbackup::describe(" in ui
    assert "meshremote::register_input();" not in patcher.read_text().split("if 'meshremote::register_input();'")[0]
    print("PASS: v0.6.0 simulator-to-LVGL dual rail, navigation tools, status shade and manual remote capture")
