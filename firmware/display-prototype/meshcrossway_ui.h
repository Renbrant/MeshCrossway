#pragma once
// MeshCrossway UI v0.2 — original graphite/orange design inspired by modern field consoles.
// LVGL 8.x; every radio/GNSS metric is simulated. No packets are transmitted.
#include "lvgl.h"
#include <stdint.h>
namespace meshcrossway {
static lv_obj_t *body=nullptr,*heading=nullptr,*foot=nullptr,*tabs[5]={};
static int page=0;
static const lv_color_t bg=lv_color_hex(0x090C10);
static const lv_color_t panel=lv_color_hex(0x181D24);
static const lv_color_t panel2=lv_color_hex(0x242B34);
static const lv_color_t orange=lv_color_hex(0xFF8A24);
static const lv_color_t white=lv_color_hex(0xF2F4F7);
static const lv_color_t muted=lv_color_hex(0x9AA6B4);
static void fill(lv_obj_t *o,lv_color_t color,int r=0) {
  lv_obj_set_style_bg_color(o,color,0);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_radius(o,r,0);
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_set_style_pad_all(o,0,0);
  lv_obj_set_style_shadow_width(o,0,0);
}
static lv_obj_t *txt(lv_obj_t *p,const char *s,int x,int y,int w,lv_color_t color) {
  lv_obj_t *o=lv_label_create(p);
  lv_label_set_text(o,s);
  lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
  lv_obj_set_width(o,w);
  lv_obj_set_pos(o,x,y);
  lv_obj_set_style_text_color(o,color,0);
  return o;
}
static lv_obj_t *tile(int x,int y,int w,int h,const char *top,const char *line1,const char *line2) {
  lv_obj_t *o=lv_obj_create(body);fill(o,panel,9);
  lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
  txt(o,top,11,9,w-22,orange);
  txt(o,line1,11,32,w-22,white);
  if(line2) txt(o,line2,11,53,w-22,muted);
  return o;
}
static void render() {
  static const char *heads[]={"OVERVIEW","MESSAGES","NAVIGATION","RADIOS","SETTINGS"};
  lv_label_set_text(heading,heads[page]);
  lv_obj_clean(body);
  for(int i=0;i<5;i++) {
    fill(tabs[i],i==page?orange:panel2,8);
    lv_obj_set_style_text_color(tabs[i],i==page?bg:white,0);
  }
  switch(page) {
  case 0:
    tile(7,7,171,91,"MESHTASTIC","Radio A / Standby","17 peers  [DEMO]");
    tile(185,7,171,91,"MESHCORE","Radio B / Standby","9 peers   [DEMO]");
    tile(7,105,171,91,"POSITION","No GNSS connected","Fix: demo only");
    tile(185,105,171,91,"CROSSWAY","2 independent radios","Bridge: disabled");
    break;
  case 1:
    tile(7,7,349,62,"MESHTASTIC  /  DEMO","Trail group: At checkpoint",nullptr);
    tile(7,76,349,62,"MESHCORE  /  DEMO","Field team: Copy, all good",nullptr);
    tile(7,145,349,51,"OFFLINE INBOX","Sample messages only",nullptr);
    break;
  case 2:
    tile(7,7,349,130,"MAP  /  OFFLINE","No map tiles loaded","GNSS source: simulated");
    tile(7,144,349,52,"LOCATION SHARING","Off  /  No data transmitted",nullptr);
    break;
  case 3:
    tile(7,7,171,152,"RADIO A  /  MT","No UART link","RSSI: demo -87 dBm");
    tile(185,7,171,152,"RADIO B  /  MC","No UART link","RSSI: demo -94 dBm");
    txt(body,"No packets sent  •  Simulation only",10,176,340,muted);
    break;
  default:
    tile(7,7,349,91,"DISPLAY","480 x 320  /  Landscape","Touch: enabled");
    tile(7,105,349,91,"FIRMWARE","MeshCrossway Dev v0.2","Radios & GNSS: simulated");
    break;
  }
}
static void nav_cb(lv_event_t *e) {
  page=(int)(intptr_t)lv_event_get_user_data(e);
  render();
}
static void init() {
  lv_obj_t *s=lv_scr_act();fill(s,bg);
  lv_obj_clear_flag(s,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *bar=lv_obj_create(s);fill(bar,panel);
  lv_obj_set_pos(bar,0,0);lv_obj_set_size(bar,480,43);
  lv_obj_clear_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
  txt(bar,"MESH",10,11,49,white);
  txt(bar,"CROSSWAY",59,11,104,orange);
  heading=txt(bar,"OVERVIEW",185,11,170,white);
  txt(bar,"DEV 0.2",395,12,80,orange);
  lv_obj_t *rail=lv_obj_create(s);fill(rail,bg);
  lv_obj_set_pos(rail,0,43);lv_obj_set_size(rail,108,242);
  lv_obj_clear_flag(rail,LV_OBJ_FLAG_SCROLLABLE);
  static const char *names[]={"Home","Messages","Map","Radios","Settings"};
  for(int i=0;i<5;i++) {
    tabs[i]=lv_btn_create(rail);
    lv_obj_set_pos(tabs[i],5,5+i*47);
    lv_obj_set_size(tabs[i],98,42);
    lv_obj_set_style_pad_all(tabs[i],0,0);
    lv_obj_add_event_cb(tabs[i],nav_cb,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    lv_obj_t *t=lv_label_create(tabs[i]);
    lv_label_set_text(t,names[i]);lv_obj_center(t);
  }
  body=lv_obj_create(s);fill(body,bg);
  lv_obj_set_pos(body,108,43);lv_obj_set_size(body,372,242);
  lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *bottom=lv_obj_create(s);fill(bottom,panel);
  lv_obj_set_pos(bottom,0,285);lv_obj_set_size(bottom,480,35);
  lv_obj_clear_flag(bottom,LV_OBJ_FLAG_SCROLLABLE);
  foot=txt(bottom,"DEMO MODE  |  MT: --  MC: --  GPS: --",11,9,450,muted);
  render();
}
} // namespace meshcrossway
