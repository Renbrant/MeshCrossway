#pragma once
// MeshCrossway Wi-Fi station + recovery AP. No network credentials in source code.
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>

namespace meshnetwork {
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static char local_ip[16]="";
static char wifi_state[30]="Wi-Fi not configured";
static char ssid_name[33]="";
static bool wants_station=false;
static unsigned retries=0;
ESP_EVENT_DEFINE_BASE(MCX_CONTROL_EVENT);
struct credentials_t { char ssid[33]; char password[64]; };
// Scan asynchronously so the touchscreen does not stop responding.
static bool scan_done=false;
static bool scan_running=false;
static char scan_ssids[12][33]={};
static uint16_t scan_count=0;
static bool begin_scan(){
  if(scan_running)return false;
  portENTER_CRITICAL(&guard);
  const bool connecting=strcmp(wifi_state,"Connecting...")==0 || strcmp(wifi_state,"Reconnecting...")==0;
  if(!connecting)scan_done=false;
  portEXIT_CRITICAL(&guard);
  if(connecting)return false;
  wifi_scan_config_t config={};
  config.show_hidden=false;
  if(esp_wifi_scan_start(&config,false)!=ESP_OK)return false;
  scan_running=true;
  return true;
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
  if(esp_wifi_scan_get_ap_records(&count,records)!=ESP_OK){
    scan_count=0;return true;
  }
  scan_count=0;
  for(uint16_t i=0;i<count;i++){
    char name[33]={};
    memcpy(name,records[i].ssid,32);
    if(!name[0])continue;
    for(char *c=name;*c;c++)if(*c=='\n'||*c=='\r')*c=' ';
    bool duplicate=false;
    for(uint16_t j=0;j<scan_count;j++)
      if(strcmp(scan_ssids[j],name)==0){duplicate=true;break;}
    if(!duplicate)snprintf(scan_ssids[scan_count++],33,"%s",name);
  }
  return true;
}


static void set_state(const char *status,const char *ip=nullptr) {
  portENTER_CRITICAL(&guard);
  snprintf(wifi_state,sizeof(wifi_state),"%s",status);
  if(ip) snprintf(local_ip,sizeof(local_ip),"%s",ip);
  portEXIT_CRITICAL(&guard);
}
static void get_status(char *ip,size_t ip_len,char *state,size_t state_len) {
  portENTER_CRITICAL(&guard);
  snprintf(ip,ip_len,"%s",local_ip);
  snprintf(state,state_len,"%s",wifi_state);
  portEXIT_CRITICAL(&guard);
}
static void wifi_event(void *,esp_event_base_t base,int32_t event,void *data) {
  if(base==MCX_CONTROL_EVENT) {
    // All station configuration/retry state belongs to the ESP event task.
    const credentials_t *credentials=(const credentials_t*)data;
    wifi_config_t station={};
    memcpy(station.sta.ssid,credentials->ssid,strlen(credentials->ssid));
    memcpy(station.sta.password,credentials->password,strlen(credentials->password));
    wants_station=false;
    (void)esp_wifi_disconnect();
    if(esp_wifi_set_config(WIFI_IF_STA,&station)!=ESP_OK) {
      set_state("Configuration failed","");return;
    }
    snprintf(ssid_name,sizeof(ssid_name),"%s",credentials->ssid);
    retries=0;
    wants_station=true;
    set_state("Connecting...","");
    if(esp_wifi_connect()!=ESP_OK)set_state("Connection failed","");
  } else if(base==WIFI_EVENT && event==WIFI_EVENT_SCAN_DONE) {
    portENTER_CRITICAL(&guard);
    scan_done=true;
    portEXIT_CRITICAL(&guard);
  } else if(base==WIFI_EVENT && event==WIFI_EVENT_STA_START) {
    if(wants_station)(void)esp_wifi_connect();
  } else if(base==WIFI_EVENT && event==WIFI_EVENT_STA_DISCONNECTED) {
    const wifi_event_sta_disconnected_t *disconnected=(const wifi_event_sta_disconnected_t*)data;
    // A queued disconnect from changing credentials must not trigger a second connect.
    if(disconnected->reason==WIFI_REASON_ASSOC_LEAVE)return;
    set_state("Reconnecting...","");
    if(wants_station && retries<5) {
      ++retries;
      if(esp_wifi_connect()!=ESP_OK)set_state("Connection failed","");
    } else set_state("Connection failed","");
  } else if(base==IP_EVENT && event==IP_EVENT_STA_GOT_IP) {
    const ip_event_got_ip_t *event_data=(const ip_event_got_ip_t*)data;
    char ip[16];
    snprintf(ip,sizeof(ip),IPSTR,IP2STR(&event_data->ip_info.ip));
    retries=0;
    set_state("Connected to Wi-Fi",ip);
  }
}
static bool connect(const char *ssid,const char *password) {
  if(!ssid || !password)return false;
  const size_t n=strlen(ssid),p=strlen(password);
  // WPA2/WPA3 personal only in this development build, on 2.4 GHz.
  if(n==0 || n>32 || p<8 || p>63)return false;
  // The LVGL task owns scan_running. Drain scan results before connecting.
  if(scan_running)return false;
  credentials_t credentials={};
  memcpy(credentials.ssid,ssid,n);
  memcpy(credentials.password,password,p);
  set_state("Connecting...","");
  const esp_err_t result=esp_event_post(MCX_CONTROL_EVENT,0,&credentials,sizeof(credentials),0);
  memset(&credentials,0,sizeof(credentials));
  if(result!=ESP_OK)set_state("Connection queue busy","");
  return result==ESP_OK;
}
static void start() {
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
  wifi_config_t old={};
  if(esp_wifi_get_config(WIFI_IF_STA,&old)==ESP_OK && old.sta.ssid[0]) {
    portENTER_CRITICAL(&guard);
    memcpy(ssid_name,old.sta.ssid,32);ssid_name[32]='\0';
    wants_station=true;
    portEXIT_CRITICAL(&guard);
    set_state("Connecting...","");
  }
  ESP_ERROR_CHECK(esp_wifi_start());
}
} // namespace meshnetwork
