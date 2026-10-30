#pragma once
#include "meshcrossway_clock.h"
#include "nvs.h"
#include "esp_timer.h"
#include <sys/time.h>
#include <stdlib.h>
namespace mcxclock {
static State state;
static int applied_zone=-2,persisted_zone=-1;
static bool initialized=false;
static int64_t last_save_attempt=-60000;
static void begin(){
  if(initialized)return;
  initialized=true;nvs_handle_t h;
  if(nvs_open("mcx_clock",NVS_READONLY,&h)==ESP_OK){
    char name[64]={};size_t size=sizeof(name);
    if(nvs_get_blob(h,"zone",name,&size)==ESP_OK&&size>1&&size<=sizeof(name)&&name[size-1]==0){
      for(unsigned i=0;i<mcxtz::count;++i)if(!strcmp(name,mcxtz::zones[i].name)){state.zone=persisted_zone=int(i);break;}
    }nvs_close(h);
  }
}
// Called only by LVGL's one-second timer; no network or map task is involved.
static void poll(const mcxgnss::Fix &fix,const mcxgnss::Settings &cfg){
  begin();const int64_t now=esp_timer_get_time()/1000;
  if(state.update(fix,cfg,now)){
    const timeval value={time_t(state.epoch(now)),0};settimeofday(&value,nullptr);
  }
  if(applied_zone!=state.zone){
    setenv("TZ",state.zone<0?"UTC0":mcxtz::zones[state.zone].rule,1);tzset();applied_zone=state.zone;
  }
  if(state.zone>=0&&persisted_zone!=state.zone&&now-last_save_attempt>=60000){
    last_save_attempt=now;
    // Store the name, so future table reordering cannot change the cached zone.
    nvs_handle_t h;
    if(nvs_open("mcx_clock",NVS_READWRITE,&h)==ESP_OK){
      const char *name=mcxtz::zones[state.zone].name;
      if(nvs_set_blob(h,"zone",name,strlen(name)+1)==ESP_OK&&nvs_commit(h)==ESP_OK)persisted_zone=state.zone;
      nvs_close(h);
    }
  }
}
static void label(char *out,size_t cap){
  if(!state.synced){snprintf(out,cap,"--:--");return;}
  const time_t value=time_t(state.epoch(esp_timer_get_time()/1000));struct tm local={};
  localtime_r(&value,&local);strftime(out,cap,"%H:%M",&local);
}
static const char *zone_name(){return state.zone<0?"UTC / waiting for GPS position":mcxtz::zones[state.zone].name;}
}
