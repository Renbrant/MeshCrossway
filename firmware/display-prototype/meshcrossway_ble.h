#pragma once
// MeshCrossway v0.7.2 - resume only last bonded BLE peer after safe boot.
// PIN-authenticated transports: MeshCore Companion and Meshtastic PhoneAPI.
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
// NimBLE os/os.h exports C min/max macros; remove them before C++ headers.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "meshcrossway_meshcore.h"
#include "meshcrossway_meshtastic.h"
#include "os/os_mbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" void ble_store_config_init(void);

namespace meshble {
enum Phase { OFF=0, STARTING, IDLE, SCANNING, CONNECTING, VERIFYING, PAIRING, READY, FAILED };
enum Radio { UNKNOWN=0, MESHTASTIC=1, MESHCORE=2 };
struct Peer { ble_addr_t addr; char name[29]; int8_t rssi; Radio advertised; };
struct Snapshot {
  Phase phase;
  Radio radio;
  Peer peers[8];
  uint8_t count;
  int selected;
  bool pin_required;
  bool confirm_required;
  uint32_t compare_number;
  bool paired;
  bool bonded;
  uint32_t generation;
  char status[64];
  char active_name[29];
};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Snapshot state={};
static std::atomic<uint16_t> conn{BLE_HS_CONN_HANDLE_NONE};
static uint8_t own_addr=0;
static std::atomic<bool> synced{false};
static Radio identified=UNKNOWN;
static constexpr uint32_t LAST_PEER_MAGIC=0x4D435842u;
struct RememberedPeer {uint32_t magic;uint8_t version;uint8_t radio;uint8_t addr_type;uint8_t addr[6];char name[29];};
static RememberedPeer remembered={};
static bool have_remembered=false;
static std::atomic<bool> auto_paused{false};
static std::atomic<bool> auto_scan{false};
static std::atomic<int> auto_candidate{-1};
static TaskHandle_t reconnect_worker=nullptr;
static const ble_uuid128_t MT_SERVICE=BLE_UUID128_INIT(
  0xfd,0xea,0x73,0xe2,0xca,0x5d,0xa8,0x9f,0x1f,0x46,0xa8,0x15,0x18,0xb2,0xa1,0x6b);
static const ble_uuid128_t MC_SERVICE=BLE_UUID128_INIT(
  0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);

static uint16_t mc_start=0,mc_end=0,mc_rx=0,mc_tx=0,mc_tx_end=0,mc_cccd=0;
static uint16_t mt_start=0,mt_end=0,mt_to=0,mt_from=0,mt_num=0,mt_num_end=0,mt_cccd=0;
static std::atomic<bool> mt_transport_started{false},mt_io_busy{false},mt_need_read{true};
static std::atomic<uint32_t> mt_epoch{0},mt_notifications{0};
static uint32_t mt_read_notice=0,mt_last_read=0;
static const ble_uuid128_t MT_TO=BLE_UUID128_INIT(
  0xe7,0x01,0x44,0x12,0x66,0x78,0xdd,0xa1,0xad,0x4d,0x9e,0x12,0xd2,0x76,0x5c,0xf7);
static const ble_uuid128_t MT_FROM=BLE_UUID128_INIT(
  0x02,0x00,0x12,0xac,0x42,0x02,0x78,0xb8,0xed,0x11,0x93,0x49,0x9e,0xe6,0x55,0x2c);
static const ble_uuid128_t MT_NUM=BLE_UUID128_INIT(
  0x53,0x44,0xe3,0x47,0x75,0xaa,0x70,0xa6,0x66,0x4f,0x00,0xa8,0x8c,0xa1,0x9d,0xed);
static void mt_down(){
  mt_transport_started=false;mt_io_busy=false;mt_epoch.fetch_add(1);meshmt::transport_down();
}
static std::atomic<bool> mc_transport_started{false};
static const ble_uuid128_t MC_RX=BLE_UUID128_INIT(
  0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x02,0x00,0x40,0x6e);
static const ble_uuid128_t MC_TX=BLE_UUID128_INIT(
  0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x03,0x00,0x40,0x6e);
static void update(Phase phase,const char *message){
  portENTER_CRITICAL(&mux);
  state.phase=phase;
  snprintf(state.status,sizeof(state.status),"%s",message);
  state.generation++;
  portEXIT_CRITICAL(&mux);
}
static Snapshot snapshot(){
  Snapshot copy;
  portENTER_CRITICAL(&mux);copy=state;portEXIT_CRITICAL(&mux);
  return copy;
}
static const char *protocol_name(Radio r){
  return r==MESHTASTIC?"Meshtastic":r==MESHCORE?"MeshCore":"Unknown";
}
static bool read_remembered(){
  nvs_handle_t h;
  if(nvs_open("mcx_ble",NVS_READONLY,&h)!=ESP_OK)return false;
  RememberedPeer p={};size_t length=sizeof(p);
  const esp_err_t rc=nvs_get_blob(h,"last_peer",&p,&length);
  nvs_close(h);
  if(rc!=ESP_OK||length!=sizeof(p)||p.magic!=LAST_PEER_MAGIC||
     p.version!=1||(p.radio!=MESHTASTIC&&p.radio!=MESHCORE)||
     p.addr_type>3||!memchr(p.name,0,sizeof(p.name)))return false;
  portENTER_CRITICAL(&mux);remembered=p;have_remembered=true;portEXIT_CRITICAL(&mux);
  return true;
}
static bool read_enabled(){
  nvs_handle_t h;if(nvs_open("mcx_ble",NVS_READONLY,&h)!=ESP_OK)return false;
  uint8_t enabled=0;const esp_err_t rc=nvs_get_u8(h,"enabled",&enabled);nvs_close(h);
  return rc==ESP_OK&&enabled==1;
}
static void remember_enabled(){
  // Runs in the initialization worker, never in an LVGL event. Store intent
  // independently of bonding: an enabled, unpaired radio must survive a restart.
  if(read_enabled())return;
  nvs_handle_t h;if(nvs_open("mcx_ble",NVS_READWRITE,&h)!=ESP_OK)return;
  esp_err_t rc=nvs_set_u8(h,"enabled",1);if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);
  if(rc!=ESP_OK)printf("[MCX-BLE] enabled preference not saved err=%d\n",(int)rc);
}
static void save_remembered(){
  const uint16_t handle=conn;
  if(handle==BLE_HS_CONN_HANDLE_NONE||identified==UNKNOWN)return;
  struct ble_gap_conn_desc desc={};
  if(ble_gap_conn_find(handle,&desc)!=0||
     !desc.sec_state.encrypted||!desc.sec_state.authenticated||!desc.sec_state.bonded)return;
  RememberedPeer p={};
  p.magic=LAST_PEER_MAGIC;p.version=1;p.radio=(uint8_t)identified;
  p.addr_type=desc.peer_id_addr.type;
  if(p.addr_type>3)return;
  memcpy(p.addr,desc.peer_id_addr.val,6);
  const Snapshot current=snapshot();
  snprintf(p.name,sizeof(p.name),"%s",current.active_name);
  bool unchanged=false;
  portENTER_CRITICAL(&mux);
  unchanged=have_remembered&&!memcmp(&remembered,&p,sizeof(p));
  portEXIT_CRITICAL(&mux);
  if(!unchanged){
    nvs_handle_t h;
    if(nvs_open("mcx_ble",NVS_READWRITE,&h)!=ESP_OK)return;
    const bool ok=nvs_set_blob(h,"last_peer",&p,sizeof(p))==ESP_OK&&nvs_commit(h)==ESP_OK;
    nvs_close(h);
    if(!ok)return;
    portENTER_CRITICAL(&mux);remembered=p;have_remembered=true;portEXIT_CRITICAL(&mux);
  }
  auto_paused.store(false);
}
static bool matching_remembered(const ble_addr_t &address){
  bool match=false;
  portENTER_CRITICAL(&mux);
  // NimBLE can report an identity type (PUBLIC_ID/RANDOM_ID) while
  // advertisement descriptors use the equivalent PUBLIC/RANDOM base type.
  match=have_remembered&&((remembered.addr_type&1)==(address.type&1))&&
        memcmp(remembered.addr,address.val,6)==0;
  portEXIT_CRITICAL(&mux);
  // Never auto-select merely from an advertised name.
  return match;
}
static int gap_event(struct ble_gap_event *event,void *arg);
static void debug_memory(const char *phase);
static void on_sync(){
  mcxhistory::ble_startup.store(false);
  const int rc=ble_hs_util_ensure_addr(0);
  if(rc!=0){update(FAILED,"BLE identity unavailable");return;}
  if(ble_hs_id_infer_auto(0,&own_addr)!=0){update(FAILED,"BLE address unavailable");return;}
  debug_memory("host synced");
  synced=true;
  update(IDLE,"Ready to scan (9 seconds)");
}
static void on_reset(int reason){
  mt_down();
  synced=false;conn=BLE_HS_CONN_HANDLE_NONE;mc_transport_started=false;meshmc::transport_down();
  char msg[64];snprintf(msg,sizeof(msg),"BLE controller reset: %d",reason);
  update(FAILED,msg);
}
static void host_task(void *){
  nimble_port_run();
  nimble_port_freertos_deinit();
}
static void scan(bool automatic=false){
  if(!synced){
    if(snapshot().phase==OFF)update(OFF,"Enable BLE before scanning");
    else if(snapshot().phase==STARTING)update(STARTING,"Bluetooth is still starting");
    else if(snapshot().phase!=FAILED)update(FAILED,"BLE host not ready"); // Keep the original init error visible.
    return;
  }
  const Snapshot current=snapshot();
  if(current.phase==CONNECTING||current.phase==VERIFYING||current.phase==PAIRING||current.phase==READY)return;
  (void)ble_gap_disc_cancel();
  auto_scan=automatic;auto_candidate=-1;
  portENTER_CRITICAL(&mux);
  state.count=0;state.selected=-1;state.radio=UNKNOWN;
  state.pin_required=false;state.confirm_required=false;
  state.generation++;
  portEXIT_CRITICAL(&mux);
  struct ble_gap_disc_params params={};
  params.passive=0;
  params.filter_duplicates=0; // Accept scan responses supplying the name.
  params.itvl=0;params.window=0;
  const int rc=ble_gap_disc(own_addr,9000,&params,gap_event,nullptr);
  if(rc==0)update(SCANNING,automatic?"Finding last paired radio...":"Searching nearby BLE radios...");
  else {auto_scan=false;char msg[64];snprintf(msg,sizeof(msg),"Scan error: %d",rc);update(FAILED,msg);}
}
static void connect(int index){
  const Snapshot found=snapshot();
  if(index<0||index>=found.count)return;
  if(found.phase==READY||found.phase==PAIRING||found.phase==VERIFYING||found.phase==CONNECTING)return;
  (void)ble_gap_disc_cancel();
  identified=UNKNOWN;mc_start=mc_end=mc_rx=mc_tx=mc_cccd=0;mc_transport_started=false;meshmc::transport_down();
  mt_down();mt_start=mt_end=mt_to=mt_from=mt_num=mt_num_end=mt_cccd=0;
  portENTER_CRITICAL(&mux);
  state.selected=index;state.radio=UNKNOWN;state.paired=false;state.bonded=false;
  state.pin_required=false;state.confirm_required=false;
  snprintf(state.active_name,sizeof(state.active_name),"%s",found.peers[index].name);
  portEXIT_CRITICAL(&mux);
  update(CONNECTING,"Connecting to BLE radio...");
  const int rc=ble_gap_connect(own_addr,&found.peers[index].addr,20000,nullptr,gap_event,nullptr);
  if(rc){char msg[64];snprintf(msg,sizeof(msg),"Connect error: %d",rc);update(FAILED,msg);}
}
static void disconnect(){
  // A deliberate disconnect stops automatic retry until the next boot.
  auto_paused.store(true);auto_scan=false;auto_candidate=-1;
  const Phase phase=snapshot().phase;
  if(phase==OFF){update(OFF,"Bluetooth is off; enable before scan");return;}
  if(phase==STARTING){update(STARTING,"Bluetooth is still starting");return;}
  const uint16_t handle=conn;
  if(handle!=BLE_HS_CONN_HANDLE_NONE){
    (void)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
    update(IDLE,"Disconnect requested");
  }else if(snapshot().phase==SCANNING){
    (void)ble_gap_disc_cancel();update(IDLE,"Scan cancelled");
  }else if(snapshot().phase==CONNECTING){
    (void)ble_gap_conn_cancel();update(IDLE,"Connection cancelled");
  }else update(IDLE,"Ready to scan");
}
static bool submit_pin(uint32_t passkey){
  if(passkey>999999)return false;
  const Snapshot s=snapshot();
  if(!s.pin_required||conn==BLE_HS_CONN_HANDLE_NONE)return false;
  struct ble_sm_io io={};
  io.action=BLE_SM_IOACT_INPUT;io.passkey=passkey;
  int rc=ble_sm_inject_io(conn,&io);
  printf("[MCX-BLE] PIN submission rc=%d\n",rc);
  if(rc){update(FAILED,"PIN submission failed");return false;}
  portENTER_CRITICAL(&mux);
  state.pin_required=false;state.generation++;
  portEXIT_CRITICAL(&mux);
  update(PAIRING,"PIN submitted; waiting for pairing...");
  return true;
}
static bool confirm_number(bool accepted){
  if(!snapshot().confirm_required||conn==BLE_HS_CONN_HANDLE_NONE)return false;
  struct ble_sm_io io={};
  io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=accepted?1:0;
  const int rc=ble_sm_inject_io(conn,&io);
  portENTER_CRITICAL(&mux);
  state.confirm_required=false;state.generation++;
  portEXIT_CRITICAL(&mux);
  update(PAIRING,accepted?"Confirming pairing...":"Pairing rejected");
  return rc==0;
}
static void transport_failure(const char *message){
  mt_down();
  meshmc::transport_down();update(FAILED,message);
  if(conn!=BLE_HS_CONN_HANDLE_NONE)(void)ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);
}
static int mc_write_cb(uint16_t handle,const struct ble_gatt_error *err,struct ble_gatt_attr *,void *){
  if(handle==conn)meshmc::on_write(err->status==0);
  return 0;
}
static bool mc_write(const uint8_t *p,size_t n){
  if(!mc_transport_started||conn==BLE_HS_CONN_HANDLE_NONE||!mc_rx||n>(size_t)(ble_att_mtu(conn)-3))return false;
  return ble_gattc_write_flat(conn,mc_rx,p,n,mc_write_cb,nullptr)==0;
}
static void mc_close(){transport_failure("MeshCore session stopped; reconnect radio");}
static int mc_subscribed(uint16_t handle,const struct ble_gatt_error *err,struct ble_gatt_attr *,void *){
  if(handle!=conn)return 0;
  if(err->status){transport_failure("MeshCore notifications subscription failed");return 0;}
  mc_transport_started=true;meshmc::transport_up(ble_att_mtu(conn)-3);
  update(READY,"MeshCore notifications ready; synchronizing...");
  save_remembered();return 0;
}
static int mc_descriptor(uint16_t handle,const struct ble_gatt_error *err,uint16_t,const struct ble_gatt_dsc *dsc,void *){
  if(handle!=conn)return 0;
  if(!err->status&&dsc){if(ble_uuid_u16(&dsc->uuid.u)==0x2902)mc_cccd=dsc->handle;return 0;}
  if(err->status!=BLE_HS_EDONE||!mc_cccd){transport_failure("MeshCore TX notification descriptor missing");return 0;}
  c…43226 tokens truncated…,"Tap the top bar for quick settings. Wi-Fi works; other controls are previews.");break;
    default:break;
  }
}
static void on_action(lv_event_t *e){
  if(edge_swipe_click_in_progress())return;
  lv_async_call(run_action,lv_event_get_user_data(e));
}
static bool suppress_left_nav_click=false;
static void on_nav(lv_event_t *e){
  if(suppress_left_nav_click||edge_swipe_click_in_progress())return;
  page=(int)(intptr_t)lv_event_get_user_data(e);
  if(page==5)more_view=0;
  close_shade();draw();
}
// Poll the existing LVGL touch device from the LVGL thread. Unlike
// LV_EVENT_GESTURE, this also works when a scrollable chat or map is visible;
// only edge-armed horizontal movements may change layout.
static mcxswipe::State swipe_state={};
static lv_indev_t *swipe_pointer=nullptr;
static bool left_drag_detected=false;
static lv_point_t left_drag_start={};
static bool left_drag_was_compact=true;
static void finish_left_drag(void *){
  left_compact=!left_drag_was_compact;
  left_drag_detected=false;
  suppress_left_nav_click=false;
  mcxswipe::reset(swipe_state);
  draw();
}
static void left_tab_touch(lv_event_t *e){
  const lv_event_code_t kind=lv_event_get_code(e);
  lv_indev_t *dev=lv_indev_get_act();
  if(!dev||lv_indev_get_type(dev)!=LV_INDEV_TYPE_POINTER)return;
  lv_point_t p={};lv_indev_get_point(dev,&p);
  if(kind==LV_EVENT_PRESSED){
    left_drag_start=p;
    left_drag_was_compact=left_compact;
    left_drag_detected=false;
    suppress_left_nav_click=false;
  }else if(kind==LV_EVENT_PRESSING){
    const int dx=int(p.x)-left_drag_start.x;
    const int dy=int(p.y)-left_drag_start.y;
    if(!left_drag_detected&&mcxswipe::iabs(dx)>=28 &&
       mcxswipe::iabs(dx)>=2*mcxswipe::iabs(dy) &&
       ((left_drag_was_compact&&dx>0)||(!left_drag_was_compact&&dx<0))){
      left_drag_detected=true;
      suppress_left_nav_click=true;
    }
  }else if(kind==LV_EVENT_RELEASED && left_drag_detected){
    lv_async_call(finish_left_drag,nullptr);
  }
}
static void edge_swipe_poll(lv_timer_t *){
  if(!swipe_pointer){
    for(lv_indev_t *d=lv_indev_get_next(nullptr);d;d=lv_indev_get_next(d))
      if(lv_indev_get_type(d)==LV_INDEV_TYPE_POINTER){swipe_pointer=d;break;}
  }
  if(!swipe_pointer)return;
  lv_point_t point={};lv_indev_get_point(swipe_pointer,&point);
  // ESP-IDF's pinned LVGL 8.4 has no lv_indev_get_state() API (LVGL 9 only).
  // The v8 public lv_hal_indev.h exposes the current state in proc.state.
  const bool pressed=swipe_pointer->proc.state==LV_INDEV_STATE_PRESSED;
  const bool modal=wifi_dialog||wifi_editor||ble_dialog||info_dialog||shade;
  const int lw=has_left_rail()?(left_compact?40:79):0;
  const int rw=has_context_rail()?(right_compact?40:125):0;
  const mcxswipe::Result gesture=mcxswipe::sample(
    swipe_state,pressed,point.x,point.y,page,has_left_rail(),
    has_context_rail(),left_compact,right_compact,lw,rw,modal,lv_tick_get());
  switch(gesture){
    case mcxswipe::OPEN_LEFT:left_compact=false;break;
    case mcxswipe::CLOSE_LEFT:left_compact=true;break;
    case mcxswipe::OPEN_RIGHT:right_compact=false;break;
    case mcxswipe::CLOSE_RIGHT:right_compact=true;break;
    default:return;
  }
  // Redraw only once, after release. Never destroy widgets while pressed.
  draw();
}
// A horizontal swipe can end on a clickable icon. LVGL 8.x can emit CLICKED
// before the 20ms polling timer sees the released state. Suppress that click
// so dragging a rail never accidentally switches page or activates Compose.
static bool edge_swipe_click_in_progress(){
  if(!swipe_state.down||swipe_state.origin==mcxswipe::NOT_ARMED)return false;
  lv_indev_t *dev=lv_indev_get_act();
  if(!dev||lv_indev_get_type(dev)!=LV_INDEV_TYPE_POINTER)return false;
  lv_point_t p={};lv_indev_get_point(dev,&p);
  const int dx=int(p.x)-int(swipe_state.start_x);
  const int dy=int(p.y)-int(swipe_state.start_y);
  if(mcxswipe::iabs(dx)<44||mcxswipe::iabs(dx)<2*mcxswipe::iabs(dy))return false;
  const bool correct_direction=
    (swipe_state.origin==mcxswipe::LEFT_OPEN_ZONE&&dx>0)||
    (swipe_state.origin==mcxswipe::LEFT_CLOSE_ZONE&&dx<0)||
    (swipe_state.origin==mcxswipe::RIGHT_OPEN_ZONE&&dx<0)||
    (swipe_state.origin==mcxswipe::RIGHT_CLOSE_ZONE&&dx>0);
  if(!correct_direction)return false;
  swipe_state.last_x=p.x;swipe_state.last_y=p.y;
  return true;
}
static void apply_filter(void *value){
  const int filter=(int)(intptr_t)value;
  if(page==1)nodes_filter=filter;
  else if(page==2)groups_filter=filter;
  else if(page==3){chats_filter=filter;reset_conversation_view();}
  draw();
}
static void on_filter(lv_event_t *e){lv_async_call(apply_filter,lv_event_get_user_data(e));}
static void open_thread_now(void *value){active_thread=(int)(intptr_t)value;page=3;draw();}
static void on_thread(lv_event_t *e){lv_async_call(open_thread_now,lv_event_get_user_data(e));}
static void quick_action_run(void *value){
  int mode=(int)(intptr_t)value;
  if(mode==2){close_shade();wifi_setup(nullptr);return;}
  if(mode==1){close_shade();page=5;more_view=1;draw();return;}
  const char *names[]={"GPS","Bluetooth","","Sound","Brightness"};
  char text[160];snprintf(text,sizeof(text),"%s is a UI preview only. No device setting was changed.",
                           names[mode]);
  notice("QUICK SETTINGS / DEMO",text);
}
static void quick_action(lv_event_t *e){lv_async_call(quick_action_run,lv_event_get_user_data(e));}
static void open_shade(lv_event_t *){
  if(shade){close_shade();return;}
  if(wifi_dialog||info_dialog)return;
  // Full-height modal background hides the underlying page without touching Wi-Fi setup.
  shade=panel_at(lv_scr_act(),0,34,480,286,bg,0);
  lv_obj_set_style_bg_opa(shade,LV_OPA_80,0);
  lv_obj_t *sheet=panel_at(shade,8,8,464,235,panel,9);
  single_label(sheet,"QUICK SETTINGS / STATUS",12,10,340,orange);
  action_button(sheet,"CLOSE",366,6,89,37,panel2,shade_close,0);
  action_button(sheet,"GPS --",9,54,108,48,panel2,quick_action,0);
  action_button(sheet,"BLE --",122,54,108,48,panel2,quick_action,1);
  action_button(sheet,"WI-FI",235,54,108,48,orange,quick_action,2);
  action_button(sheet,"SOUND",348,54,107,48,panel2,quick_action,3);
  single_label(sheet,"Brightness is a preview; no hardware change.",12,119,438,muted);
  action_button(sheet,"DISPLAY / DEMO",9,152,219,48,panel2,quick_action,4);
  action_button(sheet,"DEVICE SETTINGS",235,152,220,48,orange,on_action,A_DIAGNOSTIC);
  single_label(sheet,"GPS / BLE / SOUND: DEMO ONLY",12,209,438,muted);
}
static void show_center_heading(const char *heading_text,const char *subtitle){
  const int w=lv_obj_get_width(body);
  single_label(body,heading_text,8,8,w-16,white);
  if(subtitle)single_label(body,subtitle,8,30,w-16,muted);
}
static lv_obj_t *center_card(int x,int y,int w,int h,const char *title,
                        const char *first,const char *second,lv_color_t color){
  lv_obj_t *card=panel_at(body,x,y,w,h,panel,8);
  single_label(card,title,8,8,w-16,color);
  if(first)single_label(card,first,8,32,w-16,white);
  if(second)single_label(card,second,8,h-23,w-16,muted);
  return card;
}
static void chat_row(int y,int w,const char *network,const char *who,const char *detail,lv_color_t color){
  lv_obj_t *button=action_button(body,"",8,y,w-16,53,panel,on_action,A_NEW);
  single_label(button,network,8,5,32,color);
  single_label(button,who,47,6,w-61,white);
  single_label(button,detail,47,30,w-61,muted);
}
static void demo_row(int y,int w,const char *network,const char *title,const char *detail,
                     lv_color_t tint,int thread){
  lv_obj_t *b=action_button(body,"",8,y,w-16,57,panel,
                            thread>=0?on_thread:on_action,
                            thread>=0?thread:A_NODE_DETAILS);
  single_label(b,network,8,5,45,tint);
  single_label(b,title,54,5,w-71,white);
  single_label(b,detail,54,29,w-71,muted);
}
static void filter_tabs(int w,int filter){
  const int third=(w-22)/3;
  const char *labels[]={"ALL","MT","MC"};
  for(int i=0;i<3;i++){
    lv_obj_t *b=action_button(body,labels[i],8+i*(third+3),52,third,38,
                              filter==i?orange:panel2,on_filter,i);
    if(filter==i)lv_obj_set_style_text_color(lv_obj_get_child(b,0),bg,0);
  }
}
static void map_preview(int w){
  const int width=w-16;
  lv_obj_t *map=panel_at(body,8,56,width,155,lv_color_hex(0x151B1B),6);
  for(int i=1;i<5;i++){
    lv_obj_t *vertical=lv_obj_create(map);fill(vertical,lv_color_hex(0x293B36),0);
    lv_obj_set_pos(vertical,i*(width/5),0);lv_obj_set_size(vertical,1,155);
  }
  for(int j=1;j<4;j++){
    lv_obj_t *horizontal=lv_obj_create(map);fill(horizontal,lv_color_hex(0x293B36),0);
    lv_obj_set_pos(horizontal,0,j*38);lv_obj_set_size(horizontal,width,1);
  }
  // Topographic-style contours are illustrative, not terrain or loaded map tiles.
  static lv_point_t topo_a[5]={{0,113},{50,92},{102,118},{160,83},{250,100}};
  static lv_point_t topo_b[5]={{0,68},{45,54},{90,74},{158,38},{250,52}};
  static lv_point_t topo_c[5]={{0,141},{45,125},{108,144},{178,117},{250,131}};
  // Keep sample contours proportional when the content area grows.
  const int ax[5]={0,50,102,160,250},bx[5]={0,45,90,158,250},cx[5]={0,45,108,178,250};
  for(int i=0;i<5;i++){
    topo_a[i].x=(lv_coord_t)(ax[i]*(width-16)/250);
    topo_b[i].x=(lv_coord_t)(bx[i]*(width-16)/250);
    topo_c[i].x=(lv_coord_t)(cx[i]*(width-16)/250);
  }
  lv_obj_t *contours[3]={lv_line_create(map),lv_line_create(map),lv_line_create(map)};
  lv_line_set_points(contours[0],topo_a,5);
  lv_line_set_points(contours[1],topo_b,5);
  lv_line_set_points(contours[2],topo_c,5);
  for(int k=0;k<3;k++){
    lv_obj_set_style_line_width(contours[k],2,0);
    lv_obj_set_style_line_color(contours[k],lv_color_hex(0x5B6B4C),0);
    lv_obj_set_pos(contours[k],0,0);
  }
  lv_obj_t *pin=lv_obj_create(map);
  fill(pin,orange,11);lv_obj_set_pos(pin,width/2-5,73);lv_obj_set_size(pin,11,11);
  lv_obj_set_style_border_color(pin,white,0);lv_obj_set_style_border_width(pin,2,0);
  lv_obj_t *label=panel_at(map,7,124,width-14,24,panel,4);
  color_label(label,"ILLUSTRATIVE / NO OFFLINE TILES",5,5,width-24,muted);
}
// Icons stay active even when the right rail is collapsed.
static const char *context_glyph(int id){
  switch(id){
    case A_NEW:return LV_SYMBOL_EDIT;
    case A_PACKETS:return LV_SYMBOL_LIST;
    case A_THREAD_INFO:return LV_SYMBOL_FILE;
    case A_MAP_HOME:return LV_SYMBOL_GPS;
    case A_POIS:return LV_SYMBOL_HOME;
    case A_TRACKS:return LV_SYMBOL_LIST;
    case A_RECORD:return LV_SYMBOL_PLAY;
    case A_SHARE:return LV_SYMBOL_UPLOAD;
    case A_GNSS:return LV_SYMBOL_WIFI;
    case A_TEAM:return LV_SYMBOL_DIRECTORY;
    default:return LV_SYMBOL_SETTINGS;
  }
}
// Place a separate, thin scroll indicator LEFT of the action buttons.
static void update_context_scroll_track(){
  if(!context_scroll_track||!context_scroll_thumb||!context_actions)return;
  if(page!=4||!has_context_rail()){
    lv_obj_add_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);return;
  }
  lv_obj_clear_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);
  // LVGL can temporarily report negative offsets during elastic overscroll.
  // Clamp the thumb to its own gutter and size it to the visible fraction.
  const int viewport=lv_obj_get_height(context_actions);
  const int scroll_y=lv_obj_get_scroll_y(context_actions);
  const int bottom=lv_obj_get_scroll_bottom(context_actions);
  const int range=(scroll_y>0?scroll_y:0)+(bottom>0?bottom:0);
  const int track=266;
  const int content=viewport+range;
  int thumb=content>0?(track*viewport)/content:track;
  if(thumb<32)thumb=32;
  if(thumb>track)thumb=track;
  const int offset=scroll_y<0?0:(scroll_y>range?range:scroll_y);
  const int thumb_y=range>0?(offset*(track-thumb))/range:0;
  lv_obj_set_pos(context_scroll_thumb,0,thumb_y);
  lv_obj_set_height(context_scroll_thumb,thumb);
}
static void context_scrolled(lv_event_t *){update_context_scroll_track();}
static void rebuild_context(){
  lv_obj_clean(context_actions);
  lv_obj_scroll_to_y(context_actions,0,LV_ANIM_OFF);
  if(!has_context_rail()){
    update_context_scroll_track();return;
  }
  const ActionInfo *choices=page==3?chat_actions:map_actions;
  const size_t count=page==3?sizeof(chat_actions)/sizeof(chat_actions[0]):
                             sizeof(map_actions)/sizeof(map_actions[0]);
  if(!right_compact)single_label(context_actions,page==3?"CHAT":"MAP",7,6,109,orange);
  int y=right_compact?4:32;
  for(size_t i=0;i<count;i++){
    const int id=choices[i].id;
    const bool selected=page==4&&
      ((id==A_MAP_HOME&&map_view==0)||(id==A_POIS&&map_view==1)||
       (id==A_TRACKS&&map_view==2)||(id==A_GNSS&&map_view==3)||
       (id==A_TEAM&&map_view==4)||(id==A_SHARE&&map_view==5));
    lv_obj_t *button=action_button(context_actions,
                                   right_compact?context_glyph(id):choices[i].label,
                                   right_compact?0:4,y,right_compact?32:111,38,
                                   selected?orange:panel2,on_action,(intptr_t)id);
    lv_obj_set_style_border_width(button,selected?0:1,0);
    // Match the left primary navigation palette: orange icons on dark panels,
    // dark glyph on orange for the selected action.
    lv_obj_set_style_border_color(button,orange,0);
    lv_obj_set_style_text_color(lv_obj_get_child(button,0),selected?bg:orange,0);
    y+=41;
  }
  lv_obj_update_layout(context_actions);
  update_context_scroll_track();
}
static void ble_select_now(void *v){meshble::connect((int)(intptr_t)v);draw();}
static void ble_select(lv_event_t *e){lv_async_call(ble_select_now,lv_event_get_user_data(e));}
static void ble_dialog_close(){
  if(ble_pin_keyboard)lv_keyboard_set_textarea(ble_pin_keyboard,nullptr);
  if(ble_dialog)lv_obj_del(ble_dialog);
  ble_dialog=nullptr;ble_pin=nullptr;ble_pin_keyboard=nullptr;ble_pin_notice=nullptr;
}
static void ble_dialog_close_later(void *){ble_dialog_close();}
static void ble_pin_action(lv_event_t *e){
  if((intptr_t)lv_event_get_user_data(e)==0){meshble::disconnect();lv_async_call(ble_dialog_close_later,nullptr);return;}
  if(!ble_pin)return;
  const char *p=lv_textarea_get_text(ble_pin);
  if(strlen(p)!=6){
    if(ble_pin_notice)lv_label_set_text(ble_pin_notice,"Enter all six digits before pairing.");
    return;
  }
  uint32_t code=0;
  for(int i=0;i<6;i++){if(p[i]<'0'||p[i]>'9')return;code=code*10+(uint32_t)(p[i]-'0');}
  if(meshble::submit_pin(code))lv_async_call(ble_dialog_close_later,nullptr);
  else if(ble_pin_notice)lv_label_set_text(ble_pin_notice,"PIN not submitted. Cancel and reconnect.");
}
static void ble_confirmation(lv_event_t *e){
  (void)meshble::confirm_number((intptr_t)lv_event_get_user_data(e)!=0);
  lv_async_call(ble_dialog_close_later,nullptr);
}
#define MCX_PIN_KEY (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | 1)
static const char *ble_pin_map[]={"1","2","3","\n","4","5","6","\n","7","8","9","\n",LV_SYMBOL_BACKSPACE,"0",LV_SYMBOL_OK,""};
static const lv_btnmatrix_ctrl_t ble_pin_ctrl[]={MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,
                                               MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY};
#undef MCX_PIN_KEY
static void ble_show_pairing(){
  const meshble::Snapshot s=meshble::snapshot();
  if(ble_dialog||(!s.pin_required&&!s.confirm_required))return;
  ble_dialog=panel_at(lv_layer_top(),0,0,480,320,bg,0);
  lv_obj_move_foreground(ble_dialog);
  action_button(ble_dialog,"CANCEL",366,4,106,34,panel2,ble_pin_action,0);
  single_label(ble_dialog,"BLUETOOTH PAIRING",10,8,260,orange);
  if(s.pin_required){
    ble_pin_notice=single_label(ble_dialog,"Enter your radio's six-digit Bluetooth PIN.",10,37,460,white);
    ble_pin=lv_textarea_create(ble_dialog);
    lv_obj_set_pos(ble_pin,10,62);lv_obj_set_size(ble_pin,278,42);
    lv_textarea_set_one_line(ble_pin,true);
    lv_obj_set_height(ble_pin,42);
    lv_textarea_set_accepted_chars(ble_pin,"0123456789");
    lv_textarea_set_max_length(ble_pin,6);
    lv_textarea_set_password_mode(ble_pin,true);
    lv_obj_add_state(ble_pin,LV_STATE_FOCUSED);
    action_button(ble_dialog,"PAIR",300,62,172,42,orange,ble_pin_action,1);
    ble_pin_keyboard=lv_keyboard_create(ble_dialog);
    // lv_keyboard defaults to BOTTOM_MID: positive y offsets otherwise clip it.
    lv_obj_set_align(ble_pin_keyboard,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(ble_pin_keyboard,4,112);lv_obj_set_size(ble_pin_keyboard,472,204);
    lv_keyboard_set_map(ble_pin_keyboard,LV_KEYBOARD_MODE_NUMBER,ble_pin_map,ble_pin_ctrl);
    lv_keyboard_set_mode(ble_pin_keyboard,LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ble_pin_keyboard,ble_pin);
    lv_obj_set_style_bg_color(ble_pin_keyboard,panel,0);
    lv_obj_set_style_text_font(ble_pin_keyboard,&lv_font_montserrat_20,LV_PART_ITEMS);
    lv_obj_set_style_pad_all(ble_pin_keyboard,3,0);
    lv_obj_set_style_pad_row(ble_pin_keyboard,4,0);
    lv_obj_set_style_pad_column(ble_pin_keyboard,4,0);
    lv_obj_add_event_cb(ble_pin_keyboard,ble_pin_action,LV_EVENT_READY,(void*)1);
    lv_obj_add_event_cb(ble_pin_keyboard,ble_pin_action,LV_EVENT_CANCEL,(void*)0);
  }else{
    char number[48];snprintf(number,sizeof(number),"Verify: %06lu",(unsigned long)s.compare_number);
    single_label(ble_dialog,number,10,67,430,white);
    action_button(ble_dialog,"MATCHES",10,142,208,50,orange,ble_confirmation,1);
    action_button(ble_dialog,"REJECT",227,142,215,50,panel2,ble_confirmation,0);
  }
}
// MeshCore workspace
#include "meshcrossway_meshcore_ui.h"
#include "meshcrossway_meshtastic_ui.h"
static void reset_conversation_view(){mtui::conversation=false;mcui::conversation=false;}
static bool showing_mt(){
  if(page==1)return nodes_filter!=2;
  if(page==2)return groups_filter!=2;
  if(page==3)return chats_filter==1||(chats_filter==0&&(!mcui::conversation||active_chat_network==1));
  return meshble::snapshot().radio==meshble::MESHTASTIC;
}

static void draw(){
  mcxruntime::Scope measured(mcxruntime::DRAW);
  if(!body||!left_rail||!right_rail||!context_actions)return;
  const int lw=has_left_rail()?(left_compact?40:79):0;
  const int rw=has_context_rail()?(right_compact?40:125):0;
  const int cw=480-lw-rw;
  uptime_value=nullptr;heap_value=nullptr;network_value=nullptr;backup_value=nullptr;
  mcui::radio_status=mcui::home_name=mcui::home_status=mcui::history_status=nullptr;
  mtui::radio_status=mtui::home_name=mtui::home_status=nullptr;
  if(lw)lv_obj_clear_flag(left_rail,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(left_rail,LV_OBJ_FLAG_HIDDEN);
  if(rw)lv_obj_clear_flag(right_rail,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(right_rail,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(left_rail,0,34);lv_obj_set_size(left_rail,lw?lw:40,286);
  lv_obj_set_pos(body,lw,34);lv_obj_set_size(body,cw,286);
  lv_obj_set_pos(right_rail,480-rw,34);lv_obj_set_size(right_rail,rw?rw:40,286);
  // All rail height is now available for icons; no arrow-button row.
  // Reserve a dedicated 5px gutter at left of the contextual icons.
  lv_obj_set_pos(context_actions,5,0);
  lv_obj_set_size(context_actions,rw?rw-5:35,286);
  if(context_scroll_track)lv_obj_set_pos(context_scroll_track,1,9);
  for(int i=0;i<6;i++){
    if(!lw){lv_obj_add_flag(tabs[i],LV_OBJ_FLAG_HIDDEN);continue;}
    lv_obj_clear_flag(tabs[i],LV_OBJ_FLAG_HIDDEN);
    const int button_width=lw-6;
    lv_obj_set_pos(tabs[i],3,7+i*45);
    lv_obj_set_size(tabs[i],button_width,38);
    const bool active=i==page;
    const lv_color_t tint=active?bg:orange;
    lv_obj_set_style_bg_color(tabs[i],active?orange:panel2,0);
    lv_obj_t *glyph=lv_obj_get_child(tabs[i],0);
    lv_label_set_text(glyph,tab_glyphs[i]);
    lv_obj_set_style_text_color(glyph,tint,0);
    lv_obj_set_pos(glyph,0,left_compact?9:0);
    lv_obj_set_size(glyph,button_width,19);
    lv_obj_set_style_text_align(glyph,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *caption=lv_obj_get_child(tabs[i],1);
    lv_obj_set_pos(caption,0,21);
    lv_obj_set_size(caption,button_width,16);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(caption,active?bg:white,0);
    if(left_compact)lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(caption,LV_OBJ_FLAG_HIDDEN);
    if(tab_vectors[i]){
      lv_obj_set_pos(tab_vectors[i],(button_width-23)/2,left_compact?9:0);
      update_nav_vector(i,tint);
    }
  }
  lv_obj_clean(body);
  lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
  rebuild_context();
  const int w=cw;
  if(page==1||page==2||page==3){if(showing_mt())mtui::render();else mcui::render();return;}
  if(page==0){
    show_center_heading("FIELD CONSOLE","Independent Meshtastic / MeshCore radios");
    const int col=(w-24)/2;
    const bool narrow=col<155;
    auto *mt_card=center_card(8,54,col,95,"MESHTASTIC",mtui::ready()?mtui::name(meshmt::client->data.my_num):"BLE: OFF",mtui::ready()?"Meshtastic connected":"Connect in More / Radios",green);
    mtui::home_name=lv_obj_get_child(mt_card,1);mtui::home_status=lv_obj_get_child(mt_card,2);
    lv_obj_t *mc_card=center_card(col+16,54,col,95,"MESHCORE",mcui::ready()?meshmc::client->name:"BLE: OFF",mcui::ready()?"MeshCore connected":"Connect in More / Radios",purple);
    mcui::home_name=lv_obj_get_child(mc_card,1);mcui::home_status=lv_obj_get_child(mc_card,2);
    if(mcui::ready())mcui::set_home_name(mcui::home_name,meshmc::client->name);
    center_card(8,157,col,95,"POSITION","GNSS: OFFLINE","No valid fix",cyan);
    center_card(col+16,157,col,95,"CROSSWAY","Separate networks","One active BLE radio",orange);
  } else if(page==4){
    static const char *modes[]={"MAP / OFFLINE","POINTS OF INTEREST","SAVED TRACKS",
                                "GNSS STATUS","TEAM POSITIONS","LIVE LOCATION SHARE"};
    show_center_heading(modes[map_view],"Navigation workspace / DEMO");
    if(map_view==0){
      map_preview(w);
      color_label(body,"Fix: NONE   Accuracy: --   Age: --",8,221,w-16,orange);
      color_label(body,"Sharing OFF  /  demo basemap only",8,249,w-16,muted);
    }else if(map_view==1){
      center_card(8,58,w-16,66,"POI / DEMO","Camp Alpha  [sample]","Not saved to SD",orange);
      center_card(8,131,w-16,66,"WAYPOINT / DEMO","Trail Junction  [sample]","No coordinates recorded",orange);
      color_label(body,"Real POIs require GNSS and SD storage.",8,229,w-16,muted);
    }else if(map_view==2){
      center_card(8,58,w-16,69,"TRACKS / DEMO","Ridge Loop  [sample]","No GPX file loaded",purple);
      center_card(8,134,w-16,69,"FOLDERS / DEMO","Expedition A  [sample]","No tracks stored",purple);
      color_label(body,"GPX import/export is not implemented.",8,232,w-16,muted);
    }else if(map_view==3){
      center_card(8,58,w-16,98,"GNSS / OFFLINE","No MAX-M10S connected","Fix: NONE  /  Sats: --",cyan);
      center_card(8,165,w-16,78,"SKY VIEW / OFF","No satellite observations",nullptr,cyan);
    }else if(map_view==4){
      center_card(8,58,w-16,78,"TEAM / DEMO","No nearby radios connected","All peer positions unavailable",green);
      color_label(body,"No live teammates or location data.",8,163,w-16,muted);
    }else{
      center_card(8,58,w-16,90,"LIVE SHARE / OFF","No session running","No position transmitted",green);
      center_card(8,156,w-16,86,"PRIVACY / REQUIRED","Destination + consent needed","Auto-stop required before share",orange);
    }
  } else {
    if(more_view==1){
      action_button(body,"SETTINGS",w-112,5,104,35,panel2,on_action,A_SETTINGS);
      const meshble::Snapshot s=meshble::snapshot();
      show_center_heading("BLE / RADIO PODS","Meshtastic T-Deck / MeshCore Companion");
      const char *phase=s.phase==meshble::OFF?"DISABLED":s.phase==meshble::STARTING?"STARTING":s.phase==meshble::READY?"SECURE":s.phase==meshble::SCANNING?"SCANNING":s.phase==meshble::CONNECTING?"CONNECTING":s.phase==meshble::VERIFYING?"VERIFYING":s.phase==meshble::PAIRING?"PAIRING":s.phase==meshble::FAILED?"ERROR":"IDLE";
      char detail[64];snprintf(detail,sizeof(detail),"%s / %s",phase,meshble::protocol_name(s.radio));
      single_label(body,detail,8,53,w-16,s.phase==meshble::READY?green:orange);
      single_label(body,s.phase==meshble::READY?(s.bonded?"BLE bond saved / PIN remembered":"Secure link / bond not supported"):s.status,8,76,w-16,muted);
      if(s.phase==meshble::OFF){
        single_label(body,"Safe boot: BLE is off until enabled.",8,106,w-16,green);
        action_button(body,"ENABLE BLE / TEST",8,151,w-16,49,orange,on_action,A_BLE_ENABLE);
        single_label(body,"Wi-Fi and touch should work before BLE.",8,219,w-16,muted);
      }else if(s.phase==meshble::STARTING){
        single_label(body,"BLE starting in dedicated worker...",8,113,w-16,white);
        single_label(body,"Watch Wi-Fi / touch; report boot log.",8,152,w-16,muted);
      }else if(s.phase==meshble::READY){
        lv_obj_t *mc_card=center_card(8,104,w-16,80,"CONNECTED",s.active_name,s.radio==meshble::MESHCORE&&meshmc::client?meshmc::client->status:meshmt::client?meshmt::client->status:"Meshtastic synchronizing",green);
        mcxemoji::view::set_message_text(lv_obj_get_child(mc_card,1),s.active_name);
        if(s.radio==meshble::MESHCORE)mcui::radio_status=lv_obj_get_child(mc_card,2);
        else mtui::radio_status=lv_obj_get_child(mc_card,2);
        action_button(body,s.radio==meshble::MESHTASTIC?"MESHTASTIC STATUS":"MESHCORE TOOLS",8,190,w-16,40,orange,s.radio==meshble::MESHTASTIC?mtui::tools_event:mcui::on_tools,0);
        action_button(body,"DISCONNECT",8,236,w-16,40,panel2,on_action,A_BLE_DISCONNECT);
      }else if(s.phase==meshble::CONNECTING||s.phase==meshble::VERIFYING||s.phase==meshble::PAIRING){
        single_label(body,"BLE handshake / PIN pairing",8,116,w-16,white);
        action_button(body,"DISCONNECT",8,179,w-16,43,panel2,on_action,A_BLE_DISCONNECT);
      }else{
        action_button(body,"SCAN NEARBY BLE",8,105,w-16,40,orange,on_action,A_BLE_SCAN);
        int y=155;
        for(int k=0;k<s.count;k++){
          char name[60];snprintf(name,sizeof(name),"%s / %d dBm",s.peers[k].name,s.peers[k].rssi);
          lv_obj_t *b=action_button(body,"",8,y,w-16,52,panel,ble_select,k);
          lv_obj_t *peer_name=single_label(b,name,7,6,w-28,white);
          mcxemoji::view::set_message_text(peer_name,name);
          single_label(b,meshble::protocol_name(s.peers[k].advertised),7,30,w-28,muted);
          y+=56;
        }
      }
      return;
    }
    show_center_heading("SETTINGS / DEVICE","Core configuration and system status");
    char uptime[42],memory[42],ip[16],wifi[30],status[55];
    const uint64_t seconds=(uint64_t)(esp_timer_get_time()/1000000LL);
    snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
             (unsigned long long)(seconds/3600),(unsigned long long)((seconds/60)%60));
    snprintf(memory,sizeof(memory),"Heap: %lu KB",(unsigned long)(esp_get_free_heap_size()/1024));
    meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
    char backup_detail[48];meshbackup::describe(backup_detail,sizeof(backup_detail));
    snprintf(status,sizeof(status),"SD: %.42s",backup_detail);
    const int col=(w-26)/2;
    lv_obj_t *device_card=panel_at(body,8,53,col,92,panel);
    single_label(device_card,"DEVICE",10,7,col-20,orange);
    uptime_value=single_label(device_card,uptime,10,34,col-20,white);
    heap_value=single_label(device_card,memory,10,60,col-20,muted);
    lv_obj_t *wifi_card=panel_at(body,col+18,53,col,92,panel);
    single_label(wifi_card,"WI-FI / STORAGE",10,7,col-20,orange);
    network_value=single_label(wifi_card,ip[0]?ip:wifi,10,34,col-20,white);
    backup_value=single_label(wifi_card,status,10,60,col-20,muted);
    action_button(body,"WI-FI NETWORKS",8,153,col,39,orange,on_action,A_WIFI);
    action_button(body,"BLUETOOTH / RADIOS",col+18,153,col,39,panel2,on_action,A_RADIOS);
    action_button(body,"REMOTE VIEW",8,198,col,39,panel2,on_action,A_VIEW);
    action_button(body,"DEVICE DIAGNOSTICS",col+18,198,col,39,panel2,on_action,A_DIAGNOSTIC);
    action_button(body,"PRIVACY",8,243,col,35,panel2,on_action,A_PRIVACY);
    action_button(body,"ABOUT",col+18,243,col,35,panel2,on_action,A_ABOUT);
  }
}
static void refresh(lv_timer_t *){
  update_wifi_scan();
  const meshble::Snapshot bs=meshble::snapshot();
  if(bs.generation!=ble_generation){
    ble_generation=bs.generation;
    if(ble_dialog&&!bs.pin_required&&!bs.confirm_required)ble_dialog_close();
    if(page==5&&more_view==1&&!wifi_dialog&&!info_dialog&&!ble_dialog)draw();
    if(bs.pin_required||bs.confirm_required)ble_show_pairing();
  }
  if(wifi_sd_status){
    char detail[64],backup[88];
    meshbackup::describe(detail,sizeof(detail));
    snprintf(backup,sizeof(backup),"SD backup: %s",detail);
    if(strcmp(lv_label_get_text(wifi_sd_status),backup))lv_label_set_text(wifi_sd_status,backup);
  }
  // Compact status bar: real Wi-Fi STA IP state and actual current BLE
  // GATT identity. The current radio adapter has ONE active BLE session.
  if(top_wifi&&top_mt&&top_mc&&top_mt_name&&top_mc_name){
    char ip[16]={},state[30]={};
    meshnetwork::get_status(ip,sizeof(ip),state,sizeof(state));
    const bool wifi_online=ip[0]!=0;
    const bool mt_online=bs.phase==meshble::READY &&
                         bs.radio==meshble::MESHTASTIC && mtui::ready();
    const bool mc_online=bs.phase==meshble::READY &&
                         bs.radio==meshble::MESHCORE && mcui::ready();
    lv_obj_set_style_text_color(top_wifi,wifi_online?green:muted,0);
    lv_obj_set_style_text_color(top_mt,mt_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mc,mc_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mt_name,mt_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mc_name,mc_online?orange:muted,0);
  }
  // PMU readout never fabricates a level for an absent battery.
  if(top_battery&&top_battery_fill&&top_battery_percent&&top_charge){
    const mcxbattery::Reading b=meshpower::snapshot();
    const bool present=b.valid&&b.battery_present&&b.percent<=100;
    char number[8];
    if(present)snprintf(number,sizeof(number),"%u%%",(unsigned)b.percent);
    else snprintf(number,sizeof(number),"--");
    if(strcmp(lv_label_get_text(top_battery_percent),number))
      lv_label_set_text(top_battery_percent,number);
    lv_obj_set_style_border_color(top_battery,present?
      (b.percent<=15?red:green):muted,0);
    lv_obj_set_width(top_battery_fill,present?int(b.percent)*34/100:0);
    lv_obj_set_style_bg_color(top_battery_fill,b.percent<=15?red:green,0);
    const char *charging=b.charging?LV_SYMBOL_CHARGE:
                         (b.valid&&b.usb_present?LV_SYMBOL_USB:"");
    if(strcmp(lv_label_get_text(top_charge),charging))
      lv_label_set_text(top_charge,charging);
    lv_obj_set_style_text_color(top_charge,b.charging?orange:muted,0);
  }
  printf("[MCX-DIAG v0.7.12] uptime_ms=%llu heap=%lu page=%d capture=%lu keys=%lu\n",
         (unsigned long long)(esp_timer_get_time()/1000),
         (unsigned long)esp_get_free_heap_size(),page,
         (unsigned long)meshcapture::captures.load(),(unsigned long)editor_key_count);
  // Never destroy and rebuild LVGL settings cards in the periodic timer.
  if(page!=5||more_view!=0||wifi_dialog||!uptime_value||!heap_value||!network_value||!backup_value)return;
  char uptime[42],memory[42],ip[16],wifi[30],sd[55],backup_detail[48];
  const uint64_t seconds=(uint64_t)(esp_timer_get_time()/1000000LL);
  snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
           (unsigned long long)(seconds/3600),(unsigned long long)((seconds/60)%60));
  snprintf(memory,sizeof(memory),"Heap: %lu KB",(unsigned long)(esp_get_free_heap_size()/1024));
  meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
  meshbackup::describe(backup_detail,sizeof(backup_detail));
  snprintf(sd,sizeof(sd),"SD: %.42s",backup_detail);
  lv_obj_t *objects[]={uptime_value,heap_value,network_value,backup_value};
  const char *values[]={uptime,memory,ip[0]?ip:wifi,sd};
  for(int i=0;i<4;i++)
    if(strcmp(lv_label_get_text(objects[i]),values[i]))lv_label_set_text(objects[i],values[i]);
}
static void init(){
  mcxruntime::begin();
  printf("[MCX-DIAG v0.7.12] boot reset_reason=%d heap=%lu http=MANUAL_CAPTURE touch=OFF wifi=ON\n",
         (int)esp_reset_reason(),(unsigned long)esp_get_free_heap_size());
  lv_obj_t *screen=lv_scr_act();fill(screen,bg);
  lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *header=panel_at(screen,0,0,480,34,panel,0);
  lv_obj_t *top=lv_btn_create(header);fill(top,panel,0);
  lv_obj_set_pos(top,0,0);lv_obj_set_size(top,480,34);
  lv_obj_add_event_cb(top,open_shade,LV_EVENT_CLICKED,nullptr);
  // Use the official mountain mark and traced MeshCrossway lettering.
  lv_obj_t *brand=lv_img_create(top);
  lv_img_set_src(brand,&mcxbrand::official_lockup);
  lv_obj_set_pos(brand,6,5);
  // No DEV, IP, overlapping MT/MC symbols or external battery percentage.
  lv_obj_t *version=color_label(top,"0.7.12",187,11,43,muted);
  lv_obj_set_style_text_font(version,&lv_font_montserrat_10,0);
  top_wifi=color_label(top,LV_SYMBOL_WIFI,242,7,22,muted);
  top_mt=color_label(top,LV_SYMBOL_BLUETOOTH,273,7,20,muted);
  top_mt_name=color_label(top,"MT",293,10,27,muted);
  top_mc=color_label(top,LV_SYMBOL_BLUETOOTH,323,7,20,muted);
  top_mc_name=color_label(top,"MC",343,10,27,muted);
  top_charge=color_label(top,"",403,8,23,muted);
  top_battery=panel_at(top,430,7,44,20,panel,2);
  lv_obj_set_style_border_width(top_battery,1,0);
  lv_obj_set_style_border_color(top_battery,muted,0);
  lv_obj_set_style_radius(top_battery,3,0);
  panel_at(top,475,13,4,8,muted,1);
  top_battery_fill=panel_at(top_battery,3,3,0,14,green,1);
  lv_obj_set_style_bg_opa(top_battery_fill,LV_OPA_40,0);
  top_battery_percent=color_label(top_battery,"--",2,1,40,white);
  lv_obj_set_style_text_align(top_battery_percent,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_text_font(top_battery_percent,&lv_font_montserrat_10,0);
  lv_label_set_long_mode(top_battery_percent,LV_LABEL_LONG_CLIP);
  left_rail=panel_at(screen,0,34,79,286,panel,0);
  right_rail=panel_at(screen,355,34,125,286,panel,0);
  for(int i=0;i<6;i++){
    tabs[i]=action_button(left_rail,"",3,7+i*45,73,38,panel2,on_nav,i);
    // Keep tracking this finger after it leaves the icon bounds during drag.
    lv_obj_add_flag(tabs[i],LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_PRESSED,nullptr);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_PRESSING,nullptr);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_RELEASED,nullptr);
    single_label(tabs[i],tab_names[i],0,19,73,white);
    make_nav_vector(i);
  }
  context_actions=panel_at(right_rail,5,0,120,286,panel,0);
  context_scroll_track=panel_at(right_rail,1,9,3,266,muted,0);
  lv_obj_set_style_bg_opa(context_scroll_track,LV_OPA_30,0);
  context_scroll_thumb=panel_at(context_scroll_track,0,0,3,200,orange,1);
  lv_obj_add_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(context_actions,context_scrolled,LV_EVENT_SCROLL,nullptr);
  // The Map context has more actions than fit vertically; allow finger scrolling.
  lv_obj_add_flag(context_actions,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(context_actions,LV_DIR_VER);
  // Keep touch scrolling, but never draw an overlay over the action icons.
  lv_obj_set_scrollbar_mode(context_actions,LV_SCROLLBAR_MODE_OFF);
  body=panel_at(screen,79,34,276,286,bg,0);
  lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
  draw();
  meshpower::start(); // Read-only 15s AXP2101 samples on a background task.
  lv_timer_create(edge_swipe_poll,20,nullptr);
  lv_timer_create(refresh,3000,nullptr);
  lv_timer_create(mcui::poll,50,nullptr);
  lv_timer_create(mtui::poll,50,nullptr);
}
} // namespace meshcrossway

