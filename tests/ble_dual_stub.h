#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
using esp_err_t=int;using nvs_handle_t=int;using TaskHandle_t=void*;using BaseType_t=int;using TickType_t=uint32_t;using esp_bt_controller_config_t=int;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
#define pdMS_TO_TICKS(n) (n)
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() 0
static constexpr int ESP_OK=0,ESP_ERR_NO_MEM=-12,ESP_ERR_INVALID_STATE=-1,NVS_READONLY=0,NVS_READWRITE=1,pdPASS=1,tskIDLE_PRIORITY=0,ESP_BT_MODE_BLE=1,ESP_BT_CONTROLLER_STATUS_IDLE=0;
static constexpr int pdTRUE=1;
static constexpr uint32_t portMAX_DELAY=0xffffffffu;
static constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_DMA=4;
static constexpr int BLE_HS_CONN_HANDLE_NONE=65535,BLE_HS_EDONE=14,BLE_ERR_REM_USER_CONN_TERM=19;
static constexpr int BLE_GATT_CHR_F_WRITE=8,BLE_GATT_CHR_F_NOTIFY=16;
static constexpr int BLE_GATT_CHR_F_READ=2,BLE_SM_IOACT_INPUT=1,BLE_SM_IOACT_NUMCMP=2,BLE_HS_IO_KEYBOARD_ONLY=2,BLE_SM_PAIR_KEY_DIST_ENC=1,BLE_SM_PAIR_KEY_DIST_ID=2;
enum {BLE_GAP_EVENT_DISC=1,BLE_GAP_EVENT_DISC_COMPLETE,BLE_GAP_EVENT_LINK_ESTAB,BLE_GAP_EVENT_PASSKEY_ACTION,BLE_GAP_EVENT_ENC_CHANGE,BLE_GAP_EVENT_DISCONNECT,BLE_GAP_EVENT_NOTIFY_RX};
static constexpr int BLE_HCI_ADV_RPT_EVTYPE_ADV_IND=0,BLE_HCI_ADV_RPT_EVTYPE_DIR_IND=1;
struct ble_addr_t {uint8_t type=0,val[6]={};};
struct ble_uuid_t {uint8_t bytes[16];};struct ble_uuid128_t {ble_uuid_t u;};
#define BLE_UUID128_INIT(...) {{__VA_ARGS__}}
static int ble_uuid_cmp(const ble_uuid_t *a,const ble_uuid_t *b){return memcmp(a,b,sizeof(*a));}
static uint16_t ble_uuid_u16(const ble_uuid_t *a){return uint16_t(a->bytes[0])|(uint16_t(a->bytes[1])<<8);}
struct ble_gap_conn_desc {uint16_t conn_handle=0;ble_addr_t peer_id_addr;struct {bool encrypted=true,authenticated=true,bonded=true;}sec_state;};
struct ble_gap_disc_desc {ble_addr_t addr;int event_type=0;uint8_t *data=nullptr;unsigned length_data=0;int8_t rssi=-40;};
struct ble_hs_adv_fields {uint8_t num_uuids128=0,name_len=0;ble_uuid128_t *uuids128=nullptr;uint8_t *name=nullptr;};
struct ble_gap_disc_params {int passive,filter_duplicates,itvl,window;};
struct os_mbuf {uint8_t data[512]={};unsigned len=0;};
#define OS_MBUF_PKTLEN(p) ((p)->len)
struct ble_gap_event {
 int type=0;ble_gap_disc_desc disc;
 struct {int status=0;uint16_t conn_handle=0;}connect,enc_change;
 struct {uint16_t conn_handle=0;struct{int action=0;uint32_t numcmp=0;}params;}passkey;
 struct {ble_gap_conn_desc conn;}disconnect;
 struct {uint16_t conn_handle=0,attr_handle=0;os_mbuf *om=nullptr;}notify_rx;
};
struct ble_gatt_error{int status=0;};struct ble_gatt_attr{os_mbuf *om=nullptr;unsigned offset=0;};
struct ble_gatt_dsc{ble_uuid128_t uuid;uint16_t handle;};
struct ble_gatt_chr{ble_uuid128_t uuid;uint16_t def_handle=0,val_handle=0;unsigned properties=0;};
struct ble_gatt_svc{ble_uuid128_t uuid;uint16_t start_handle=1,end_handle=100;};
struct ble_sm_io{int action=0;uint32_t passkey=0;int numcmp_accept=0;};
struct {void(*reset_cb)(int)=nullptr;void(*sync_cb)()=nullptr;int(*store_status_cb)(void*)=nullptr;int sm_io_cap=0,sm_bonding=0,sm_mitm=0,sm_sc=0,sm_our_key_dist=0,sm_their_key_dist=0;}ble_hs_cfg;
static std::map<std::string,std::vector<uint8_t>> nvs;
static ble_addr_t dialing;static int dial_count=0,write_handle=-1,pin_handle=-1,terminate_handle=-1;static uint32_t clock_ms=15000;
static ble_gap_conn_desc descriptions[2];
static int nvs_open(const char*,int,nvs_handle_t *h){*h=1;return 0;}
static int nvs_get_blob(int,const char *key,void *p,size_t *n){auto i=nvs.find(key);if(i==nvs.end())return -1;if(*n<i->second.size())return -2;*n=i->second.size();memcpy(p,i->second.data(),*n);return 0;}
static int nvs_set_blob(int,const char *key,const void *p,size_t n){nvs[key]=std::vector<uint8_t>((const uint8_t*)p,(const uint8_t*)p+n);return 0;}
static int nvs_get_u8(int,const char *key,uint8_t *v){size_t n=1;return nvs_get_blob(1,key,v,&n);}
static int nvs_set_u8(int,const char *key,uint8_t v){return nvs_set_blob(1,key,&v,1);}
static int nvs_commit(int){return 0;}static void nvs_close(int){}
static int ble_gap_conn_find(uint16_t h,ble_gap_conn_desc *out){if(h>1)return -1;*out=descriptions[h];return 0;}
static int ble_sm_inject_io(uint16_t h,ble_sm_io*){pin_handle=h;return 0;}
static int ble_gap_terminate(uint16_t h,int){terminate_handle=h;return 0;}
static int ble_gap_connect(uint8_t,const ble_addr_t *a,int,void*,int(*)(ble_gap_event*,void*),void*){dialing=*a;++dial_count;return 0;}
static int ble_gap_disc_cancel(){return 0;}static int ble_gap_conn_cancel(){return 0;}
static int ble_gap_disc(uint8_t,int,ble_gap_disc_params*,int(*)(ble_gap_event*,void*),void*){return 0;}
static int ble_hs_adv_parse_fields(ble_hs_adv_fields*,uint8_t*,unsigned){return 0;}
static int ble_hs_util_ensure_addr(int){return 0;}static int ble_hs_id_infer_auto(int,uint8_t *a){*a=0;return 0;}
static int ble_att_mtu(uint16_t){return 517;}static int ble_att_set_preferred_mtu(int){return 0;}
static int ble_gap_security_initiate(uint16_t){return 0;}
template<class... A>static int ble_gattc_disc_all_svcs(uint16_t,A...){return 0;}
template<class... A>static int ble_gattc_disc_all_chrs(A...){return 0;}
template<class... A>static int ble_gattc_disc_all_dscs(A...){return 0;}
template<class... A>static int ble_gattc_exchange_mtu(uint16_t,A...){return 0;}
template<class... A>static int ble_gattc_read_long(uint16_t,A...){return 0;}
template<class... A>static int ble_gattc_write_flat(uint16_t h,A...){write_handle=h;return 0;}
template<class... A>static int ble_gattc_write_long(uint16_t h,A...){write_handle=h;return 0;}
static os_mbuf *ble_hs_mbuf_from_flat(const void*,size_t){static os_mbuf b;return &b;}
static int os_mbuf_copydata(os_mbuf *p,unsigned o,unsigned n,void *out){if(o+n>p->len)return -1;memcpy(out,p->data+o,n);return 0;}
static int64_t esp_timer_get_time(){return int64_t(clock_ms)*1000;}
static size_t heap_caps_get_free_size(int){return 100000;}static size_t heap_caps_get_largest_free_block(int){return 50000;}
static unsigned uxTaskGetStackHighWaterMark(void*){return 5000;}static unsigned esp_get_free_heap_size(){return 200000;}static int xPortGetCoreID(){return 1;}
static int esp_bt_controller_get_status(){return ESP_BT_CONTROLLER_STATUS_IDLE;}static int esp_bt_controller_init(int*){return 0;}static int esp_bt_controller_enable(int){return 0;}static int esp_bt_controller_disable(){return 0;}static int esp_bt_controller_deinit(){return 0;}static int esp_nimble_init(){return 0;}
static const char *esp_err_to_name(int){return "OK";}static void nimble_port_run(){}static void nimble_port_freertos_deinit(){}static void nimble_port_freertos_init(void(*)(void*)){}
static int ble_store_util_status_rr(void*){return 0;}static int ble_svc_gap_device_name_set(const char*){return 0;}
extern "C" void ble_store_config_init(){}
static void vTaskDelay(unsigned){}static void vTaskDelete(void*){}
static int xTaskCreate(void (*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t *created){*created=reinterpret_cast<TaskHandle_t>(uintptr_t(1));return pdPASS;}
static uint32_t ulTaskNotifyTake(unsigned,uint32_t){return 0;}static unsigned notify_count=0;static int xTaskNotifyGive(TaskHandle_t){++notify_count;return pdPASS;}
template<class...A>static int xTaskCreatePinnedToCore(A...){return pdPASS;}
namespace mcxhistory{static std::atomic<bool> ble_startup{false};}
namespace mcxpriority{static std::atomic<bool> map_busy{false},ble_initializing{false};}
namespace meshmc{static constexpr unsigned MAX_FRAME=176;static int downs=0,ups=0,frames=0,writes=0;static std::atomic<bool> overflow{false};static bool(*send_frame)(const uint8_t*,size_t)=nullptr;static void(*close_link)()=nullptr;static bool init(){return true;}static void transport_down(){++downs;}static void transport_up(int){++ups;}static void on_frame(const uint8_t*,size_t){++frames;}static void on_write(bool){++writes;}}
namespace meshmt{static constexpr unsigned MAX_FRAME=512;struct{struct{unsigned len=0;uint8_t data[512]={};}reading;}buffer;static auto *buffers=&buffer;struct{char status[100]="status";}model;static auto *client=&model;
static int downs=0,ups=0,frames=0,writes=0;static bool(*send_frame)(const uint8_t*,size_t)=nullptr;static bool(*can_write)()=nullptr;static void(*pump_read)()=nullptr;static void(*close_link)()=nullptr;
static bool init(){return true;}static void transport_down(){++downs;}static void transport_up(uint8_t*,int){++ups;}static bool on_frame(const uint8_t*,size_t){++frames;return true;}static void on_write(bool){++writes;}static void request_offline(uint8_t*){}}
