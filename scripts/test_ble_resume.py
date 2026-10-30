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
preferences=section('static bool read_enabled(){','static void save_remembered(){')
resume=section('static void auto_resume_task(void *){','static void schedule_auto_resume(){')
test=r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
using nvs_handle_t=int;using esp_err_t=int;
static constexpr int ESP_OK=0,NVS_READONLY=0,NVS_READWRITE=1,BLE_HS_CONN_HANDLE_NONE=65535;
static bool enabled=false,pending=false,nvs_error=false,known_peer=false,deleted=false;
static int writes=0,starts=0,scans=0,waits=0;
static uint32_t elapsed=0;
static int nvs_open(const char *,int,nvs_handle_t *h){*h=1;return nvs_error?-1:0;}
static int nvs_get_u8(nvs_handle_t,const char *key,uint8_t *v){assert(!strcmp(key,"enabled"));*v=enabled;return 0;}
static int nvs_set_u8(nvs_handle_t,const char *,uint8_t v){++writes;pending=v;return 0;}
static int nvs_commit(nvs_handle_t){enabled=pending;return 0;}
static void nvs_close(nvs_handle_t){}
static bool read_remembered(){return known_peer;}
static std::atomic<bool> auto_paused{false},synced{true};
static int conn=BLE_HS_CONN_HANDLE_NONE;
static void *reconnect_worker=(void*)1;
enum Phase{IDLE,FAILED};struct Snapshot{Phase phase=IDLE;};
static Snapshot snapshot(){return {};}
static bool request_start(){++starts;return true;}
static void scan(bool automatic){assert(automatic&&known_peer&&elapsed>=15000);++scans;}
struct Stop{};
static void vTaskDelay(uint32_t n){elapsed+=n;if(++waits>2)throw Stop{};}
static void vTaskDelete(void *){deleted=true;}
#define pdMS_TO_TICKS(n) (n)
'''+preferences+resume+r'''
static void scenario(bool saved_enabled,bool paired,bool paused,int want_start,int want_scan){
  enabled=saved_enabled;known_peer=paired;auto_paused=paused;
  starts=scans=waits=elapsed=0;deleted=false;reconnect_worker=(void*)1;
  try{auto_resume_task(nullptr);}catch(Stop&){}
  assert(starts==want_start&&scans==want_scan);
  if(!want_start)assert(deleted&&reconnect_worker==nullptr);
}
int main(){
  assert(!read_enabled());remember_enabled();assert(read_enabled()&&writes==1);
  remember_enabled();assert(writes==1);
  scenario(false,false,false,0,0); // Fresh Core stays opt-in.
  scenario(true,false,false,1,0);  // Explicitly enabled, not yet bonded: BLE on.
  scenario(false,true,false,1,1);  // Upgrade from the old bonded-peer format.
  scenario(true,true,false,1,1);   // Warm restart restores the authenticated peer.
  scenario(true,true,true,0,0);    // A deliberate disconnect pauses auto-resume.
  nvs_error=true;scenario(true,false,false,0,0); // Unreadable intent is not guessed.
  puts("PASS: production BLE resume preserves enabled intent, migrates bonded peers, honors manual pause, and never auto-selects an unknown radio");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp=Path(directory)/'resume.cpp';cpp.write_text(test)
    binary=Path(directory)/'resume'
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

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
