#pragma once
// MeshCrossway v0.5: local Wi-Fi provisioning + two-way remote UI.
// All radio and GNSS values remain simulated.
#include "lvgl.h"
#include "meshcrossway_remote.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace meshcrossway {
static lv_obj_t *body=nullptr,*heading=nullptr,*tabs[5]={};
static lv_obj_t *wifi_dialog=nullptr,*wifi_ssid=nullptr,*wifi_password=nullptr,*wifi_keyboard=nullptr,*wifi_notice=nullptr;
static int page=0;
static lv_obj_t *uptime_value=nullptr,*heap_value=nullptr,*network_value=nullptr;
static const lv_color_t bg=lv_color_hex(0x090C10);
static const lv_color_t panel=lv_color_hex(0x181D24);
static const lv_color_t panel2=lv_color_hex(0x242B34);
static const lv_color_t orange=lv_color_hex(0xFF8A24);
static const lv_color_t white=lv_color_hex(0xF2F4F7);
static const lv_color_t muted=lv_color_hex(0x9AA6B4);

static void fill(lv_obj_t *o,lv_color_t color,int r=0){
  lv_obj_set_style_bg_color(o,color,0);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_radius(o,r,0);
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_set_style_pad_all(o,0,0);
  lv_obj_set_style_shadow_width(o,0,0);
}
static lv_obj_t *txt(lv_obj_t *p,const char *s,int x,int y,int w,lv_color_t c){
  lv_obj_t *l=lv_label_create(p);
  lv_label_set_text(l,s);
  lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
  lv_obj_set_width(l,w);lv_obj_set_pos(l,x,y);
  lv_obj_set_style_text_color(l,c,0);
  return l;
}
static lv_obj_t *tile(int x,int y,int w,int h,const char *title,const char *line1,const char *line2){
  lv_obj_t *c=lv_obj_create(body);
  fill(c,panel,9);
  lv_obj_set_pos(c,x,y);lv_obj_set_size(c,w,h);
  lv_obj_clear_flag(c,LV_OBJ_FLAG_SCROLLABLE);
  txt(c,title,11,9,w-22,orange);
  txt(c,line1,11,32,w-22,white);
  if(line2)txt(c,line2,11,53,w-22,muted);
  return c;
}
static void draw();
static void close_wifi(){
  if(wifi_dialog)lv_obj_del(wifi_dialog);
  wifi_dialog=nullptr;
  wifi_ssid=nullptr;wifi_password=nullptr;wifi_keyboard=nullptr;wifi_notice=nullptr;
}
static void on_cancel(lv_event_t *){close_wifi();}
static void on_connect(lv_event_t *){
  if(!wifi_ssid || !wifi_password)return;
  const char *ssid=lv_textarea_get_text(wifi_ssid);
  const char *password=lv_textarea_get_text(wifi_password);
  const size_t n=strlen(ssid),p=strlen(password);
  if(n<1 || n>32 || p<8 || p>63){
    lv_label_set_text(wifi_notice,"SSID: 1-32 chars  /  Password: 8-63 chars");
    return;
  }
  if(!meshnetwork::connect(ssid,password)){
    lv_label_set_text(wifi_notice,"Could not initiate Wi-Fi connection.");
    return;
  }
  // Keep the Wi-Fi credentials in ESP-IDF NVS, not in app source or LVGL fields.
  lv_textarea_set_text(wifi_password,"");
  close_wifi();
  page=4;
  draw();
}
static void on_wifi_field(lv_event_t *e){
  if(wifi_keyboard)lv_keyboard_set_textarea(wifi_keyboard,lv_event_get_target(e));
}
static void wifi_setup(lv_event_t *){
  if(wifi_dialog)return;
  wifi_dialog=lv_obj_create(lv_layer_top());
  fill(wifi_dialog,bg,0);
  lv_obj_set_pos(wifi_dialog,0,0);
  lv_obj_set_size(wifi_dialog,480,320);
  lv_obj_clear_flag(wifi_dialog,LV_OBJ_FLAG_SCROLLABLE);
  txt(wifi_dialog,"CONNECT TO HOME WI-FI  /  2.4 GHz",10,8,345,orange);
  lv_obj_t *cancel=lv_btn_create(wifi_dialog);
  fill(cancel,panel2,7);lv_obj_set_pos(cancel,383,5);lv_obj_set_size(cancel,87,32);
  txt(cancel,"CANCEL",14,7,72,white);
  lv_obj_add_event_cb(cancel,on_cancel,LV_EVENT_CLICKED,nullptr);

  wifi_ssid=lv_textarea_create(wifi_dialog);
  lv_obj_set_pos(wifi_ssid,9,42);lv_obj_set_size(wifi_ssid,462,37);
  lv_textarea_set_one_line(wifi_ssid,true);
  lv_textarea_set_max_length(wifi_ssid,32);
  lv_textarea_set_placeholder_text(wifi_ssid,"Wi-Fi SSID (network name)");
  lv_obj_add_event_cb(wifi_ssid,on_wifi_field,LV_EVENT_CLICKED,nullptr);

  wifi_password=lv_textarea_create(wifi_dialog);
  lv_obj_set_pos(wifi_password,9,86);lv_obj_set_size(wifi_password,462,37);
  lv_textarea_set_one_line(wifi_password,true);
  lv_textarea_set_max_length(wifi_password,63);
  lv_textarea_set_password_mode(wifi_password,true);
  lv_textarea_set_placeholder_text(wifi_password,"Wi-Fi password");
  lv_obj_add_event_cb(wifi_password,on_wifi_field,LV_EVENT_CLICKED,nullptr);

  wifi_notice=txt(wifi_dialog,"WPA2/WPA3 home network; credentials saved locally.",10,133,320,muted);
  lv_obj_t *connect=lv_btn_create(wifi_dialog);
  fill(connect,orange,7);lv_obj_set_pos(connect,350,126);lv_obj_set_size(connect,121,35);
  txt(connect,"CONNECT",22,9,90,bg);
  lv_obj_add_event_cb(connect,on_connect,LV_EVENT_CLICKED,nullptr);

  wifi_keyboard=lv_keyboard_create(wifi_dialog);
  lv_obj_set_pos(wifi_keyboard,4,167);lv_obj_set_size(wifi_keyboard,472,149);
  lv_keyboard_set_textarea(wifi_keyboard,wifi_ssid);
  lv_obj_set_style_bg_color(wifi_keyboard,panel,0);
}
static void draw(){
  uptime_value=nullptr;heap_value=nullptr;network_value=nullptr;
  static const char *names[]={"OVERVIEW","MESSAGES","NAVIGATION","RADIOS","SETTINGS"};
  lv_label_set_text(heading,names[page]);
  lv_obj_clean(body);
  for(int i=0;i<5;i++){
    fill(tabs[i],i==page?orange:panel2,8);
    lv_obj_set_style_text_color(tabs[i],i==page?bg:white,0);
  }
  switch(page){
    case 0:
      tile(7,7,171,91,"MESHTASTIC","Radio A / Standby","17 peers [DEMO]");
      tile(185,7,171,91,"MESHCORE","Radio B / Standby","9 peers [DEMO]");
      tile(7,105,171,91,"POSITION","No GNSS connected","Fix: demo only");
      tile(185,105,171,91,"CROSSWAY","Dual radio design","Bridge: off");
      break;
    case 1:
      tile(7,7,349,62,"MESHTASTIC / DEMO","Trail group: At checkpoint",nullptr);
      tile(7,76,349,62,"MESHCORE / DEMO","Field team: Copy, all good",nullptr);
      tile(7,145,349,51,"OFFLINE INBOX","Sample messages only",nullptr);
      break;
    case 2:
      tile(7,7,349,130,"MAP / OFFLINE","No map tiles loaded","GNSS source: simulated");
      tile(7,144,349,52,"LOCATION SHARING","Off / No data transmitted",nullptr);
      break;
    case 3:
      tile(7,7,171,152,"RADIO A / MT","BLE not connected","RSSI: demo -87 dBm");
      tile(185,7,171,152,"RADIO B / MC","BLE not connected","RSSI: demo -94 dBm");
      txt(body,"No packets sent - Simulation only",10,176,340,muted);
      break;
    default:{
      char uptime[40],memory[40],ip[16],wifi[30],address[50],pairing[48];
      const uint64_t seconds=(uint64_t)(esp_timer_get_time()/1000000LL);
      snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
               (unsigned long long)(seconds/3600),(unsigned long long)((seconds/60)%60));
      snprintf(memory,sizeof(memory),"Free heap: %lu KB",
               (unsigned long)(esp_get_free_heap_size()/1024));
      meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
      snprintf(address,sizeof(address),"%s",ip[0]?ip:wifi);
      snprintf(pairing,sizeof(pairing),"Remote control: disabled (diagnostic)");
      lv_obj_t *device_card=tile(7,6,349,80,"DEVICE / LIVE",uptime,memory);
      lv_obj_t *wifi_card=tile(7,93,349,82,"WIFI / REMOTE",address,pairing);
      uptime_value=lv_obj_get_child(device_card,1);
      heap_value=lv_obj_get_child(device_card,2);
      network_value=lv_obj_get_child(wifi_card,1);
      lv_obj_t *b=lv_btn_create(body);
      fill(b,orange,8);lv_obj_set_pos(b,7,182);lv_obj_set_size(b,349,44);
      txt(b,"CONNECT / CHANGE HOME WI-FI",28,11,325,bg);
      lv_obj_add_event_cb(b,wifi_setup,LV_EVENT_CLICKED,nullptr);
      break;
    }
  }
}
static void on_nav(lv_event_t *e){
  page=(int)(intptr_t)lv_event_get_user_data(e);
  draw();
}
static void refresh(lv_timer_t *){
  printf("[MCX-DIAG v0.5.2] uptime_ms=%llu heap=%lu page=%d\n",
         (unsigned long long)(esp_timer_get_time()/1000),
         (unsigned long)esp_get_free_heap_size(),page);
  // Change only three existing labels. Destroying/rebuilding the full Settings
  // subtree every 3 seconds invalidates the entire LVGL area and can visibly flash.
  if(page!=4 || wifi_dialog || !uptime_value || !heap_value || !network_value)return;
  char uptime[40],memory[40],ip[16],wifi[30];
  const uint64_t sec=(uint64_t)(esp_timer_get_time()/1000000LL);
  snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
           (unsigned long long)(sec/3600),(unsigned long long)((sec/60)%60));
  snprintf(memory,sizeof(memory),"Free heap: %lu KB",
           (unsigned long)(esp_get_free_heap_size()/1024));
  meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
  const char *network=ip[0]?ip:wifi;
  if(strcmp(lv_label_get_text(uptime_value),uptime)!=0)lv_label_set_text(uptime_value,uptime);
  if(strcmp(lv_label_get_text(heap_value),memory)!=0)lv_label_set_text(heap_value,memory);
  if(strcmp(lv_label_get_text(network_value),network)!=0)lv_label_set_text(network_value,network);
}
static void init(){
  printf("[MCX-DIAG v0.5.2] boot reset_reason=%d heap=%lu remote=OFF wifi=ON\n",
         (int)esp_reset_reason(),(unsigned long)esp_get_free_heap_size());
  lv_obj_t *screen=lv_scr_act();fill(screen,bg);
  lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *bar=lv_obj_create(screen);fill(bar,panel);
  lv_obj_set_pos(bar,0,0);lv_obj_set_size(bar,480,43);
  lv_obj_clear_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
  txt(bar,"MESH",10,11,49,white);
  txt(bar,"CROSSWAY",59,11,104,orange);
  heading=txt(bar,"OVERVIEW",185,11,170,white);
  txt(bar,"DEV 0.5.2",386,12,90,orange);
  lv_obj_t *rail=lv_obj_create(screen);fill(rail,bg);
  lv_obj_set_pos(rail,0,43);lv_obj_set_size(rail,108,242);
  lv_obj_clear_flag(rail,LV_OBJ_FLAG_SCROLLABLE);
  static const char *names[]={"Home","Messages","Map","Radios","Settings"};
  for(int i=0;i<5;i++){
    tabs[i]=lv_btn_create(rail);fill(tabs[i],panel2,8);
    lv_obj_set_pos(tabs[i],5,5+i*47);lv_obj_set_size(tabs[i],98,42);
    lv_obj_add_event_cb(tabs[i],on_nav,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    lv_obj_t *l=lv_label_create(tabs[i]);lv_label_set_text(l,names[i]);lv_obj_center(l);
  }
  body=lv_obj_create(screen);fill(body,bg);
  lv_obj_set_pos(body,108,43);lv_obj_set_size(body,372,242);
  lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *bottom=lv_obj_create(screen);fill(bottom,panel);
  lv_obj_set_pos(bottom,0,285);lv_obj_set_size(bottom,480,35);
  lv_obj_clear_flag(bottom,LV_OBJ_FLAG_SCROLLABLE);
  txt(bottom,"DEMO MODE  |  MT: --  MC: --  GPS: --",11,9,450,muted);
  draw();
  lv_timer_create(refresh,3000,nullptr);
}
} // namespace meshcrossway
