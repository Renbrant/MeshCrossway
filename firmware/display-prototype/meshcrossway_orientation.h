#pragma once
#include "meshcrossway_orientation_sensor.h"
#include "meshcrossway_touch_input.h"
#include "esp_lcd_panel_ops.h"
#include "nvs.h"
extern esp_lcd_panel_handle_t panel_handle;
namespace mcxorientation {
struct Preferences {uint8_t version=1,locked=0,flipped=0;int8_t normal=0;};
static Preferences preferences;
static Policy policy;
static bool flipped=false,requested=false,dirty=false,manual_pending=false,calibration_pending=false;
static uint32_t changed=0;
static void mark(){dirty=true;changed=lv_tick_get();}
static void lock_toggle(){
  preferences.locked=!preferences.locked;preferences.flipped=flipped;requested=flipped;manual_pending=false;
  paused.store(preferences.locked&&!calibration_pending);sample.store(0);policy.reset();mark();
}
static void manual_flip(){
  requested=!requested;preferences.locked=1;preferences.flipped=requested;manual_pending=requested!=flipped;
  paused.store(true);sample.store(0);policy.reset();mark();
}
static bool apply(bool target){
  auto *display=lv_disp_get_default();
  if(!display||!panel_handle||!mcxtouch::active()||mcxtouch::busy())return false;
  // No panel MADCTL change while DMA is reading an outstanding draw buffer.
  if(display->driver->draw_buf->flushing)return false;
  // Pinned BSP landscape uses swap_xy=1, mirror_x=mirror_y=1. Only mirrors
  // change; resolution, LVGL objects, software rotation and buffers stay intact.
  if(esp_lcd_panel_mirror(panel_handle,!target,!target)!=ESP_OK)return false;
  mcxtouch::rotate_input(target);flipped=target;
  lv_obj_invalidate(lv_scr_act());lv_obj_invalidate(lv_layer_top());lv_obj_invalidate(lv_layer_sys());
  printf("[MCX-ROTATE] applied=%u locked=%u\n",target?180u:0u,unsigned(preferences.locked));
  return true;
}
static void tick(lv_timer_t *){
  const uint32_t now=lv_tick_get();
  if(requested!=flipped){if(apply(requested)){
    if(manual_pending){
      policy.manual_correction();preferences.normal=0;manual_pending=false;calibration_pending=true;
      sample.store(0);paused.store(false);mark();
      printf("[MCX-ROTATE] hold readable orientation to correct AUTO reference\n");
    }
    policy.applied(now);
  }}
  else {
    bool target=flipped;
    if(policy.update(fresh_sign(),now,preferences.locked&&!calibration_pending,mcxtouch::busy(),flipped,target)&&!preferences.locked&&apply(target)){
      requested=target;policy.applied(now);
    }
    if(policy.normal!=preferences.normal){
      preferences.normal=int8_t(policy.normal);mark();
      if(calibration_pending&&policy.normal){calibration_pending=false;paused.store(preferences.locked);
        printf("[MCX-ROTATE] manual reference corrected normal=%d\n",policy.normal);}
    }
  }
  if(dirty&&uint32_t(now-changed)>=1200){
    nvs_handle_t h;
    if(nvs_open("mcxdisplay",NVS_READWRITE,&h)==ESP_OK){
      if(nvs_set_blob(h,"orientation",&preferences,sizeof(preferences))==ESP_OK&&nvs_commit(h)==ESP_OK)dirty=false;
      nvs_close(h);
    }
    changed=now;
  }
}
static void init(){
  nvs_handle_t h;
  if(nvs_open("mcxdisplay",NVS_READONLY,&h)==ESP_OK){
    Preferences saved;size_t size=sizeof(saved);
    if(nvs_get_blob(h,"orientation",&saved,&size)==ESP_OK&&size==sizeof(saved)&&saved.version==1&&
       saved.locked<=1&&saved.flipped<=1&&saved.normal>=-2&&saved.normal<=2)preferences=saved;
    nvs_close(h);
  }
  requested=preferences.flipped;policy.normal=preferences.normal;paused.store(preferences.locked);
  lv_timer_create(tick,200,nullptr);
}
}
