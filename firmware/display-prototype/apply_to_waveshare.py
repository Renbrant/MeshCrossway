#!/usr/bin/env python3
"""Apply the MeshCrossway LVGL demo and Wi-Fi remote capture to Waveshare example."""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
vendor = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root / "vendor" / "ESP32-S3-Touch-LCD-3.5"
project = vendor / "examples" / "esp-idf" / "02_lvgl_example"
main = project / "main" / "main.cpp"
if not main.is_file():
    sys.exit(f"Missing Waveshare example: {main}")
source = main.read_text()
if '#include "meshcrossway_ui.h"' not in source:
    if '#define EXAMPLE_DISPLAY_ROTATION 0' not in source or 'lv_demo_widgets();' not in source:
        sys.exit("Upstream Waveshare example changed; manual review required")
    source = source.replace('#define EXAMPLE_DISPLAY_ROTATION 0', '#define EXAMPLE_DISPLAY_ROTATION 90', 1)
    source = source.replace('#include "lvgl_ui.h"', '#include "lvgl_ui.h"\n#include "meshcrossway_ui.h"\n#include "meshcrossway_remote.h"', 1)
    source = source.replace('lv_demo_widgets();','meshcrossway::init();',1)
    anchor = '        lvgl_port_unlock();\n    }\n}'
    if anchor not in source:
        sys.exit("Cannot locate LVGL initialization section")
    source = source.replace(anchor,'        lvgl_port_unlock();\n    }\n    meshremote::start();\n}',1)
    main.write_text(source)
for name in ("meshcrossway_ui.h", "meshcrossway_remote.h"):
    shutil.copy2(root / "firmware" / "display-prototype" / name, project / "main" / name)
cmake = project / "main" / "CMakeLists.txt"
config = cmake.read_text()
if "esp_http_server" not in config:
    config = config.replace('INCLUDE_DIRS "."','INCLUDE_DIRS "."')
    cmake.write_text(config)
sdk = project / "sdkconfig.defaults"
defaults = sdk.read_text()
if "CONFIG_LV_USE_SNAPSHOT=y" not in defaults:
    sdk.write_text(defaults + "\nCONFIG_LV_USE_SNAPSHOT=y\n")
print(f"MeshCrossway patched: {project}")
