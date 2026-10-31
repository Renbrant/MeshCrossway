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
#include <initializer_list>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
using nvs_handle_t=int;using esp_err_t=int;
static constexpr int ESP_OK=0,NVS_READONLY=0,NVS_READWRITE=1,BLE_HS_CONN_HANDLE_NONE=65535;
static bool enabled=false,pending=false,nvs_error=false,known_peer=false,deleted=false;
static int writes=0,starts=0,scans=0,directs=0,waits=0;
static bool &have_remembered=known_peer;static int mux=0;
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
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
static Phase phase(){return IDLE;}
static void resume_stack_diagnostic(){}
static bool request_start(){++starts;return true;}
static void scan(bool automatic){assert(automatic&&known_peer&&elapsed>=15000);++scans;}
static bool reconnect_remembered(){assert(known_peer&&elapsed>=15000);++directs;return true;}
struct Stop{};
static void vTaskDelay(uint32_t n){elapsed+=n;if(++waits>2)throw Stop{};}
static void vTaskDelete(void *){deleted=true;}
#define pdMS_TO_TICKS(n) (n)
'''+preferences+resume+r'''
static void scenario(bool saved_enabled,bool paired,bool paused,int want_start,int want_scan){
  enabled=saved_enabled;known_peer=paired;auto_paused=paused;
  starts=scans=directs=waits=elapsed=0;deleted=false;reconnect_worker=(void*)1;
  try{auto_resume_task(nullptr);}catch(Stop&){}
  assert(starts==want_start&&directs==want_scan&&scans==0);
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

# Actual direct-resume selection, including identity/base address normalization.
reconnect=section('static bool reconnect_remembered(){','static void disconnect(){')
fixture=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <atomic>
#include <initializer_list>
static constexpr int BLE_HS_CONN_HANDLE_NONE=65535;
enum Phase{OFF,IDLE,FAILED,CONNECTING,READY};enum Radio{UNKNOWN,MESHTASTIC,MESHCORE};
struct Address{uint8_t type=0,val[6]={};};struct Peer{Address addr;char name[29]={};Radio advertised=UNKNOWN;};
struct Snapshot{Phase phase=IDLE;int count=0,selected=-1;Peer peers[8];};
static Snapshot state;
struct Saved{uint8_t addr_type=0,addr[6]={},radio=0;char name[29]={};};static Saved remembered;
static bool have_remembered=false;static std::atomic<bool> auto_paused{false},synced{true};
static int conn=BLE_HS_CONN_HANDLE_NONE,mux=0,calls=0;
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
static Phase phase(){return state.phase;}
static void connect_selected(const Peer &p,int i){assert(i==0&&p.addr.val[5]==91);++calls;state.phase=CONNECTING;}
'''+reconnect+r'''
int main(){
 assert(!reconnect_remembered()&&calls==0);
 have_remembered=true;remembered.addr[5]=91;strcpy(remembered.name,"My radio");
 for(auto radio:{MESHTASTIC,MESHCORE})for(uint8_t type=0;type<4;++type){
  state.phase=IDLE;state.count=8;remembered.radio=radio;remembered.addr_type=type;
  assert(reconnect_remembered());assert(state.count==1&&state.peers[0].addr.type==(type&1));
  assert(state.peers[0].advertised==radio&&state.peers[0].addr.val[5]==91&&!strcmp(state.peers[0].name,"My radio"));
 }
 state.phase=READY;assert(!reconnect_remembered());state.phase=IDLE;
 auto_paused=true;assert(!reconnect_remembered());auto_paused=false;
 synced=false;assert(!reconnect_remembered());synced=true;
 conn=1;assert(!reconnect_remembered());
 puts("PASS: direct remembered peer selection for MC and MT, crowded lists, identity normalization and active/manual-disconnect gating");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp=Path(directory)/'direct.cpp';cpp.write_text(fixture)
    binary=Path(directory)/'direct'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

# Exercise the actual nested connection path, not a mocked connect wrapper.
# No snapshot() exists in this fixture: control paths must copy at most one peer.
helpers=section('static Phase phase(){','static const char *protocol_name(')
connections=section('static void connect_selected(', 'static void disconnect(){')
fixture=r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
enum Phase{OFF,IDLE,FAILED,CONNECTING,VERIFYING,PAIRING,READY};
enum Radio{UNKNOWN,MESHTASTIC,MESHCORE};
struct Address{uint8_t type=0,val[6]={};};
struct Peer{Address addr;char name[29]={};int8_t rssi=0;Radio advertised=UNKNOWN;};
struct Snapshot{Phase phase=IDLE;uint8_t count=0;int selected=-1;Peer peers[8];Radio radio=UNKNOWN;
 bool paired=false,bonded=false,pin_required=false,confirm_required=false;char active_name[29]={};};
static Snapshot state;
struct Saved{uint8_t addr_type=0,addr[6]={},radio=0;char name[29]={};};static Saved remembered;
static bool have_remembered=false,synced=true;
static std::atomic<bool> auto_paused{false};
static constexpr int BLE_HS_CONN_HANDLE_NONE=65535;
static int conn=BLE_HS_CONN_HANDLE_NONE,mux=0,calls=0,rc=0,lock_depth=0;
static Radio identified=UNKNOWN;
static int mc_start,mc_end,mc_rx,mc_tx,mc_cccd,mt_start,mt_end,mt_to,mt_from,mt_num,mt_num_end,mt_cccd;
static bool mc_transport_started=false;
#define portENTER_CRITICAL(p) do{(void)(p);assert(lock_depth++==0);}while(0)
#define portEXIT_CRITICAL(p) do{(void)(p);assert(--lock_depth==0);}while(0)
namespace meshmc{static void transport_down(){assert(!lock_depth);}}
static void mt_down(){assert(!lock_depth);}
static void update(Phase p,const char *){assert(!lock_depth);state.phase=p;}
static Address dialed;
static bool reorder=false;
static int ble_gap_disc_cancel(){
 assert(!lock_depth);
 if(reorder){state.peers[0].addr.val[5]=222;state.peers[1].addr.val[5]=223;}
 return 0;
}
static int own_addr=0;
static void gap_event(){}
static int ble_gap_connect(int,const Address *a,int timeout,void *,void(*)(),void *){
 assert(!lock_depth&&timeout==20000);++calls;dialed=*a;return rc;
}
'''+helpers+connections+r'''
int main(){
 for(auto radio:{MESHTASTIC,MESHCORE})for(int repeat=0;repeat<100;++repeat){
  state={};have_remembered=true;remembered.radio=radio;remembered.addr_type=repeat%4;
  remembered.addr[5]=91;strcpy(remembered.name,"Remembered");
  reorder=true;rc=repeat%2?0:5;
  assert(reconnect_remembered());
  assert(dialed.val[5]==91&&dialed.type==(repeat%4&1));
  assert(!strcmp(state.active_name,"Remembered"));
  assert(state.phase==(rc?FAILED:CONNECTING));
 }
 state={};state.count=2;state.peers[1].addr.val[5]=72;strcpy(state.peers[1].name,"Native");
 rc=0;connect(1);assert(dialed.val[5]==72&&!strcmp(state.active_name,"Native"));
 state={};state.count=2;state.peers[1].addr.val[5]=81;strcpy(state.peers[1].name,"Remote");
 Peer expected=state.peers[1];connect_peer(expected);assert(dialed.val[5]==81&&!strcmp(state.active_name,"Remote"));
 state.phase=IDLE;int before=calls;expected.addr.val[5]=201;connect_peer(expected);assert(calls==before);
 connect(-1);connect(8);assert(calls==before);
 for(auto p:{READY,VERIFYING,PAIRING,CONNECTING}){
  state.phase=p;connect_selected(expected,0);assert(calls==before);
 }
 state.phase=IDLE;auto_paused=true;assert(!reconnect_remembered()&&calls==before);
 puts("PASS: actual MC/MT reconnect chain, 200 retries, error handling, native/remote peer reorder, active-link guards, no full snapshots or GAP calls under lock");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp=Path(directory)/'connection.cpp';cpp.write_text(fixture)
    binary=Path(directory)/'connection'
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

# The target frame checker must reject the exact old failure shape.
from check_ble_resume_stack import check
old='\n'.join(f'file:1:1:void meshble::{name}()\t{size}\tstatic' for name,size in
              [('auto_resume_task',512),('reconnect_remembered',512),('connect',512),('connect_selected',576),('scan',1008),('request_start',528)])
try:
    check(old,'RESUME_STACK_BYTES=4096;')
except ValueError:
    pass
else:
    raise AssertionError('Original v0.8.12 stack regression accepted')
assert 'PASS' in check(old.replace('512','64').replace('576','96').replace('1008','80').replace('528','80'),'RESUME_STACK_BYTES=6144;')
for reports in ('', 'file:1:1:void meshble::auto_resume_task()\t64\tdynamic'):
    try:check(reports,'RESUME_STACK_BYTES=6144;')
    except ValueError:pass
    else:raise AssertionError('Missing/unbounded compiler report accepted')
print('PASS: stack checker rejects original overflow, absent reports and unbounded frames')
