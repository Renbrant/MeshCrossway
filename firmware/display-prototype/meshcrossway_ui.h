#pragma once
// MeshCrossway early touchscreen bring-up UI. LVGL 8.4; all network data is fake.
// This file is included from Waveshare examples/esp-idf/02_lvgl_example/main/main.cpp.
#include "lvgl.h"
#include <stdio.h>

namespace meshcrossway {
static lv_obj_t *content = nullptr;
static lv_obj_t *title = nullptr;
static lv_obj_t *status = nullptr;
static lv_obj_t *nav[5] = {};
static int page = 0;
static unsigned tick = 0;
static const lv_color_t bg = lv_color_hex(0x101722);
static const lv_color_t panel = lv_color_hex(0x1c293a);
static const lv_color_t bright = lv_color_hex(0x28baca);
static const lv_color_t muted = lv_color_hex(0xaabacc);

static void style(lv_obj_t *o, lv_color_t color, int radius=9) {
  lv_obj_set_style_bg_color(o,color,0);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_set_style_radius(o,radius,0);
}
static lv_obj_t *label(lv_obj_t *parent,const char *s,int x,int y,lv_color_t c) {
  lv_obj_t *l=lv_label_create(parent);
  lv_label_set_text(l,s);
  lv_obj_set_style_text_color(l,c,0);
  lv_obj_set_pos(l,x,y);
  return l;
}
static void clear_content() { lv_obj_clean(content); }
static lv_obj_t *card(int x,int y,int w,int h,const char *heading,const char *body) {
  lv_obj_t *c=lv_obj_create(content);
  lv_obj_set_pos(c,x,y); lv_obj_set_size(c,w,h);
  style(c,panel);
  lv_obj_clear_flag(c,LV_OBJ_FLAG_SCROLLABLE);
  label(c,heading,10,7,bright);
  label(c,body,10,31,lv_color_white());
  return c;
}
static void draw() {
  static const char *names[]={"HOME","MESSAGES","MAP","RADIOS","SETTINGS"};
  lv_label_set_text(title,names[page]);
  lv_label_set_text(status,"DEMO  |  MESH A + MESH B  |  GNSS MOCK");
  clear_content();
  for(int i=0;i<5;i++) style(nav[i],i==page?bright:panel,7);
  if(page==0) {
    card(3,3,160,87,"MESHTASTIC","ONLINE (MOCK)\n17 nodes  -87 dBm");
    card(170,3,160,87,"MESHCORE","ONLINE (MOCK)\n9 nodes   -94 dBm");
    card(3,99,160,86,"GNSS","3D FIX (MOCK)\n14 sats  1.8 m");
    card(170,99,160,86,"CROSSWAY","2 independent links\nNO BRIDGE");
    label(content,"Touch the left menu to explore.",9,194,muted);
  } else if(page==1) {
    card(3,3,327,57,"MESHTASTIC  |  MOCK","Trail group: Checkpoint reached");
    card(3,65,327,57,"MESHCORE  |  MOCK","Field team: Copy, all good");
    card(3,127,327,57,"SAFETY","Messages are examples. Nothing is sent.");
  } else if(page==2) {
    card(3,3,327,152,"MAP / NAVIGATION","Map tiles not loaded in offline demo.\nGNSS coordinates simulated.\nReal position sharing: DISABLED.");
    card(3,161,327,55,"FIELD TEST","Touch response + navigation enabled");
  } else if(page==3) {
    card(3,3,160,159,"RADIO A - MESH","Meshtastic (MOCK)\nUART not connected\nRSSI -87 dBm\nSNR 8.2 dB");
    card(170,3,160,159,"RADIO B - CORE","MeshCore (MOCK)\nUART not connected\nRSSI -94 dBm\nSNR 5.7 dB");
    label(content,"No packets transmitted.",10,177,muted);
  } else {
    card(3,3,327,153,"DEVICE SETTINGS","Mode: UI-only demo\nDisplay: 480 x 320 landscape\nTouch: enabled\nRadio A/B: simulated\nGNSS: simulated");
    label(content,"Hardware integration follows later.",10,176,muted);
  }
}
static void on_nav(lv_event_t *e) {
  page=(int)(intptr_t)lv_event_get_user_data(e);
  draw();
}
static void on_timer(lv_timer_t *) {
  ++tick;
  // Keep a genuine UI update in the prototype without implying real radio telemetry.
  if(page==0 && tick%5==0) draw();
}
static void init() {
  lv_obj_t *screen=lv_scr_act();
  style(screen,bg,0);
  lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *top=lv_obj_create(screen);
  lv_obj_set_pos(top,0,0); lv_obj_set_size(top,480,45);
  style(top,panel,0); lv_obj_clear_flag(top,LV_OBJ_FLAG_SCROLLABLE);
  label(top,"MeshCrossway",10,8,lv_color_white());
  title=label(top,"HOME",207,8,bright);
  lv_obj_t *side=lv_obj_create(screen);
  lv_obj_set_pos(side,0,45); lv_obj_set_size(side,134,275);
  style(side,bg,0);
  lv_obj_clear_flag(side,LV_OBJ_FLAG_SCROLLABLE);
  const char *navtext[]={"Home","Messages","Map","Radios","Settings"};
  for(int i=0;i<5;i++) {
    nav[i]=lv_btn_create(side);
    lv_obj_set_pos(nav[i],1,4+i*49);
    lv_obj_set_size(nav[i],113,44);
    lv_obj_set_style_pad_all(nav[i],0,0);
    lv_obj_add_event_cb(nav[i],on_nav,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    lv_obj_t *t=lv_label_create(nav[i]);
    lv_label_set_text(t,navtext[i]);
    lv_obj_center(t);
  }
  content=lv_obj_create(screen);
  lv_obj_set_pos(content,134,45); lv_obj_set_size(content,346,242);
  style(content,bg,0);
  lv_obj_set_style_pad_all(content,0,0);
  lv_obj_clear_flag(content,LV_OBJ_FLAG_SCROLLABLE);
  status=label(screen,"DEMO",145,294,muted);
  draw();
  lv_timer_create(on_timer,1000,nullptr);
}
}  // namespace meshcrossway
