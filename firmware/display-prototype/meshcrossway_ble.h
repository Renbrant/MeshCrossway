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
// One controller; two independent link instances. GAP discovery/connection setup
// is serialized, while established GATT transports run concurrently.
static std::atomic<Radio> procedure{UNKNOWN},focus{MESHTASTIC};
static std::atomic<uint32_t> revision{0};
static void debug_memory(const char *phase);
static bool claim(Radio radio){Radio empty=UNKNOWN;return procedure.compare_exchange_strong(empty,radio);}
static void release(Radio radio){Radio expected=radio;procedure.compare_exchange_strong(expected,UNKNOWN);}
static const char *protocol_name(Radio r){return r==MESHTASTIC?"Meshtastic":r==MESHCORE?"MeshCore":"Unknown";}
} // namespace meshble
#define MCX_BLE_SLOT mt
#define MCX_BLE_RADIO MESHTASTIC
#include "meshcrossway_ble_link.inc"
#undef MCX_BLE_SLOT
#undef MCX_BLE_RADIO
#define MCX_BLE_SLOT mc
#define MCX_BLE_RADIO MESHCORE
#include "meshcrossway_ble_link.inc"
#undef MCX_BLE_SLOT
#undef MCX_BLE_RADIO
namespace meshble {
static Radio selected(){return focus.load();}
static bool select(Radio r){
  if(r!=MESHTASTIC&&r!=MESHCORE)return false;
  // Keep PIN entry bound to the link that requested it.
  const Radio busy=procedure.load();if(busy!=UNKNOWN&&busy!=r)return false;
  focus=r;++revision;return true;
}
static Snapshot snapshot(Radio r){auto s=r==MESHCORE?mc::snapshot():mt::snapshot();s.generation=revision.load();s.radio=r;return s;}
static Snapshot snapshot(){return snapshot(selected());}
static Phase phase(){return selected()==MESHCORE?mc::phase():mt::phase();}
static void scan(bool automatic=false){if(selected()==MESHCORE)mc::scan(automatic);else mt::scan(automatic);}
static void connect(int i){if(selected()==MESHCORE)mc::connect(i);else mt::connect(i);}
static void connect_peer(const Peer &p){if(selected()==MESHCORE)mc::connect_peer(p);else mt::connect_peer(p);}
static void disconnect(Radio r){if(r==MESHCORE)mc::disconnect();else mt::disconnect();}
static void disconnect(){disconnect(selected());}
static bool submit_pin(Radio r,uint32_t pin){return r==MESHCORE?mc::submit_pin(pin):mt::submit_pin(pin);}
static bool submit_pin(uint32_t pin){return submit_pin(selected(),pin);}
static bool confirm_number(Radio r,bool ok){return r==MESHCORE?mc::confirm_number(ok):mt::confirm_number(ok);}
static bool confirm_number(bool ok){return confirm_number(selected(),ok);}
static bool read_enabled(){return mt::read_enabled();}
static bool read_remembered(){const bool a=mt::read_remembered(),b=mc::read_remembered();return a||b;}
static void restore_offline(){if(mt::read_remembered()){
  uint8_t id[7];portENTER_CRITICAL(&mt::mux);id[0]=mt::remembered.addr_type&1;memcpy(id+1,mt::remembered.addr,6);portEXIT_CRITICAL(&mt::mux);meshmt::request_offline(id);
}}
static void update(Phase p,const char *s){mt::update(p,s);mc::update(p,s);}
static void on_sync(){mt::on_sync();mc::on_sync();}
static void on_reset(int reason){mt::on_reset(reason);mc::on_reset(reason);}
static void host_task(void *){nimble_port_run();nimble_port_freertos_deinit();}
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
  mt::remember_enabled();
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
  read_remembered();restore_offline();
  vTaskDelete(nullptr);
}
static void schedule_auto_resume();
static bool request_start(){
  if(mt::phase()!=OFF||mc::phase()!=OFF)return false; // Do not repeat a partial or failed init.
  mcxhistory::ble_startup.store(true);
  if(!meshmc::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for MeshCore session");return false;}
  if(!meshmt::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for Meshtastic session");return false;}
  meshmc::send_frame=mc::mc_write;meshmc::close_link=mc::mc_close;
  meshmt::send_frame=mt::mt_write;meshmt::can_write=mt::mt_can_write;meshmt::pump_read=mt::mt_pump_read;meshmt::close_link=mt::mt_close;
  mcxpriority::ble_initializing=true;
  debug_memory("BLE requested");
  update(STARTING,"BLE worker requested - standby...");
  const BaseType_t created=xTaskCreatePinnedToCore(init_task,"mcx_ble_init",6144,nullptr,tskIDLE_PRIORITY+1,nullptr,1);
  if(created!=pdPASS){
    mcxhistory::ble_startup.store(false);
    mcxpriority::ble_initializing=false;
    update(FAILED,"No memory for BLE worker; restart");return false;
  }
  schedule_auto_resume();
  return true;
}
static constexpr unsigned RESUME_STACK_BYTES=6144;
static void resume_stack_diagnostic(){
  // ESP-IDF reports bytes, unlike upstream FreeRTOS's word-based API.
  printf("[MCX-BLE] resume stack_free_min=%lu bytes / %u\n",
         (unsigned long)uxTaskGetStackHighWaterMark(nullptr),RESUME_STACK_BYTES);
}
static TaskHandle_t reconnect_worker=nullptr;
static void auto_resume_task(void *){
  vTaskDelay(pdMS_TO_TICKS(12000));
  const bool known=read_remembered();
  if(!known&&!read_enabled()){reconnect_worker=nullptr;vTaskDelete(nullptr);return;}
  (void)request_start();
  bool alternate=false;
  for(;;){
    vTaskDelay(pdMS_TO_TICKS(3000));
    if(procedure.load()!=UNKNOWN)continue;
    // Round-robin ensures an absent remembered radio cannot starve the other.
    alternate=!alternate;
    if(alternate)mt::resume_once();else mc::resume_once();
    resume_stack_diagnostic();
  }
}
static void schedule_auto_resume(){
  if(reconnect_worker)return;
  (void)xTaskCreate(auto_resume_task,"mcx_ble_resume",RESUME_STACK_BYTES,nullptr,tskIDLE_PRIORITY+1,&reconnect_worker);
}
} // namespace meshble
