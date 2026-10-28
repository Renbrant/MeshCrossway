#pragma once
// MeshCrossway v0.5.5: local Wi-Fi provisioning; remote UI disabled.
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
static lv_obj_t *wifi_dialog=nullptr,*wifi_ssid=nullptr,*wifi_password=nullptr,*wifi_keyboard=nullptr,*wifi_notice=nullptr,*wifi_picker=nullptr;
static lv_obj_t *wifi_editor=nullptr,*wifi_edit_text=nullptr,*wifi_edit_target=nullptr;
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
static void close_wifi_editor(){
  if(!wifi_editor)return;
  // LVGL marks active events invalid when their object/ancestor is deleted.
  // Delete synchronously so closing the parent cannot leave a queued stale delete.
  lv_keyboard_set_textarea(wifi_keyboard,nullptr);
  lv_obj_t *editor=wifi_editor;
  wifi_editor=nullptr;wifi_edit_text=nullptr;wifi_edit_target=nullptr;wifi_keyboard=nullptr;
  lv_obj_del(editor);
}
static void close_wifi(){
  close_wifi_editor();
  if(wifi_dialog)lv_obj_del(wifi_dialog);
  wifi_dialog=nullptr;
  wifi_ssid=nullptr;wifi_password=nullptr;wifi_keyboard=nullptr;wifi_notice=nullptr;wifi_picker=nullptr;
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
    lv_label_set_text(wifi_notice,"Wait for scan, then retry CONNECT.");
    return;
  }
  // Keep the Wi-Fi credentials in ESP-IDF NVS, not in app source or LVGL fields.
  lv_textarea_set_text(wifi_password,"");
  close_wifi();
  page=4;
  draw();
}
static void on_scan(lv_event_t *){
  if(meshnetwork::begin_scan())lv_label_set_text(wifi_notice,"Scanning 2.4 GHz networks...");
  else lv_label_set_text(wifi_notice,"Scan unavailable; enter SSID manually.");
}
static void on_network_selected(lv_event_t *){
  if(!wifi_picker||!wifi_ssid)return;
  uint16_t selected=lv_dropdown_get_selected(wifi_picker);
  if(selected>0&&selected<=meshnetwork::scan_count)
    lv_textarea_set_text(wifi_ssid,meshnetwork::scan_ssids[selected-1]);
}
static void update_wifi_scan(){
  if(!meshnetwork::collect_scan())return;
  if(!wifi_dialog||!wifi_picker)return;
  char options[540]="Select network (or enter SSID)";
  for(uint16_t i=0;i<meshnetwork::scan_count;i++){
    if(strlen(options)+strlen(meshnetwork::scan_ssids[i])+2>=sizeof(options))break;
    strcat(options,"\n");
    strcat(options,meshnetwork::scan_ssids[i]);
  }
  // An open dropdown label points directly at the old options buffer.
  // Close it before replacement so rendering cannot read freed option text.
  lv_dropdown_close(wifi_picker);
  lv_dropdown_set_options(wifi_picker,options);
  lv_label_set_text(wifi_notice,meshnetwork::scan_count?
                    "Select SSID, then enter password below.":"No networks found; enter SSID manually.");
}
static void finish_wifi_edit(lv_event_t *){
  if(wifi_edit_target&&wifi_edit_text)
    lv_textarea_set_text(wifi_edit_target,lv_textarea_get_text(wifi_edit_text));
  close_wifi_editor();
}
static void cancel_wifi_edit(lv_event_t *){close_wifi_editor();}
static void toggle_wifi_password(lv_event_t *e){
  if(!wifi_edit_text)return;
  const bool masked=lv_textarea_get_password_mode(wifi_edit_text);
  lv_textarea_set_password_mode(wifi_edit_text,!masked);
  lv_label_set_text(lv_obj_get_child(lv_event_get_target(e),0),masked?"HIDE":"SHOW");
}
static void on_wifi_field(lv_event_t *e){
  if(wifi_editor||!wifi_dialog)return;
  wifi_edit_target=lv_event_get_target(e);
  const bool password=wifi_edit_target==wifi_password;
  lv_dropdown_close(wifi_picker);
  wifi_editor=lv_obj_create(wifi_dialog);
  fill(wifi_editor,bg);
  lv_obj_set_align(wifi_editor,LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(wifi_editor,0,0);lv_obj_set_size(wifi_editor,480,320);
  lv_obj_clear_flag(wifi_editor,LV_OBJ_FLAG_SCROLLABLE);
  txt(wifi_editor,password?"WI-FI PASSWORD":"NETWORK NAME / SSID",8,10,245,orange);
  lv_obj_t *cancel=lv_btn_create(wifi_editor);
  fill(cancel,panel2,6);lv_obj_set_pos(cancel,276,4);lv_obj_set_size(cancel,94,32);
  txt(cancel,"CANCEL",14,8,78,white);
  lv_obj_add_event_cb(cancel,cancel_wifi_edit,LV_EVENT_CLICKED,nullptr);
  lv_obj_t *done=lv_btn_create(wifi_editor);
  fill(done,orange,6);lv_obj_set_pos(done,376,4);lv_obj_set_size(done,96,32);
  txt(done,"DONE",23,8,70,bg);
  lv_obj_add_event_cb(done,finish_wifi_edit,LV_EVENT_CLICKED,nullptr);
  wifi_edit_text=lv_textarea_create(wifi_editor);
  lv_obj_set_pos(wifi_edit_text,6,42);lv_obj_set_size(wifi_edit_text,password?392:468,48);
  lv_textarea_set_one_line(wifi_edit_text,true);
  lv_obj_set_height(wifi_edit_text,48);
  lv_textarea_set_max_length(wifi_edit_text,password?63:32);
  lv_textarea_set_password_mode(wifi_edit_text,password);
  lv_textarea_set_text(wifi_edit_text,lv_textarea_get_text(wifi_edit_target));
  lv_textarea_set_cursor_pos(wifi_edit_text,LV_TEXTAREA_CURSOR_LAST);
  if(password){
    lv_obj_t *show=lv_btn_create(wifi_editor);
    fill(show,panel2,6);lv_obj_set_pos(show,404,42);lv_obj_set_size(show,70,48);
    txt(show,"SHOW",10,16,60,white);
    lv_obj_add_event_cb(show,toggle_wifi_password,LV_EVENT_CLICKED,nullptr);
  }
  wifi_keyboard=lv_keyboard_create(wifi_editor);
  lv_obj_set_align(wifi_keyboard,LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(wifi_keyboard,4,96);lv_obj_set_size(wifi_keyboard,472,218);
  lv_keyboard_set_mode(wifi_keyboard,LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(wifi_keyboard,wifi_edit_text);
  lv_obj_set_style_bg_color(wifi_keyboard,panel,0);
  lv_obj_set_style_pad_all(wifi_keyboard,3,0);
  lv_obj_set_style_pad_row(wifi_keyboard,4,0);
  lv_obj_set_style_pad_column(wifi_keyboard,4,0);
  lv_obj_add_event_cb(wifi_keyboard,finish_wifi_edit,LV_EVENT_READY,nullptr);
  lv_obj_add_event_cb(wifi_keyboard,cancel_wifi_edit,LV_EVENT_CANCEL,nullptr);
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

  wifi_picker=lv_dropdown_create(wifi_dialog);
  lv_obj_set_pos(wifi_picker,9,43);lv_obj_set_size(wifi_picker,352,37);
  lv_dropdown_set_options(wifi_picker,"Select network (or enter SSID)");
  lv_dropdown_set_dir(wifi_picker,LV_DIR_BOTTOM);
  lv_obj_set_style_max_height(lv_dropdown_get_list(wifi_picker),200,0);
  lv_obj_add_event_cb(wifi_picker,on_network_selected,LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_t *scan=lv_btn_create(wifi_dialog);
  fill(scan,orange,6);lv_obj_set_pos(scan,369,43);lv_obj_set_size(scan,102,37);
  txt(scan,"SCAN",21,9,80,bg);
  lv_obj_add_event_cb(scan,on_scan,LV_EVENT_CLICKED,nullptr);

  wifi_ssid=lv_textarea_create(wifi_dialog);
  lv_obj_set_pos(wifi_ssid,9,85);lv_obj_set_size(wifi_ssid,462,34);
  lv_textarea_set_one_line(wifi_ssid,true);
  lv_textarea_set_max_length(wifi_ssid,32);
  lv_textarea_set_placeholder_text(wifi_ssid,"Network name (SSID)");
  lv_obj_add_event_cb(wifi_ssid,on_wifi_field,LV_EVENT_CLICKED,nullptr);

  wifi_password=lv_textarea_create(wifi_dialog);
  lv_obj_set_pos(wifi_password,9,123);lv_obj_set_size(wifi_password,337,36);
  lv_textarea_set_one_line(wifi_password,true);
  lv_textarea_set_max_length(wifi_password,63);
  lv_textarea_set_password_mode(wifi_password,true);
  lv_textarea_set_placeholder_text(wifi_password,"Wi-Fi password");
  lv_obj_add_event_cb(wifi_password,on_wifi_field,LV_EVENT_CLICKED,nullptr);

  lv_obj_t *connect=lv_btn_create(wifi_dialog);
  fill(connect,orange,7);lv_obj_set_pos(connect,351,123);lv_obj_set_size(connect,120,36);
  txt(connect,"CONNECT",22,9,92,bg);
  lv_obj_add_event_cb(connect,on_connect,LV_EVENT_CLICKED,nullptr);

  wifi_notice=txt(wifi_dialog,"Tap SCAN to find nearby Wi-Fi networks.",10,162,453,muted);
  txt(wifi_dialog,"Tap the SSID or password field to edit.",10,207,453,white);
  txt(wifi_dialog,"Full-screen QWERTY / DONE returns here.",10,234,453,muted);
  (void)meshnetwork::begin_scan();

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
  update_wifi_scan();
  printf("[MCX-DIAG v0.5.5] uptime_ms=%llu heap=%lu page=%d\n",
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
  printf("[MCX-DIAG v0.5.5] boot reset_reason=%d heap=%lu remote=OFF wifi=ON\n",
         (int)esp_reset_reason(),(unsigned long)esp_get_free_heap_size());
  lv_obj_t *screen=lv_scr_act();fill(screen,bg);
  lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *bar=lv_obj_create(screen);fill(bar,panel);
  lv_obj_set_pos(bar,0,0);lv_obj_set_size(bar,480,43);
  lv_obj_clear_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
  txt(bar,"MESH",10,11,49,white);
  txt(bar,"CROSSWAY",59,11,104,orange);
  heading=txt(bar,"OVERVIEW",185,11,170,white);
  txt(bar,"DEV 0.5.5",386,12,90,orange);
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
