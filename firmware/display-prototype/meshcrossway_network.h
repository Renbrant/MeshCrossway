#pragma once
// MeshCrossway v0.7.2 Wi-Fi: reconnect most recently successful known network,
// then fall back to up to four other saved WPA2/WPA3-personal profiles.
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "nvs.h"
#include "meshcrossway_backup.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace meshnetwork {
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static char local_ip[16]="";
static char wifi_state[30]="Wi-Fi not configured";
static char ssid_name[33]="";
static bool wants_station=false;
static bool station_connected=false;
static bool wifi_started=false;
static unsigned retries=0;
ESP_EVENT_DEFINE_BASE(MCX_CONTROL_EVENT);
enum ControlEvent { CONNECT_MANUAL=1, RETRY_KNOWN=2 };
struct credentials_t {char ssid[33];char password[64];};
// A versioned NVS list, not a copy of the Wi-Fi SDK's single STA slot.
// Passwords stay in NVS / RAM; never put them in HTTP, serial logs, or SD lists.
static constexpr uint32_t WIFI_VAULT_MAGIC=0x4D435857u;
static constexpr uint8_t MAX_PROFILES=5;
struct profile_list_t {
  uint32_t magic;
  uint8_t version;
  uint8_t count;
  uint8_t reserved[2];
  credentials_t entries[MAX_PROFILES];
};
static profile_list_t profiles={};
static credentials_t legacy_profile={}; // first upgrade from single-SSID ESP-IDF NVS or SD

static int active_index=-1;      // -1: manually entered / legacy STA configuration
static unsigned attempted=0;    // profiles attempted in this fallback pass
static bool manual_active=false;
static bool suppress_user_leave=false;
static int64_t suppress_leave_until_us=0;
static esp_timer_handle_t retry_timer=nullptr;
static bool scan_done=false;
static bool scan_running=false;
static char scan_ssids[12][33]={};
static uint16_t scan_count=0;

static bool valid(const credentials_t &p) {
  const size_t s=strnlen(p.ssid,sizeof(p.ssid));
  const size_t n=strnlen(p.password,sizeof(p.password));
  return s>0&&s<=32&&n>=8&&n<=63;
}
static bool same_ssid(const credentials_t &p,const char *ssid){
  return strcmp(p.ssid,ssid)==0;
}
static void load_profiles() {
  profile_list_t incoming={};
  nvs_handle_t h;
  if(nvs_open("mcx_wifi",NVS_READONLY,&h)!=ESP_OK)return;
  size_t length=sizeof(incoming);
  const esp_err_t rc=nvs_get_blob(h,"profiles",&incoming,&length);
  nvs_close(h);
  if(rc!=ESP_OK||length!=sizeof(incoming)||incoming.magic!=WIFI_VAULT_MAGIC||
     incoming.version!=1||incoming.count>MAX_PROFILES)return;
  for(unsigned i=0;i<incoming.count;i++){
    if(!valid(incoming.entries[i]))return;
    for(unsigned j=0;j<i;j++)
      if(same_ssid(incoming.entries[j],incoming.entries[i].ssid))return;
  }
  portENTER_CRITICAL(&guard);profiles=incoming;portEXIT_CRITICAL(&guard);
}
static bool known_network(const char *ssid,credentials_t *out=nullptr){
  if(!ssid||!ssid[0])return false;
  bool found=false;
  portENTER_CRITICAL(&guard);
  for(unsigned i=0;i<profiles.count;i++){
    if(same_ssid(profiles.entries[i],ssid)){
      if(out)*out=profiles.entries[i];
      found=true;break;
    }
  }
  portEXIT_CRITICAL(&guard);
  return found;
}
static void remember_success(const credentials_t &success){
  if(!valid(success))return;
  profile_list_t next={};
  next.magic=WIFI_VAULT_MAGIC;next.version=1;next.count=1;
  next.entries[0]=success;
  // Prefer most recently successful, retain other known networks.
  portENTER_CRITICAL(&guard);
  for(unsigned i=0;i<profiles.count&&next.count<MAX_PROFILES;i++)
    if(!same_ssid(profiles.entries[i],success.ssid))
      next.entries[next.count++]=profiles.entries[i];
  const bool unchanged=profiles.magic==next.magic&&profiles.count==next.count&&
                       !memcmp(&profiles,&next,sizeof(next));
  portEXIT_CRITICAL(&guard);
  if(unchanged)return; // No needless flash write on every successful reboot.
  nvs_handle_t h;
  if(nvs_open("mcx_wifi",NVS_READWRITE,&h)!=ESP_OK)return;
  const bool ok=nvs_set_blob(h,"profiles",&next,sizeof(next))==ESP_OK &&
                nvs_commit(h)==ESP_OK;
  nvs_close(h);
  if(ok){portENTER_CRITICAL(&guard);profiles=next;portEXIT_CRITICAL(&guard);}
}
static void set_state(const char *status,const char *ip=nullptr){
  portENTER_CRITICAL(&guard);
  snprintf(wifi_state,sizeof(wifi_state),"%s",status);
  if(ip)snprintf(local_ip,sizeof(local_ip),"%s",ip);
  portEXIT_CRITICAL(&guard);
}
static void get_status(char *ip,size_t ip_len,char *state,size_t state_len){
  portENTER_CRITICAL(&guard);
  snprintf(ip,ip_len,"%s",local_ip);
  snprintf(state,state_len,"%s",wifi_state);
  portEXIT_CRITICAL(&guard);
}
struct HomeStatus {char ssid[33]={},state[30]={};bool connected=false;};
static HomeStatus home_status(){
  HomeStatus out;
  portENTER_CRITICAL(&guard);
  snprintf(out.ssid,sizeof(out.ssid),"%s",ssid_name);
  snprintf(out.state,sizeof(out.state),"%s",wifi_state);
  out.connected=local_ip[0]!=0;
  portEXIT_CRITICAL(&guard);
  return out;
}
static void timer_callback(void*){
  // Timer task must never reconfigure Wi-Fi itself.
  (void)esp_event_post(MCX_CONTROL_EVENT,RETRY_KNOWN,nullptr,0,0);
}
static void stop_retry_timer(){
  if(retry_timer)(void)esp_timer_stop(retry_timer);
}
static void delay_retry(){
  wants_station=false;
  active_index=-1;
  manual_active=false;
  set_state("Known Wi-Fi unavailable","");
  if(retry_timer){
    (void)esp_timer_stop(retry_timer);
    (void)esp_timer_start_once(retry_timer,60000000ULL);
  }
}
static bool configure_station(const credentials_t &c) {
  if(!valid(c))return false;
  wifi_config_t station={};
  memcpy(station.sta.ssid,c.ssid,strlen(c.ssid));
  memcpy(station.sta.password,c.password,strlen(c.password));
  // Set only after a previous attempt has disconnected.
  if(esp_wifi_set_config(WIFI_IF_STA,&station)!=ESP_OK)return false;
  portENTER_CRITICAL(&guard);snprintf(ssid_name,sizeof(ssid_name),"%s",c.ssid);local_ip[0]=0;portEXIT_CRITICAL(&guard);
  wants_station=true;
  station_connected=false;
  retries=0;
  set_state("Connecting...","");
  return true;
}
static void try_saved(unsigned index){
  credentials_t c={};
  portENTER_CRITICAL(&guard);
  if(index<profiles.count)c=profiles.entries[index];
  portEXIT_CRITICAL(&guard);
  if(!valid(c)){delay_retry();return;}
  active_index=(int)index;
  manual_active=false;
  ++attempted;
  if(!configure_station(c)){delay_retry();return;}
  if(wifi_started && esp_wifi_connect()!=ESP_OK)delay_retry();
  memset(&c,0,sizeof(c));
}
static void next_saved(){
  unsigned count=0;
  portENTER_CRITICAL(&guard);count=profiles.count;portEXIT_CRITICAL(&guard);
  if(count==0||attempted>=count){delay_retry();return;}
  const unsigned next=active_index<0?0:((unsigned)active_index+1)%count;
  try_saved(next);
}
static bool begin_scan(){
  if(scan_running)return false;
  portENTER_CRITICAL(&guard);
  const bool connecting=strcmp(wifi_state,"Connecting...")==0 || strcmp(wifi_state,"Reconnecting...")==0;
  if(!connecting)scan_done=false;
  portEXIT_CRITICAL(&guard);
  if(connecting)return false;
  wifi_scan_config_t config={};config.show_hidden=false;
  if(esp_wifi_scan_start(&config,false)!=ESP_OK)return false;
  scan_running=true;return true;
}
static bool collect_scan(){
  portENTER_CRITICAL(&guard);
  const bool done=scan_done;
  scan_done=false;
  portEXIT_CRITICAL(&guard);
  if(!done)return false;
  scan_running=false;
  wifi_ap_record_t records[12]={};
  uint16_t count=12;
  if(esp_wifi_scan_get_ap_records(&count,records)!=ESP_OK){scan_count=0;return true;}
  scan_count=0;
  for(uint16_t i=0;i<count;i++){
    char name[33]={};memcpy(name,records[i].ssid,32);
    if(!name[0])continue;
    for(char *c=name;*c;c++)if(*c=='\n'||*c=='\r')*c=' ';
    bool duplicate=false;
    for(uint16_t j=0;j<scan_count;j++)if(strcmp(scan_ssids[j],name)==0){duplicate=true;break;}
    if(!duplicate)snprintf(scan_ssids[scan_count++],33,"%s",name);
  }
  return true;
}
static void wifi_event(void *,esp_event_base_t base,int32_t event,void *data){
  if(base==MCX_CONTROL_EVENT && event==CONNECT_MANUAL){
    const credentials_t *c=(const credentials_t*)data;
    if(!c||!valid(*c))return;
    stop_retry_timer();
    attempted=0;manual_active=true;active_index=-1;
    // Suppress only the intentional leave generated by replacing credentials.
    suppress_user_leave=station_connected||wants_station;
    suppress_leave_until_us=esp_timer_get_time()+2000000LL;
    wants_station=false;
    (void)esp_wifi_disconnect();
    if(!configure_station(*c)){set_state("Configuration failed","");return;}
    manual_active=true;
    if(esp_wifi_connect()!=ESP_OK)set_state("Connection failed","");
  }else if(base==MCX_CONTROL_EVENT && event==RETRY_KNOWN){
    if(!station_connected&&!manual_active){
      attempted=0;
      if(profiles.count==0 && valid(legacy_profile)){
        active_index=-1;
        if(configure_station(legacy_profile)){
          if(esp_wifi_connect()!=ESP_OK)delay_retry();
        }else delay_retry();
      }else next_saved();
    }
  }else if(base==WIFI_EVENT && event==WIFI_EVENT_SCAN_DONE){
    portENTER_CRITICAL(&guard);scan_done=true;portEXIT_CRITICAL(&guard);
  }else if(base==WIFI_EVENT && event==WIFI_EVENT_STA_START){
    wifi_started=true;
    if(wants_station)(void)esp_wifi_connect();
  }else if(base==WIFI_EVENT && event==WIFI_EVENT_STA_DISCONNECTED){
    const wifi_event_sta_disconnected_t *d=(const wifi_event_sta_disconnected_t*)data;
    station_connected=false;
    set_state("Reconnecting...","");
    if(!wants_station)return;
    // An AP-originated leave is a real link loss. Suppress only our recent
    // explicit disconnect used to switch the selected station profile.
    if(d && d->reason==WIFI_REASON_ASSOC_LEAVE && suppress_user_leave &&
       esp_timer_get_time()<=suppress_leave_until_us){
      suppress_user_leave=false;return;
    }
    suppress_user_leave=false;
    if(retries<2){
      ++retries;
      if(esp_wifi_connect()==ESP_OK)return;
    }
    next_saved();
  }else if(base==IP_EVENT && event==IP_EVENT_STA_GOT_IP){
    const ip_event_got_ip_t *ev=(const ip_event_got_ip_t*)data;
    char ip[16];snprintf(ip,sizeof(ip),IPSTR,IP2STR(&ev->ip_info.ip));
    retries=0;attempted=0;manual_active=false;station_connected=true;
    stop_retry_timer();
    set_state("Connected to Wi-Fi",ip);
    wifi_config_t saved={};
    if(esp_wifi_get_config(WIFI_IF_STA,&saved)==ESP_OK){
      credentials_t c={};
      memcpy(c.ssid,saved.sta.ssid,32);
      memcpy(c.password,saved.sta.password,63);
      if(valid(c)){remember_success(c);active_index=0;}
      meshbackup::schedule(saved);
      memset(&c,0,sizeof(c));
    }
  }
}
static bool connect(const char *ssid,const char *password){
  if(!ssid||!password)return false;
  const size_t n=strnlen(ssid,34),p=strnlen(password,65);
  if(!n||n>32||p>63||scan_running)return false;
  credentials_t c={};
  if(!p){
    if(!known_network(ssid,&c))return false;
  }else{
    if(p<8)return false;
    memcpy(c.ssid,ssid,n);memcpy(c.password,password,p);
  }
  set_state("Connecting...","");
  const esp_err_t rc=esp_event_post(MCX_CONTROL_EVENT,CONNECT_MANUAL,&c,sizeof(c),0);
  memset(&c,0,sizeof(c));
  if(rc!=ESP_OK)set_state("Connection queue busy","");
  return rc==ESP_OK;
}
static void start(){
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(MCX_CONTROL_EVENT,ESP_EVENT_ANY_ID,wifi_event,nullptr));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  wifi_config_t ap={};
  strncpy((char*)ap.ap.ssid,"MeshCrossway-Dev",sizeof(ap.ap.ssid)-1);
  strncpy((char*)ap.ap.password,"meshcrossway-dev",sizeof(ap.ap.password)-1);
  ap.ap.ssid_len=strlen((char*)ap.ap.ssid);
  ap.ap.channel=6;ap.ap.max_connection=2;ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
  meshbackup::start();
  load_profiles();
  wifi_config_t old={};
  if(esp_wifi_get_config(WIFI_IF_STA,&old)!=ESP_OK)old={};
  if(!old.sta.ssid[0])(void)meshbackup::restore(old);
  const esp_timer_create_args_t t={.callback=timer_callback,.arg=nullptr,.dispatch_method=ESP_TIMER_TASK,.name="mcx_wifi_retry"};
  if(esp_timer_create(&t,&retry_timer)!=ESP_OK)retry_timer=nullptr;
  if(profiles.count){
    // The primary profile is the last network successfully connected.
    credentials_t c={};
    portENTER_CRITICAL(&guard);c=profiles.entries[0];portEXIT_CRITICAL(&guard);
    if(configure_station(c)){active_index=0;attempted=1;}
    memset(&c,0,sizeof(c));
  }else if(old.sta.ssid[0]){
    credentials_t c={};
    memcpy(c.ssid,old.sta.ssid,32);
    memcpy(c.password,old.sta.password,63);
    if(configure_station(c)){
      active_index=-1;attempted=0;legacy_profile=c;
    }
    memset(&c,0,sizeof(c));
  }
  ESP_ERROR_CHECK(esp_wifi_start());
}
} // namespace meshnetwork
