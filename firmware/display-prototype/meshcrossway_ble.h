#pragma once
#include "meshcrossway_memory_log.h"
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
static std::atomic<bool> stop_requested{false},host_task_running{false},host_task_requested{false},host_stack_initialized{false};
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
static void remember_disabled(){
  nvs_handle_t h;if(nvs_open("mcx_ble",NVS_READWRITE,&h)!=ESP_OK)return;
  const esp_err_t rc=nvs_set_u8(h,"enabled",0)==ESP_OK?nvs_commit(h):ESP_FAIL;nvs_close(h);
  if(rc!=ESP_OK)printf("[MCX-BLE] disabled preference not saved err=%d\n",(int)rc);
}
static bool read_remembered(){const bool a=mt::read_remembered(),b=mc::read_remembered();return a||b;}
static void restore_offline(){if(mt::read_remembered()){
  uint8_t id[7];portENTER_CRITICAL(&mt::mux);id[0]=mt::remembered.addr_type&1;memcpy(id+1,mt::remembered.addr,6);portEXIT_CRITICAL(&mt::mux);meshmt::request_offline(id);
}}
static void update(Phase p,const char *s){mt::update(p,s);mc::update(p,s);}
static void on_sync(){
  printf("[MCX-BLE] NimBLE controller synchronized stack_free=%u\n",(unsigned)uxTaskGetStackHighWaterMark(nullptr));
  fflush(stdout);
  mt::on_sync();mc::on_sync();
}
static void on_reset(int reason){mt::on_reset(reason);mc::on_reset(reason);}
static void host_task(void *){
  host_task_running.store(true);
  printf("[MCX-BLE] NimBLE host task entered core=%d stack_free=%u\n",(int)xPortGetCoreID(),(unsigned)uxTaskGetStackHighWaterMark(nullptr));
  fflush(stdout);
  nimble_port_run();
  printf("[MCX-BLE] NimBLE host task returned; shutting down\n");
  fflush(stdout);
  host_task_running.store(false);
  nimble_port_freertos_deinit();
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
// Bring up the memory-hungry controller before Wi-Fi fragments internal DRAM.
// v0.9.3 logs showed only a 25-26 KB largest block after Wi-Fi, while the
// BLE controller requested 30-34 KB and asserted instead of returning ENOMEM.
static const char *init_stage="not started";
static std::atomic<bool> controller_ready{false},controller_enabled{false},controller_prepare_failed{false};
static std::atomic<int> controller_prepare_error{0};
static void prepare_controller_early(){
  if(controller_ready.load()||controller_prepare_failed.load())return;
  const uint32_t largest=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(largest<48*1024){
    controller_prepare_error.store(ESP_ERR_NO_MEM);controller_prepare_failed.store(true);
    printf("[MCX-BLE] early controller skipped: largest internal block=%lu; need at least 49152 bytes\n",(unsigned long)largest);
    return;
  }
  esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  init_stage="early controller init";
  esp_err_t rc=esp_bt_controller_init(&cfg);
  if(rc==ESP_OK){init_stage="early controller enable";rc=esp_bt_controller_enable(ESP_BT_MODE_BLE);}
  if(rc!=ESP_OK){
    controller_prepare_error.store((int)rc);controller_prepare_failed.store(true);
    if(esp_bt_controller_get_status()!=ESP_BT_CONTROLLER_STATUS_IDLE){
      (void)esp_bt_controller_disable();(void)esp_bt_controller_deinit();
    }
    controller_enabled.store(false);
    printf("[MCX-BLE] %s failed: %s (%d); BLE auto-start disabled\n",init_stage,esp_err_to_name(rc),(int)rc);
    return;
  }
  controller_enabled.store(true);controller_ready.store(true);
  printf("[MCX-BLE] controller prepared before Wi-Fi; internal largest=%lu\n",
         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}
static esp_err_t initialize_stack(){
  if(controller_prepare_failed.load())return (esp_err_t)controller_prepare_error.load();
  if(!controller_ready.load()){init_stage="controller not prepared before Wi-Fi";return ESP_ERR_INVALID_STATE;}
  if(!controller_enabled.load()){
    init_stage="controller re-enable";
    const esp_err_t enabled=esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if(enabled!=ESP_OK)return enabled;
    controller_enabled.store(true);
  }
  init_stage="host / HCI";esp_err_t rc=esp_nimble_init();
  if(rc!=ESP_OK){
    (void)esp_bt_controller_disable();(void)esp_bt_controller_deinit();
    controller_enabled.store(false);controller_ready.store(false);
    controller_prepare_error.store((int)rc);controller_prepare_failed.store(true);
    return rc;
  }
  init_stage="host ready";return ESP_OK;
}
static void start_internal(){
  printf("[MCX-BLE] worker entered; heap=%lu\n",(unsigned long)esp_get_free_heap_size());
  #if MCX_MEMORY_DIAGNOSTICS
  mcxmem::capture(7);
#endif
  debug_memory("before nimble");
  update(STARTING,"Initializing NimBLE in worker...");
  const esp_err_t err=initialize_stack();
  printf("[MCX-BLE] init stage=%s result=%s (%d) heap=%lu\n",init_stage,esp_err_to_name(err),(int)err,(unsigned long)esp_get_free_heap_size());
  debug_memory("after nimble init");
#if MCX_MEMORY_DIAGNOSTICS
  mcxmem::capture(8,unsigned(err));
#endif
  if(err!=ESP_OK){
    mcxhistory::ble_startup.store(false);
    mcxpriority::ble_initializing=false;
    char msg[96];snprintf(msg,sizeof(msg),"BLE %s: %s (%d)",init_stage,esp_err_to_name(err),(int)err);
    update(FAILED,msg);return;
  }
  host_stack_initialized.store(true);
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
  host_task_requested.store(true);
  mt::remember_enabled();
  mcxhistory::ble_startup.store(false);
  mcxpriority::ble_initializing=false;
  printf("[MCX-BLE] host task requested; waiting for controller sync\n");
  fflush(stdout);
}
static constexpr unsigned RESUME_STACK_BYTES=6144;
static TaskHandle_t reconnect_worker=nullptr;
static std::atomic<bool> start_requested{false};
static void resume_stack_diagnostic(){
  printf("[MCX-BLE] resume stack_free_min=%lu bytes / %u\n",
         (unsigned long)uxTaskGetStackHighWaterMark(nullptr),RESUME_STACK_BYTES);
}
static void schedule_auto_resume();
static void start_on_worker(){
  // Reuse the already allocated coordinator stack; a second 6 KB task cannot
  // be allocated after the BLE controller and Wi-Fi claim internal DRAM.
  for(unsigned wait=0;mcxpriority::map_busy.load()&&wait<80;++wait)vTaskDelay(pdMS_TO_TICKS(50));
  if(mcxpriority::map_busy.load()){
    mcxhistory::ble_startup.store(false);mcxpriority::ble_initializing=false;
    update(FAILED,"Map worker busy; BLE startup deferred");return;
  }
  start_internal();
  read_remembered();restore_offline();
}

static bool request_start_impl(bool run_on_worker){
  if(controller_prepare_failed.load()){
    mcxhistory::ble_startup.store(false);
    update(FAILED,"BLE startup skipped: controller memory unavailable; device kept running");
    return false;
  }
  if(mt::phase()!=OFF||mc::phase()!=OFF)return false;
  mcxhistory::ble_startup.store(true);
  if(!meshmc::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for MeshCore session");return false;}
  if(!meshmt::init()){mcxhistory::ble_startup.store(false);update(FAILED,"No PSRAM for Meshtastic session");return false;}
  meshmc::send_frame=mc::mc_write;meshmc::close_link=mc::mc_close;
  meshmt::send_frame=mt::mt_write;meshmt::can_write=mt::mt_can_write;meshmt::pump_read=mt::mt_pump_read;meshmt::close_link=mt::mt_close;
  mcxpriority::ble_initializing=true;
  debug_memory("BLE requested");
  update(STARTING,run_on_worker?"Starting BLE on coordinator task...":"BLE start queued...");
  if(run_on_worker){start_on_worker();return true;}
  if(!reconnect_worker)schedule_auto_resume();
  if(!reconnect_worker){
    mcxhistory::ble_startup.store(false);mcxpriority::ble_initializing=false;
    update(FAILED,"No memory for BLE coordinator task");
    return false;
  }
  start_requested.store(true);
  xTaskNotifyGive(reconnect_worker);
  return true;
}
static bool request_start(){return request_start_impl(false);}
static bool power_off_internal(){
  remember_disabled();
  mt::disconnect();mc::disconnect();
  if(host_task_requested.load()&&!host_task_running.load())
    for(unsigned i=0;i<40&&!host_task_running.load();++i)vTaskDelay(pdMS_TO_TICKS(50));
  if(host_task_requested.load()&&!host_task_running.load()){
    update(FAILED,"NimBLE host did not start; Bluetooth left on");
    printf("[MCX-BLE] refusing shutdown: requested NimBLE host task never entered\n");fflush(stdout);
    return false;
  }
  if(host_task_running.load()){
    for(unsigned i=0;i<100&&(mt::conn!=BLE_HS_CONN_HANDLE_NONE||mc::conn!=BLE_HS_CONN_HANDLE_NONE);++i)
      vTaskDelay(pdMS_TO_TICKS(50));
    if(mt::conn!=BLE_HS_CONN_HANDLE_NONE||mc::conn!=BLE_HS_CONN_HANDLE_NONE){
      update(FAILED,"Disconnect timeout; Bluetooth left on");return false;
    }
    const int stop_rc=nimble_port_stop();
    if(stop_rc!=0){char error[64];snprintf(error,sizeof(error),"NimBLE stop error: %d",stop_rc);update(FAILED,error);return false;}
    for(unsigned i=0;i<100&&host_task_running.load();++i)vTaskDelay(pdMS_TO_TICKS(10));
    if(host_task_running.load()){update(FAILED,"NimBLE host did not stop");return false;}
  }
  if(host_stack_initialized.load()){
    const esp_err_t host_rc=esp_nimble_deinit();
    if(host_rc!=ESP_OK){char error[64];snprintf(error,sizeof(error),"NimBLE deinit error: %d",(int)host_rc);update(FAILED,error);return false;}
    host_stack_initialized.store(false);
  }
  host_task_requested.store(false);
  if(controller_enabled.load()){
    const esp_err_t controller_rc=esp_bt_controller_disable();
    if(controller_rc!=ESP_OK){char error[64];snprintf(error,sizeof(error),"Bluetooth disable error: %d",(int)controller_rc);update(FAILED,error);return false;}
    controller_enabled.store(false);
  }
  mt::synced=false;mc::synced=false;mt::conn=BLE_HS_CONN_HANDLE_NONE;mc::conn=BLE_HS_CONN_HANDLE_NONE;
  mt::auto_paused=true;mc::auto_paused=true;procedure=UNKNOWN;
  update(OFF,"Bluetooth disabled");
  printf("[MCX-BLE] NimBLE host and controller disabled\n");fflush(stdout);
  return true;
}
static bool request_stop(){
  if(mt::phase()==OFF&&mc::phase()==OFF)return true;
  if(!reconnect_worker)return false;
  start_requested.store(false);stop_requested.store(true);
  xTaskNotifyGive(reconnect_worker);return true;
}
static void auto_resume_task(void *){
#if MCX_MEMORY_DIAGNOSTICS
  mcxmem::poll();
#endif
  const bool enabled=read_enabled();
  const TickType_t first_wait=enabled?pdMS_TO_TICKS(12000):portMAX_DELAY;
#if MCX_MEMORY_DIAGNOSTICS
  uint32_t notified=0;const TickType_t waiting_started=xTaskGetTickCount();
  do {
    mcxmem::poll();
    notified=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
  }while(!notified&&(!enabled||TickType_t(xTaskGetTickCount()-waiting_started)<first_wait));
#else
  const uint32_t notified=ulTaskNotifyTake(pdTRUE,first_wait);
#endif
  if(notified&&start_requested.exchange(false))start_on_worker();
  else if(enabled)(void)request_start_impl(true);
  bool alternate=false;
  for(;;){
#if MCX_MEMORY_DIAGNOSTICS
    mcxmem::poll();
#endif
    const uint32_t wake=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(3000));
    if(stop_requested.exchange(false)){(void)power_off_internal();alternate=false;continue;}
    if((wake||start_requested.load())&&start_requested.exchange(false)){
      start_on_worker();alternate=false;
    }
    if(procedure.load()!=UNKNOWN)continue;
    alternate=!alternate;
    if(alternate)mt::resume_once();else mc::resume_once();
    resume_stack_diagnostic();
  }
}
static void schedule_auto_resume(){
  if(reconnect_worker)return;
  // Keep this small internal-RAM task alive so manual pairing and auto-resume
  // share its stack instead of allocating another task during BLE startup.
  (void)xTaskCreate(auto_resume_task,"mcx_ble_resume",RESUME_STACK_BYTES,nullptr,tskIDLE_PRIORITY+1,&reconnect_worker);
}
} // namespace meshble
