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
#include "esp_bt.h"
#include "meshcrossway_radio_priority.h"
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
static bool mt_callback_current(uint16_t handle,void *token){return handle==conn&&uintptr_t(token)==mt_epoch.load();
}
static void *mt_token(){return reinterpret_cast<void *>(uintptr_t(mt_epoch.load()));
}
static int mt_written(uint16_t handle,const struct ble_gatt_error *err,struct ble_gatt_attr *,void *token){
  if(!mt_callback_current(handle,token))return 0;
  meshmt::on_write(err->status==0);
  mt_io_busy.store(false);
  mt_need_read.store(true);
  return 0;
}
static bool mt_can_write(){return mt_transport_started.load()&&!mt_io_busy.load()&&conn!=BLE_HS_CONN_HANDLE_NONE;
}
static bool mt_write(const uint8_t *p,size_t n){
  if(!mt_can_write()||!mt_to||!p||!n||n>meshmt::MAX_FRAME)return false;
  const uint16_t mtu=ble_att_mtu(conn);
  if(mtu<23)return false;
  bool expected=false;
  if(!mt_io_busy.compare_exchange_strong(expected,true))return false;
  // PhoneAPI sends one complete protobuf per GATT write. For smaller MTUs,
  // use ATT Prepare/Execute Write rather than splitting a protobuf.
  int rc=0;
  if(n<=size_t(mtu-3)){
    rc=ble_gattc_write_flat(conn,mt_to,p,n,mt_written,mt_token());
  }else{
    struct os_mbuf *payload=ble_hs_mbuf_from_flat(p,n);
    if(!payload){mt_io_busy=false;return false;}
    // NimBLE consumes the mbuf, including on failed initiation.
    rc=ble_gattc_write_long(conn,mt_to,0,payload,mt_written,mt_token());
  }
  if(rc)mt_io_busy=false;
  return rc==0;
}
static void mt_close(){transport_failure(meshmt::client?meshmt::client->status:"Meshtastic session stopped");
}
static int mt_read_result(uint16_t handle,const struct ble_gatt_error *err,struct ble_gatt_attr *attr,void *token){
  if(!mt_callback_current(handle,token)||!mt_transport_started)return 0;
  auto &frame=meshmt::buffers->reading;
  if(!err->status&&attr){
    const uint16_t n=OS_MBUF_PKTLEN(attr->om);
    if(attr->offset!=frame.len||n>meshmt::MAX_FRAME-frame.len||os_mbuf_copydata(attr->om,0,n,frame.data+frame.len)){
      transport_failure("Meshtastic long-read invalid / over 512 bytes");
      return 0;
    }
    frame.len+=n;
    return 0;
  }
  if(err->status!=BLE_HS_EDONE){transport_failure("Meshtastic FromRadio read failed; reconnect");
    return 0;
  }
  if(frame.len){if(!meshmt::on_frame(frame.data,frame.len)){transport_failure("Meshtastic receive queue full");
      return 0;
    }}
  else if(mt_read_notice==mt_notifications.load())mt_need_read.store(false);
  mt_last_read=uint32_t(esp_timer_get_time()/1000);
  mt_io_busy.store(false);
  return 0;
}
static void mt_pump_read(){
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(!mt_can_write()||(!mt_need_read.load()&&(uint32_t)(now-mt_last_read)<2000))return;
  bool expected=false;
  if(!mt_io_busy.compare_exchange_strong(expected,true))return;
  meshmt::buffers->reading.len=0;
  mt_read_notice=mt_notifications.load();
  // Standard GATT long read preserves a complete FromRadio at MTU 512 as well
  // as 517. No stream header and no assumption that a notification is a packet.
  if(ble_gattc_read_long(conn,mt_from,0,mt_read_result,mt_token())){mt_io_busy=false;
    transport_failure("Cannot read Meshtastic FromRadio");
  }
}
static int mt_subscribed(uint16_t handle,const struct ble_gatt_error *err,struct ble_gatt_attr *,void *token){
  if(!mt_callback_current(handle,token))return 0;
  if(err->status){transport_failure("Meshtastic FromNum subscription failed");
    return 0;
  }
  struct ble_gap_conn_desc desc={};
  if(ble_gap_conn_find(handle,&desc)){transport_failure("Meshtastic peer identity unavailable");
    return 0;
  }
  uint8_t identity[7];
  identity[0]=desc.peer_id_addr.type&1;
  memcpy(identity+1,desc.peer_id_addr.val,6);
  mt_transport_started=true;
  mt_io_busy=false;
  mt_need_read=true;
  mt_notifications=0;
  mt_last_read=0;
  meshmt::transport_up(identity,ble_att_mtu(handle));
  update(READY,"Meshtastic PhoneAPI ready; synchronizing...");
  save_remembered();
  return 0;
}
static int mt_descriptor(uint16_t handle,const struct ble_gatt_error *err,uint16_t,const struct ble_gatt_dsc *dsc,void *token){
  if(!mt_callback_current(handle,token))return 0;
  if(!err->status&&dsc){if(ble_uuid_u16(&dsc->uuid.u)==0x2902)mt_cccd=dsc->handle;
    return 0;
  }
  if(err->status!=BLE_HS_EDONE||!mt_cccd){transport_failure("Meshtastic FromNum CCCD missing");
    return 0;
  }
  const uint8_t enable[2]={1,0};
  if(ble_gattc_write_flat(handle,mt_cccd,enable,2,mt_subscribed,token))transport_failure("Cannot subscribe to Meshtastic FromNum");
  return 0;
}
static int mt_characteristic(uint16_t handle,const struct ble_gatt_error *err,const struct ble_gatt_chr *chr,void *token){
  if(!mt_callback_current(handle,token))return 0;
  if(!err->status&&chr){
    if(mt_num&&chr->def_handle>mt_num&&(!mt_num_end||chr->def_handle-1<mt_num_end))mt_num_end=chr->def_handle-1;
    if(ble_uuid_cmp(&chr->uuid.u,&MT_TO.u)==0&&(chr->properties&BLE_GATT_CHR_F_WRITE))mt_to=chr->val_handle;
    if(ble_uuid_cmp(&chr->uuid.u,&MT_FROM.u)==0&&(chr->properties&BLE_GATT_CHR_F_READ))mt_from=chr->val_handle;
    if(ble_uuid_cmp(&chr->uuid.u,&MT_NUM.u)==0&&(chr->properties&BLE_GATT_CHR_F_NOTIFY)&&(chr->properties&BLE_GATT_CHR_F_READ)){mt_num=chr->val_handle;
      mt_num_end=mt_end;
    }
    return 0;
  }
  if(err->status!=BLE_HS_EDONE||!mt_to||!mt_from||!mt_num||mt_num_end<=mt_num){transport_failure("Meshtastic ToRadio/FromRadio/FromNum incompatible");
    return 0;
  }
  if(ble_gattc_disc_all_dscs(handle,mt_num,mt_num_end,mt_descriptor,token))transport_failure("Meshtastic descriptor discovery failed");
  return 0;
}
static int mt_mtu(uint16_t handle,const struct ble_gatt_error *err,uint16_t mtu,void *token){
  if(!mt_callback_current(handle,token))return 0;
  // Official Meshtastic BLE devices negotiate different ATT MTUs.
  // Long characteristic reads/writes keep the protobuf framing intact.
  if(err->status||mtu<23){transport_failure("Meshtastic ATT MTU exchange failed; reconnect radio.");
    return 0;
  }
  printf("[MCX-MT] negotiated ATT MTU=%u; using long GATT operations as needed\n",(unsigned)mtu);
  if(ble_gattc_disc_all_chrs(handle,mt_start,mt_end,mt_characteristic,token))transport_failure("Meshtastic characteristic discovery failed");
  return 0;
}
static void start_meshtastic(){
  if(identified!=MESHTASTIC||mt_transport_started)return;
  update(VERIFYING,"Meshtastic: negotiating GATT PhoneAPI...");
  if(ble_gattc_exchange_mtu(conn,mt_mtu,mt_token()))transport_failure("Meshtastic MTU exchange failed");
}
static int service_cb(uint16_t handle,const struct ble_gatt_error *err,
                      const struct ble_gatt_svc *service,void *){
  if(handle!=conn)return 0;
  if(err->status==0&&service){
    if(ble_uuid_cmp(&service->uuid.u,&MT_SERVICE.u)==0){identified=MESHTASTIC;mt_start=service->start_handle;mt_end=service->end_handle;}
    if(ble_uuid_cmp(&service->uuid.u,&MC_SERVICE.u)==0){identified=MESHCORE;mc_start=service->start_handle;mc_end=service->end_handle;}
    return 0;
  }
  if(err->status==BLE_HS_EDONE){
    if(mt_start&&mc_start){transport_failure("Ambiguous dual-protocol GATT. Use one official radio firmware.");return 0;}
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
      if(index<0 && connectable && (fields.name_len>0 || hint!=UNKNOWN || (auto_scan&&matching_remembered(disc.addr))) &&
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
      if(auto_scan.load()&&connectable&&index>=0&&!auto_paused.load()&&
         matching_remembered(disc.addr)){
        auto_candidate.store(index);
        (void)ble_gap_disc_cancel(); // Wait for DISC_COMPLETE before connecting.
      }
      return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:{
      const int candidate=auto_candidate.exchange(-1);
      const bool resume=auto_scan.exchange(false)&&!auto_paused.load();
      if(snapshot().phase==SCANNING)update(IDLE,"Scan complete - choose a device");
      if(resume&&candidate>=0)connect(candidate);
      return 0;
    }
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
        update(PAIRING,"Enter the six-digit PIN shown on your radio");
      }else if(event->passkey.params.action==BLE_SM_IOACT_NUMCMP){
        portENTER_CRITICAL(&mux);
        state.compare_number=event->passkey.params.numcmp;
        state.confirm_required=true;state.generation++;
        portEXIT_CRITICAL(&mux);
        update(PAIRING,"Compare six-digit number on your radio");
      }else{
        update(FAILED,"Unsupported pairing method");
        (void)ble_gap_terminate(event->passkey.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if(event->enc_change.status==0){
        portENTER_CRITICAL(&mux);
        state.paired=true;state.bonded=false;state.pin_required=false;state.confirm_required=false;
        state.generation++;
        portEXIT_CRITICAL(&mux);
        struct ble_gap_conn_desc desc={};
        if(ble_gap_conn_find(conn,&desc)||!desc.sec_state.encrypted||!desc.sec_state.authenticated){
          transport_failure("PIN pairing required: enable RANDOM_PIN / FIXED_PIN on radio");return 0;
        }
        portENTER_CRITICAL(&mux);state.bonded=desc.sec_state.bonded;state.generation++;portEXIT_CRITICAL(&mux);
        if(identified==MESHCORE)start_meshcore();
        else if(identified==MESHTASTIC)start_meshtastic();
        else {update(READY,"BLE secure link / GATT verified");save_remembered();}
      }else{
        char msg[64];snprintf(msg,sizeof(msg),"Pairing failed: %d",event->enc_change.status);
        update(FAILED,msg);
        (void)ble_gap_terminate(event->enc_change.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:{
      mt_down();
      conn=BLE_HS_CONN_HANDLE_NONE;mc_transport_started=false;meshmc::transport_down();
      const Phase before=snapshot().phase;
      portENTER_CRITICAL(&mux);
      state.paired=false;state.bonded=false;state.pin_required=false;state.confirm_required=false;
      state.generation++;
      portEXIT_CRITICAL(&mux);
      if(before!=FAILED)update(IDLE,"Radio disconnected");
      return 0;
    }
    case BLE_GAP_EVENT_NOTIFY_RX:{
      if(event->notify_rx.conn_handle==conn&&event->notify_rx.attr_handle==mt_num&&mt_transport_started){
        if(OS_MBUF_PKTLEN(event->notify_rx.om)!=4){transport_failure("Invalid Meshtastic FromNum notification");return 0;}
        mt_notifications.fetch_add(1);mt_need_read=true;return 0;
      }
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
// Separate SDK stages: nimble_port_init() collapses HCI allocation failures
// into ESP_FAIL. Keep the actual stage visible; never retry partial host init.
static const char *init_stage="not started";
static esp_err_t initialize_stack(){
  init_stage="controller init";
  esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_err_t rc=esp_bt_controller_init(&cfg);if(rc!=ESP_OK)return rc;
  init_stage="controller enable";rc=esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if(rc!=ESP_OK){(void)esp_bt_controller_deinit();return rc;}
  init_stage="host / HCI";rc=esp_nimble_init();
  if(rc!=ESP_OK){(void)esp_bt_controller_disable();(void)esp_bt_controller_deinit();return rc;}
  init_stage="host ready";return ESP_OK;
}
static void start_internal(){
  printf("[MCX-BLE] worker entered; heap=%lu\n",(unsigned long)esp_get_free_heap_size());
  debug_memory("before nimble");
  update(STARTING,"Initializing NimBLE in worker...");
  const esp_err_t err=initialize_stack();
  printf("[MCX-BLE] init stage=%s result=%s (%d) heap=%lu\n",init_stage,esp_err_to_name(err),(int)err,(unsigned long)esp_get_free_heap_size());
  debug_memory("after nimble init");
  if(err!=ESP_OK){
    mcxhistory::ble_startup.store(false);
    char msg[96];snprintf(msg,sizeof(msg),"BLE %s: %s (%d); restart",init_stage,esp_err_to_name(err),(int)err);
    update(FAILED,msg);return;
  }
  (void)ble_att_set_preferred_mtu(517);
  ble_hs_cfg.reset_cb=on_reset;
  ble_hs_cfg.sync_cb=on_sync;
  ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
  ble_hs_cfg.sm_io_cap=BLE_HS_IO_KEYBOARD_ONLY;
  ble_hs_cfg.sm_bonding=1;
  ble_hs_cfg.sm_mitm=1;
  ble_hs_cfg.sm_sc=1;
  ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  (void)ble_svc_gap_device_name_set("MeshCrossway");
  ble_store_config_init();
  debug_memory("before host task");
  nimble_port_freertos_init(host_task);
  remember_enabled();
  mcxhistory::ble_startup.store(false);
  printf("[MCX-BLE] host created; awaiting sync\n");
}
static void init_task(void *){
  // Never initialize controller/host in LVGL or the early app_main boot path.
  // A short delay allows LVGL, the Wi-Fi AP/STA and HTTP to become responsive.
  vTaskDelay(pdMS_TO_TICKS(500));
  // In-flight HTTPS releases TLS/socket allocations before controller/HCI init.
  for(unsigned wait=0;mcxpriority::map_busy.load()&&wait<80;++wait)vTaskDelay(pdMS_TO_TICKS(50));
  if(mcxpriority::map_busy.load()){
    mcxhistory::ble_startup.store(false);mcxpriority::ble_initializing=false;
    update(FAILED,"Map worker busy; restart before BLE");vTaskDelete(nullptr);return;
  }
  start_internal();mcxpriority::ble_initializing=false;
  if(read_remembered()){
    RememberedPeer peer;portENTER_CRITICAL(&mux);peer=remembered;portEXIT_CRITICAL(&mux);
    if(peer.radio==MESHTASTIC){uint8_t id[7];id[0]=peer.addr_type&1;memcpy(id+1,peer.addr,6);meshmt::request_offline(id);}
  }
  vTaskDelete(nullptr);
}
static bool request_start(){
  if(snapshot().phase!=OFF)return false; // Do not repeat a partial or failed init.
  mcxhistory::ble_startup.store(true);
  if(!meshmc::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for MeshCore session");return false;}
  if(!meshmt::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for Meshtastic session");return false;}
  meshmc::send_frame=mc_write;meshmc::close_link=mc_close;
  meshmt::send_frame=mt_write;meshmt::can_write=mt_can_write;meshmt::pump_read=mt_pump_read;meshmt::close_link=mt_close;
  mcxpriority::ble_initializing=true;
  debug_memory("BLE requested");
  update(STARTING,"BLE worker requested - standby...");
  const BaseType_t created=xTaskCreatePinnedToCore(init_task,"mcx_ble_init",6144,nullptr,tskIDLE_PRIORITY+1,nullptr,1);
  if(created!=pdPASS){
    mcxhistory::ble_startup.store(false);
    mcxpriority::ble_initializing=false;
    update(FAILED,"No memory for BLE worker; restart");return false;
  }
  return true;
}
static void auto_resume_task(void *){
  // Keep BLE completely OFF while LVGL, AP+STA and HTTP initialize.
  vTaskDelay(pdMS_TO_TICKS(12000));
  const bool paired_before=read_remembered();
  if((!paired_before&&!read_enabled())||auto_paused.load()){
    reconnect_worker=nullptr;vTaskDelete(nullptr);return;
  }
  (void)request_start();
  bool first=true;
  while(!auto_paused.load()){
    vTaskDelay(pdMS_TO_TICKS(first?3000:30000));
    first=false;
    const Snapshot current=snapshot();
    if(paired_before&&synced&&conn==BLE_HS_CONN_HANDLE_NONE&&
       (current.phase==IDLE||current.phase==FAILED))scan(true);
  }
  reconnect_worker=nullptr;vTaskDelete(nullptr);
}
static void schedule_auto_resume(){
  // An unpaired new Core never enables BLE on its own.
  if(reconnect_worker)return;
  (void)xTaskCreate(auto_resume_task,"mcx_ble_resume",4096,nullptr,
                    tskIDLE_PRIORITY+1,&reconnect_worker);
}
} // namespace meshble

