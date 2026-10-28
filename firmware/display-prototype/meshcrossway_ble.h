#pragma once
// MeshCrossway v0.6.4 - BLE remains OFF during boot; explicit staged initialization.
// and PIN-authenticated pairing. No application frames are sent.
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
  uint32_t generation;
  char status[64];
  char active_name[29];
};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Snapshot state={};
static uint16_t conn=BLE_HS_CONN_HANDLE_NONE;
static uint8_t own_addr=0;
static bool synced=false;
static Radio identified=UNKNOWN;
static const ble_uuid128_t MT_SERVICE=BLE_UUID128_INIT(
  0xfd,0xea,0x73,0xe2,0xca,0x5d,0xa8,0x9f,0x1f,0x46,0xa8,0x15,0x18,0xb2,0xa1,0x6b);
static const ble_uuid128_t MC_SERVICE=BLE_UUID128_INIT(
  0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);

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
static int gap_event(struct ble_gap_event *event,void *arg);
static void on_sync(){
  const int rc=ble_hs_util_ensure_addr(0);
  if(rc!=0){update(FAILED,"BLE identity unavailable");return;}
  if(ble_hs_id_infer_auto(0,&own_addr)!=0){update(FAILED,"BLE address unavailable");return;}
  synced=true;
  update(IDLE,"Ready to scan (9 seconds)");
}
static void on_reset(int reason){
  synced=false;conn=BLE_HS_CONN_HANDLE_NONE;
  char msg[64];snprintf(msg,sizeof(msg),"BLE controller reset: %d",reason);
  update(FAILED,msg);
}
static void host_task(void *){
  nimble_port_run();
  nimble_port_freertos_deinit();
}
static void scan(){
  if(!synced){
    if(snapshot().phase==OFF)update(OFF,"Enable BLE before scanning");
    else update(FAILED,"BLE host not ready");
    return;
  }
  const Snapshot current=snapshot();
  if(current.phase==CONNECTING||current.phase==VERIFYING||current.phase==PAIRING||current.phase==READY)return;
  (void)ble_gap_disc_cancel();
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
  if(rc==0)update(SCANNING,"Searching nearby BLE radios...");
  else {char msg[64];snprintf(msg,sizeof(msg),"Scan error: %d",rc);update(FAILED,msg);}
}
static void connect(int index){
  const Snapshot found=snapshot();
  if(index<0||index>=found.count)return;
  if(found.phase==READY||found.phase==PAIRING||found.phase==VERIFYING||found.phase==CONNECTING)return;
  (void)ble_gap_disc_cancel();
  identified=UNKNOWN;
  portENTER_CRITICAL(&mux);
  state.selected=index;state.radio=UNKNOWN;state.paired=false;
  state.pin_required=false;state.confirm_required=false;
  snprintf(state.active_name,sizeof(state.active_name),"%s",found.peers[index].name);
  portEXIT_CRITICAL(&mux);
  update(CONNECTING,"Connecting to BLE radio...");
  const int rc=ble_gap_connect(own_addr,&found.peers[index].addr,20000,nullptr,gap_event,nullptr);
  if(rc){char msg[64];snprintf(msg,sizeof(msg),"Connect error: %d",rc);update(FAILED,msg);}
}
static void disconnect(){
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
static int service_cb(uint16_t handle,const struct ble_gatt_error *err,
                      const struct ble_gatt_svc *service,void *){
  if(err->status==0&&service){
    if(ble_uuid_cmp(&service->uuid.u,&MT_SERVICE.u)==0)identified=MESHTASTIC;
    if(ble_uuid_cmp(&service->uuid.u,&MC_SERVICE.u)==0)identified=MESHCORE;
    return 0;
  }
  if(err->status==BLE_HS_EDONE){
    if(identified==UNKNOWN){
      update(FAILED,"No Meshtastic / MeshCore GATT service");
      (void)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
      return 0;
    }
    portENTER_CRITICAL(&mux);
    state.radio=identified;state.generation++;
    portEXIT_CRITICAL(&mux);
    update(PAIRING,"Service found; pairing / encryption...");
    const int rc=ble_gap_security_initiate(handle);
    if(rc){
      char msg[64];snprintf(msg,sizeof(msg),"Pairing start error: %d",rc);
      update(FAILED,msg);
      (void)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
    }
  }else{
    update(FAILED,"GATT service discovery failed");
    (void)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
  }
  return 0;
}
static int gap_event(struct ble_gap_event *event,void *){
  switch(event->type){
    case BLE_GAP_EVENT_DISC:{
      const struct ble_gap_disc_desc &disc=event->disc;
      struct ble_hs_adv_fields fields={};
      if(ble_hs_adv_parse_fields(&fields,disc.data,disc.length_data)!=0)return 0;
      // Keep named connectable advertisements, then merge subsequent scan responses.
      bool connectable=(disc.event_type==BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
                        disc.event_type==BLE_HCI_ADV_RPT_EVTYPE_DIR_IND);
      Radio hint=UNKNOWN;
      for(int i=0;i<fields.num_uuids128;i++){
        if(ble_uuid_cmp(&fields.uuids128[i].u,&MT_SERVICE.u)==0)hint=MESHTASTIC;
        if(ble_uuid_cmp(&fields.uuids128[i].u,&MC_SERVICE.u)==0)hint=MESHCORE;
      }
      portENTER_CRITICAL(&mux);
      int index=-1;
      for(int i=0;i<state.count;i++)
        if(!memcmp(state.peers[i].addr.val,disc.addr.val,6)&&
           state.peers[i].addr.type==disc.addr.type){index=i;break;}
      if(index<0 && connectable && (fields.name_len>0 || hint!=UNKNOWN) &&
         state.count<8){
        index=state.count++;
        state.peers[index].addr=disc.addr;
        snprintf(state.peers[index].name,sizeof(state.peers[index].name),"BLE radio %02X:%02X",
                 disc.addr.val[1],disc.addr.val[0]);
        state.peers[index].advertised=UNKNOWN;
      }
      if(index>=0){
        if(fields.name && fields.name_len){
          const size_t n=fields.name_len<sizeof(state.peers[index].name)-1?
                         fields.name_len:sizeof(state.peers[index].name)-1;
          memcpy(state.peers[index].name,fields.name,n);
          state.peers[index].name[n]=0;
        }
        state.peers[index].rssi=disc.rssi;
        if(hint!=UNKNOWN)state.peers[index].advertised=hint;
        state.generation++;
      }
      portEXIT_CRITICAL(&mux);
      return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:
      if(snapshot().phase==SCANNING)update(IDLE,"Scan complete - choose a device");
      return 0;
    case BLE_GAP_EVENT_LINK_ESTAB:
      if(event->connect.status!=0){
        conn=BLE_HS_CONN_HANDLE_NONE;
        char msg[64];snprintf(msg,sizeof(msg),"BLE connect failed: %d",event->connect.status);
        update(FAILED,msg);return 0;
      }
      conn=event->connect.conn_handle;
      update(VERIFYING,"Connected; checking radio protocol...");
      if(ble_gattc_disc_all_svcs(conn,service_cb,nullptr)!=0){
        update(FAILED,"Could not discover GATT services");
        (void)ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      if(event->passkey.params.action==BLE_SM_IOACT_INPUT){
        portENTER_CRITICAL(&mux);
        state.pin_required=true;state.generation++;
        portEXIT_CRITICAL(&mux);
        update(PAIRING,"Enter the six-digit PIN shown on Heltec");
      }else if(event->passkey.params.action==BLE_SM_IOACT_NUMCMP){
        portENTER_CRITICAL(&mux);
        state.compare_number=event->passkey.params.numcmp;
        state.confirm_required=true;state.generation++;
        portEXIT_CRITICAL(&mux);
        update(PAIRING,"Compare six-digit number on the Heltec");
      }else{
        update(FAILED,"Unsupported pairing method");
        (void)ble_gap_terminate(event->passkey.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if(event->enc_change.status==0){
        portENTER_CRITICAL(&mux);
        state.paired=true;state.pin_required=false;state.confirm_required=false;
        state.generation++;
        portEXIT_CRITICAL(&mux);
        update(READY,"BLE secure link / GATT verified");
      }else{
        char msg[64];snprintf(msg,sizeof(msg),"Pairing failed: %d",event->enc_change.status);
        update(FAILED,msg);
        (void)ble_gap_terminate(event->enc_change.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:{
      conn=BLE_HS_CONN_HANDLE_NONE;
      const Phase before=snapshot().phase;
      portENTER_CRITICAL(&mux);
      state.paired=false;state.pin_required=false;state.confirm_required=false;
      state.generation++;
      portEXIT_CRITICAL(&mux);
      if(before!=FAILED)update(IDLE,"Radio disconnected");
      return 0;
    }
    default:return 0;
  }
}
static void debug_memory(const char *phase){
  const uint32_t free_internal=(uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largest_internal=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t free_dma=(uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  printf("[MCX-MEM] %s core=%d internal=%lu largest=%lu dma=%lu all=%lu\\n",
         phase,(int)xPortGetCoreID(),(unsigned long)free_internal,
         (unsigned long)largest_internal,(unsigned long)free_dma,
         (unsigned long)esp_get_free_heap_size());
  fflush(stdout);
}
static void start_internal(){
  printf("[MCX-BLE] worker entered; heap=%lu\n",(unsigned long)esp_get_free_heap_size());
  debug_memory("before nimble");
  update(STARTING,"Initializing NimBLE in worker...");
  const esp_err_t err=nimble_port_init();
  printf("[MCX-BLE] nimble_port_init result=%d heap=%lu\n",(int)err,(unsigned long)esp_get_free_heap_size());
  if(err!=ESP_OK){
    char msg[64];snprintf(msg,sizeof(msg),"NimBLE init failed: %d",(int)err);
    update(FAILED,msg);return;
  }
  ble_hs_cfg.reset_cb=on_reset;
  ble_hs_cfg.sync_cb=on_sync;
  ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
  ble_hs_cfg.sm_io_cap=BLE_HS_IO_KEYBOARD_ONLY;
  ble_hs_cfg.sm_bonding=1;
  ble_hs_cfg.sm_mitm=1;
  ble_hs_cfg.sm_sc=1;
  (void)ble_svc_gap_device_name_set("MeshCrossway");
  ble_store_config_init();
  debug_memory("before host task");
  nimble_port_freertos_init(host_task);
  printf("[MCX-BLE] host created; awaiting sync\n");
}
static void init_task(void *){
  // Never initialize controller/host in LVGL or the early app_main boot path.
  // A short delay allows LVGL, the Wi-Fi AP/STA and HTTP to become responsive.
  vTaskDelay(pdMS_TO_TICKS(500));
  start_internal();
  vTaskDelete(nullptr);
}
static bool request_start(){
  if(snapshot().phase!=OFF)return false; // Do not repeat a partial or failed init.
  debug_memory("BLE button pressed");
  update(STARTING,"BLE worker requested - standby...");
  const BaseType_t created=xTaskCreate(init_task,"mcx_ble_init",6144,nullptr,tskIDLE_PRIORITY+1,nullptr);
  if(created!=pdPASS){
    update(FAILED,"No memory for BLE worker; restart");
    return false;
  }
  return true;
}
} // namespace meshble
