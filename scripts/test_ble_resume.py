#!/usr/bin/env python3
"""Execute production BLE preference/resume functions against fake NVS/time."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'firmware/display-prototype/meshcrossway_ble.h').read_text()
def section(start,end):
    first=source.index(start)
    return source[first:source.index(end,first)]
# Run the actual staged SDK initializer with failure injection. No radio or eFuse writes.
startup=section('static const char *init_stage=', 'static void start_internal(){')
fixture=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
using esp_err_t=int;using esp_bt_controller_config_t=int;
static constexpr int ESP_OK=0,ESP_BT_MODE_BLE=1;
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() 0
static int fail=0,step=0,disabled=0,deinitialized=0;
static int esp_bt_controller_init(int *){assert(++step==1);return fail==1?257:0;}
static int esp_bt_controller_enable(int){assert(++step==2);return fail==2?258:0;}
static int esp_nimble_init(){assert(++step==3);return fail==3?-1:0;}
static int esp_bt_controller_disable(){++disabled;return 0;}
static int esp_bt_controller_deinit(){++deinitialized;return 0;}
'''+startup+r'''
int main(){
 for(fail=0;fail<=3;++fail){step=disabled=deinitialized=0;const int rc=initialize_stack();
  if(fail==0)assert(rc==0&&step==3&&!disabled&&!deinitialized&&!strcmp(init_stage,"host ready"));
  if(fail==1)assert(rc==257&&step==1&&!disabled&&!deinitialized&&!strcmp(init_stage,"controller init"));
  if(fail==2)assert(rc==258&&step==2&&!disabled&&deinitialized==1&&!strcmp(init_stage,"controller enable"));
  if(fail==3)assert(rc==-1&&step==3&&disabled==1&&deinitialized==1&&!strcmp(init_stage,"host / HCI"));
 }
 puts("PASS: production BLE SDK stage reporting, exact failure codes and controller cleanup; no partial-host retry");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp=Path(directory)/'startup.cpp';cpp.write_text(fixture)
    binary=Path(directory)/'startup'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


subprocess.run(["python3",str(ROOT/"scripts/test_ble_dual.py")],check=True)
