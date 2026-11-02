#pragma once
#include "lvgl.h"
#include "nvs.h"
#include <stdint.h>
namespace mcxdisplay {
struct Settings {uint32_t version=1;uint16_t brightness=80,timeout=0;};
static Settings settings;
static constexpr uint16_t timeouts[]={0,30,60,120,300,600};
static void (*apply_brightness)(unsigned)=nullptr;
static lv_obj_t *cover=nullptr;
static bool dirty=false;static uint32_t changed=0;
static bool valid(const Settings &s){
  if(s.version!=1||s.brightness<5||s.brightness>100)return false;
  for(auto t:timeouts)if(t==s.timeout)return true;
  return false;
}
static void mark_changed(){dirty=true;changed=lv_tick_get();}
static void brightness(unsigned value){settings.brightness=uint16_t(value<5?5:value>100?100:value);if(apply_brightness&&!cover)apply_brightness(settings.brightness);mark_changed();}
static void timeout(unsigned slot){if(slot<6){settings.timeout=timeouts[slot];lv_disp_trig_activity(nullptr);mark_changed();}}
static unsigned timeout_slot(){for(unsigned i=0;i<6;++i)if(timeouts[i]==settings.timeout)return i;return 0;}
static void wake(lv_event_t *){
  if(!cover)return;
  if(apply_brightness)apply_brightness(settings.brightness);
  lv_disp_trig_activity(nullptr);
  if(auto *input=lv_indev_get_act())lv_indev_wait_release(input);
  // Keep the cover through this event dispatch so the waking tap cannot hit a widget.
  lv_async_call([](void *){if(cover){lv_obj_del(cover);cover=nullptr;}},nullptr);
}
static void tick(lv_timer_t *){
  if(dirty&&uint32_t(lv_tick_get()-changed)>=1200){
    nvs_handle_t h;
    if(nvs_open("mcxdisplay",NVS_READWRITE,&h)==ESP_OK){
      if(nvs_set_blob(h,"settings",&settings,sizeof(settings))==ESP_OK&&nvs_commit(h)==ESP_OK)dirty=false;
      nvs_close(h);
    }
    changed=lv_tick_get();
  }
  if(!cover&&settings.timeout&&lv_disp_get_inactive_time(nullptr)>=uint32_t(settings.timeout)*1000){
    cover=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(cover);lv_obj_set_pos(cover,0,0);lv_obj_set_size(cover,480,320);
    lv_obj_set_style_bg_color(cover,lv_color_black(),0);lv_obj_set_style_bg_opa(cover,LV_OPA_COVER,0);
    lv_obj_add_flag(cover,LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(cover,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(cover,wake,LV_EVENT_PRESSED,nullptr);
    if(apply_brightness)apply_brightness(0);
  }
}
static void init(void (*set)(unsigned)){
  apply_brightness=set;nvs_handle_t h;
  if(nvs_open("mcxdisplay",NVS_READONLY,&h)==ESP_OK){Settings saved;size_t size=sizeof(saved);
    if(nvs_get_blob(h,"settings",&saved,&size)==ESP_OK&&size==sizeof(saved)&&valid(saved))settings=saved;
    nvs_close(h);
  }
  if(apply_brightness)apply_brightness(settings.brightness);
  lv_disp_trig_activity(nullptr);lv_timer_create(tick,250,nullptr);
}
}
