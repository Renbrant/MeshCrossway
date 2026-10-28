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

if 'meshremote::register_input();' in source or 'meshremote::start();' in source:
    sys.exit('Diagnostic firmware must not activate remote LVGL input or screenshot HTTP')
if source.count('meshcapture::init();') != 1:
    sys.exit('Missing or duplicate LVGL-task-only capture initialization')
if source.count('meshhealth::start();') != 1:
    sys.exit('Missing or duplicate status-only HTTP initialization')
if source.count('meshnetwork::start();') != 1:
    sys.exit('Missing or duplicate isolated Wi-Fi initialization')
for name in ("meshcrossway_backup.h", "meshcrossway_health.h", "meshcrossway_ui.h", "meshcrossway_remote.h",
             "meshcrossway_remote_page.h", "meshcrossway_network.h"):
    shutil.copy2(root / "firmware" / "display-prototype" / name,
                 project / "main" / name)

sdk = project / "sdkconfig.defaults"
defaults = sdk.read_text()
if "CONFIG_LV_USE_SNAPSHOT=y" not in defaults:
    sdk.write_text(defaults + "\nCONFIG_LV_USE_SNAPSHOT=y\n")
print(f"MeshCrossway patched and verified: {project}")
