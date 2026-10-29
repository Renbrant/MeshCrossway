#pragma once
// MeshCrossway v0.7.0 - BLE remains OFF during boot; explicit staged initialization.
// PIN-authenticated MeshCore Companion transport; Meshtastic remains discovery-only.
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
#include "meshcrossway_meshcore.h"
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
  uint32_t generation;
  char status[64];
  char active_name[29];
};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Snapshot state={};
static std::atomic<uint16_t> conn{BLE_HS_CONN_HANDLE_NONE};
static uint8_t own_addr=0;
static bool synced=false;
static Radio identified=UNKNOWN;
static const ble_uuid128_t MT_SERVICE=BLE_UUID128_INIT(
  0xfd,0xea,0x73,0xe2,0xca,0x5d,0xa8,0x9f,0x1f,0x46,0xa8,0x15,0x18,0xb2,0xa1,0x6b);
static const ble_uuid128_t MC_SERVICE=BLE_UUID128_INIT(
  0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);

static uint16_t mc_start=0,mc_end=0,mc_rx=0,mc_tx=0,mc_tx_end=0,mc_cccd=0;
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
static int gap_event(struct ble_gap_event *event,void *arg);
static void debug_memory(const char *phase);
static void on_sync(){
  const int rc=ble_hs_util_ensure_addr(0);
  if(rc!=0){update(FAILED,"BLE identity unavailable");return;}
  if(ble_hs_id_infer_auto(0,&own_addr)!=0){update(FAILED,"BLE address unavailable");return;}
  debug_memory("host synced");
  synced=true;
  update(IDLE,"Ready to scan (9 seconds)");
}
static void on_reset(int reason){
  synced=false;conn=BLE_HS_CONN_HANDLE_NONE;mc_transport_started=false;meshmc::transport_down();
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
  identified=UNKNOWN;mc_start=mc_end=mc_rx=mc_tx=mc_cccd=0;mc_transport_started=false;meshmc::transport_down();
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
  update(READY,"MeshCore notifications ready; synchronizing...");return 0;
}
static int mc_descriptor(uint16_t handle,const struct ble_gatt_error *err,uint16_t,const struct ble_gatt_dsc *dsc,void *){
  if(handle!=conn)return 0;
  if(!err->status&&dsc){if(ble_uuid_u16(&dsc->uuid.u)==0x2902)mc_cccd=dsc->handle;return 0;}
  if(err->status!=BLE_HS_EDONE||!mc_cccd){transport_failure("MeshCore TX notification descriptor missing");return 0;}
  const uint8_t enable[2]={1,0};
  if(ble_gattc_write_flat(handle,mc_cccd,enable,2,mc_subscribed,nullptr))transport_failure("Cannot subscribe to MeshCore TX");
  return 0;
}
static int mc_characteristic(uint16_t handle,const struct ble_gatt_error *err,const struct ble_gatt_chr *chr,void *){
  if(handle!=conn)return 0;
  if(!err->status&&chr){
    if(mc_tx&&chr->def_handle>mc_tx&&(!mc_tx_end||chr->def_handle-1<mc_tx_end))mc_tx_end=chr->def_handle-1;
    if(ble_uuid_cmp(&chr->uuid.u,&MC_RX.u)==0&&(chr->properties&BLE_GATT_CHR_F_WRITE))mc_rx=chr->val_handle;
    if(ble_uuid_cmp(&chr->uuid.u,&MC_TX.u)==0&&(chr->properties&BLE_GATT_CHR_F_NOTIFY)){mc_tx=chr->val_handle;mc_tx_end=mc_end;}
    return 0;
  }
  if(err->status!=BLE_HS_EDONE||!mc_rx||!mc_tx||mc_tx_end<=mc_tx){transport_failure("MeshCore RX/TX characteristics missing");return 0;}
  if(ble_gattc_disc_all_dscs(handle,mc_tx,mc_tx_end,mc_descriptor,nullptr))transport_failure("MeshCore descriptors discovery failed");
  return 0;
}
static int mc_mtu(uint16_t handle,const struct ble_gatt_error *err,uint16_t mtu,void *){
  if(handle!=conn)return 0;
  // Contact records are 148/152 bytes and must arrive in one notification.
  if(err->status||mtu<155){transport_failure("MeshCore needs ATT MTU >=155; reconnect/update radio");return 0;}
  if(ble_gattc_disc_all_chrs(handle,mc_start,mc_end,mc_characteristic,nullptr))transport_failure("MeshCore characteristic discovery failed");
  return 0;
}
static void start_meshcore(){
  if(identified!=MESHCORE||mc_transport_started)return;
  mc_rx=mc_tx=mc_tx_end=mc_cccd=0;
  update(VERIFYING,"MeshCore: negotiating MTU / notifications...");
  if(ble_gattc_exchange_mtu(conn,mc_mtu,nullptr))transport_failure("MeshCore MTU exchange failed");
}
static int service_cb(uint16_t handle,const struct ble_gatt_error *err,
                      const struct ble_gatt_svc *service,void *){
  if(handle!=conn)return 0;
  if(err->status==0&&service){
    if(ble_uuid_cmp(&service->uuid.u,&MT_SERVICE.u)==0)identified=MESHTASTIC;
    if(ble_uuid_cmp(&service->uuid.u,&MC_SERVICE.u)==0){identified=MESHCORE;mc_start=service->start_handle;mc_end=service->end_handle;}
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
        printf("[MCX-BLE] PIN entry requested\n");
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
        struct ble_gap_conn_desc desc={};
        if(ble_gap_conn_find(conn,&desc)||!desc.sec_state.encrypted||!desc.sec_state.authenticated){
          transport_failure("Authenticated BLE link required");return 0;
        }
        if(identified==MESHCORE)start_meshcore();
        else update(READY,"BLE secure link / GATT verified");
      }else{
        char msg[64];snprintf(msg,sizeof(msg),"Pairing failed: %d",event->enc_change.status);
        update(FAILED,msg);
        (void)ble_gap_terminate(event->enc_change.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:{
      conn=BLE_HS_CONN_HANDLE_NONE;mc_transport_started=false;meshmc::transport_down();
      const Phase before=snapshot().phase;
      portENTER_CRITICAL(&mux);
      state.paired=false;state.pin_required=false;state.confirm_required=false;
      state.generation++;
      portEXIT_CRITICAL(&mux);
      if(before!=FAILED)update(IDLE,"Radio disconnected");
      return 0;
    }
    case BLE_GAP_EVENT_NOTIFY_RX:{
      if(event->notify_rx.conn_handle!=conn||event->notify_rx.attr_handle!=mc_tx||!mc_transport_started)return 0;
      uint8_t data[meshmc::MAX_FRAME];const uint16_t n=OS_MBUF_PKTLEN(event->notify_rx.om);
      if(n>sizeof(data)||os_mbuf_copydata(event->notify_rx.om,0,n,data)!=0){meshmc::overflow.store(true);return 0;}
      meshmc::on_frame(data,n);return 0;
    }
    default:return 0;
  }
}
static void debug_memory(const char *phase){
  const uint32_t free_internal=(uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largest_internal=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largest_dma=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  const uint32_t free_dma=(uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  printf("[MCX-MEM] %s core=%d internal=%lu largest=%lu dma=%lu largest_dma=%lu stack_hwm=%u all=%lu\n",
         phase,(int)xPortGetCoreID(),(unsigned long)free_internal,
         (unsigned long)largest_internal,(unsigned long)free_dma,
         (unsigned long)largest_dma,(unsigned)uxTaskGetStackHighWaterMark(nullptr),
         (unsigned long)esp_get_free_heap_size());
  fflush(stdout);
}
static void start_internal(){
  printf("[MCX-BLE] worker entered; heap=%lu\n",(unsigned long)esp_get_free_heap_size());
  debug_memory("before nimble");
  update(STARTING,"Initializing NimBLE in worker...");
  const esp_err_t err=nimble_port_init();
  printf("[MCX-BLE] nimble_port_init result=%d heap=%lu\n",(int)err,(unsigned long)esp_get_free_heap_size());
  debug_memory("after nimble init");
  if(err!=ESP_OK){
    char msg[64];snprintf(msg,sizeof(msg),"NimBLE init failed: %d",(int)err);
    update(FAILED,msg);return;
  }
  (void)ble_att_set_preferred_mtu(247);
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
  if(!meshmc::init()){update(FAILED,"No PSRAM for MeshCore session");return false;}
  meshmc::send_frame=mc_write;meshmc::close_link=mc_close;
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
