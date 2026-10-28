#!/usr/bin/env python3
"""Apply MeshCrossway UI to Waveshare's 02_lvgl_example (non-destructive copy)."""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
vendor = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root / "vendor" / "ESP32-S3-Touch-LCD-3.5"
project = vendor / "examples" / "esp-idf" / "02_lvgl_example"
main = project / "main" / "main.cpp"
if not main.is_file():
    sys.exit(f"Missing upstream example: {main}\nClone https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5.git into vendor/")
source = main.read_text()
if '#include "meshcrossway_ui.h"' not in source:
    if '#define EXAMPLE_DISPLAY_ROTATION 0' not in source or 'lv_demo_widgets();' not in source:
        sys.exit("Upstream example has changed; review main.cpp before patching.")
    source = source.replace('#define EXAMPLE_DISPLAY_ROTATION 0','#define EXAMPLE_DISPLAY_ROTATION 90',1)
    source = source.replace('#include "lvgl_ui.h"','#include "lvgl_ui.h"\n#include "meshcrossway_ui.h"',1)
    source = source.replace('lv_demo_widgets();','meshcrossway::init();',1)
    main.write_text(source)
shutil.copy2(root / "firmware" / "display-prototype" / "meshcrossway_ui.h",
             project / "main" / "meshcrossway_ui.h")
print(f"Patched: {project}")
print("Next: activate ESP-IDF, cd to that directory, run idf.py set-target esp32s3; idf.py build")
