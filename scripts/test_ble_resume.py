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
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
using esp_err_t=int;using esp_bt_controller_config_t=int;
static constexpr int ESP_OK=0,ESP_ERR_NO_MEM=-12,ESP_ERR_INVALID_STATE=-1,ESP_BT_MODE_BLE=1;
static constexpr int ESP_BT_CONTROLLER_STATUS_IDLE=0;
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() 0
static int fail=0,step=0,disabled=0,deinitialized=0,controller_status=0;
static size_t heap_caps_get_largest_free_block(int){return 64*1024;}
static int esp_bt_controller_get_status(){return controller_status;}
static const char *esp_err_to_name(int){return "mock-error";}
static int esp_bt_controller_init(int *){assert(++step==1);if(fail==1)return 257;controller_status=1;return 0;}
static int esp_bt_controller_enable(int){assert(++step==2);if(fail==2)return 258;controller_status=2;return 0;}
static int esp_nimble_init(){assert(++step==3);return fail==3?-1:0;}
static int esp_bt_controller_disable(){++disabled;controller_status=1;return 0;}
static int esp_bt_controller_deinit(){++deinitialized;controller_status=0;return 0;}
'''
int main(){
 for(fail=0;fail<=3;++fail){
  step=disabled=deinitialized=controller_status=0;
  controller_ready.store(false);controller_prepare_failed.store(false);controller_prepare_error.store(0);
  prepare_controller_early();
  const int rc=initialize_stack();
  if(fail==0)assert(rc==0&&step==3&&controller_ready.load()&&!disabled&&!deinitialized&&!strcmp(init_stage,"host ready"));
  if(fail==1)assert(rc==257&&step==1&&controller_prepare_failed.load()&&!disabled&&!deinitialized&&!strcmp(init_stage,"early controller init"));
  if(fail==2)assert(rc==258&&step==2&&controller_prepare_failed.load()&&!disabled&&deinitialized==1&&!strcmp(init_stage,"early controller enable"));
  if(fail==3)assert(rc==-1&&step==3&&controller_prepare_failed.load()&&disabled==1&&deinitialized==1&&!strcmp(init_stage,"host / HCI"));
 }
 puts("PASS: early BLE controller memory gate, staged init errors and safe cleanup");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp=Path(directory)/'startup.cpp';cpp.write_text(fixture)
    binary=Path(directory)/'startup'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


subprocess.run(["python3",str(ROOT/"scripts/test_ble_dual.py")],check=True)
