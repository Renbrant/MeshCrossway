#pragma once
// MeshCrossway v0.7.8: swipe-open sidebars without arrow buttons.
// MeshCore is live over BLE. Meshtastic and navigation remain previews.
#include "lvgl.h"
#include "meshcrossway_health.h"
#include "meshcrossway_ble.h"
#include "meshcrossway_device_key.h"
#include "meshcrossway_brand.h"
#include "meshcrossway_edge_swipe.h"
#include "meshcrossway_power_status.h"
#include "meshcrossway_chat_presentation.h"
#include "emoji/emoji_display.h"
#define MCX_EMOJI_RENDER_ENABLED 1
#include "esp_system.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace meshcrossway {
static lv_obj_t *body=nullptr,*heading=nullptr,*tabs[6]={};
static lv_obj_t *wifi_dialog=nullptr,*wifi_ssid=nullptr,*wifi_password=nullptr,*wifi_keyboard=nullptr,*wifi_notice=nullptr,*wifi_picker=nullptr;
static lv_obj_t *wifi_editor=nullptr,*wifi_edit_text=nullptr,*wifi_edit_target=nullptr;
static lv_obj_t *wifi_sd_status=nullptr;
static int page=0;
static lv_obj_t *uptime_value=nullptr,*heap_value=nullptr,*network_value=nullptr,*backup_value=nullptr;
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
  wifi_dialog=nullptr;wifi_sd_status=nullptr;
  wifi_ssid=nullptr;wifi_password=nullptr;wifi_keyboard=nullptr;wifi_notice=nullptr;wifi_picker=nullptr;
}
static void on_cancel(lv_event_t *){close_wifi();}
static void on_connect(lv_event_t *){
  if(!wifi_ssid || !wifi_password)return;
  const char *ssid=lv_textarea_get_text(wifi_ssid);
  const char *password=lv_textarea_get_text(wifi_password);
  const size_t n=strlen(ssid),p=strlen(password);
  if(n<1 || n>32 || p>63 || (p<8 && !(p==0 && meshnetwork::known_network(ssid)))){
    lv_label_set_text(wifi_notice,"SSID: 1-32 chars / password or known network");
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
// Static maps: no stack-backed pointers; one insertion per completed tap.
#define MCX_KEY_FLAGS (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG)
static const char *wifi_qwerty_lower[]={"q","w","e","r","t","y","u","i","o","p","\n","a","s","d","f","g","h","j","k","l","\n","ABC","z","x","c","v","b","n","m",LV_SYMBOL_BACKSPACE,"\n","1#",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
static const lv_btnmatrix_ctrl_t wifi_qwerty_lower_ctrl[]={MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 4,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1};
static const char *wifi_qwerty_upper[]={"Q","W","E","R","T","Y","U","I","O","P","\n","A","S","D","F","G","H","J","K","L","\n","abc","Z","X","C","V","B","N","M",LV_SYMBOL_BACKSPACE,"\n","1#",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
static const lv_btnmatrix_ctrl_t wifi_qwerty_upper_ctrl[]={MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 4,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1};
static const char *wifi_qwerty_symbols[]={"1","2","3","4","5","6","7","8","9","0","\n","!","@","#","$","%","^","&","*","(",")","\n","-","_","+","=","[","]","{","}","<",">","\n","/","\\","|",";",":","'","\"",",",".","?","\n","abc","`","~",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_BACKSPACE,""};
static const lv_btnmatrix_ctrl_t wifi_qwerty_symbols_ctrl[]={MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 4,MCX_KEY_FLAGS | 1,MCX_KEY_FLAGS | 1};
#undef MCX_KEY_FLAGS
static uint32_t editor_key_count=0;
static uint64_t editor_key_last_us=0,editor_key_max_us=0;
static void editor_key_event(lv_event_t *e){
  ++editor_key_count;
  const int64_t started=esp_timer_get_time();
  lv_keyboard_def_event_cb(e);
  // The default callback can close the editor; never dereference its event/object here.
  editor_key_last_us=(uint64_t)(esp_timer_get_time()-started);
  if(editor_key_last_us>editor_key_max_us)editor_key_max_us=editor_key_last_us;
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
  lv_keyboard_set_map(wifi_keyboard,LV_KEYBOARD_MODE_TEXT_LOWER,wifi_qwerty_lower,wifi_qwerty_lower_ctrl);
  lv_keyboard_set_map(wifi_keyboard,LV_KEYBOARD_MODE_TEXT_UPPER,wifi_qwerty_upper,wifi_qwerty_upper_ctrl);
  lv_keyboard_set_map(wifi_keyboard,LV_KEYBOARD_MODE_SPECIAL,wifi_qwerty_symbols,wifi_qwerty_symbols_ctrl);
  lv_keyboard_set_mode(wifi_keyboard,LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_remove_event_cb(wifi_keyboard,lv_keyboard_def_event_cb);
  lv_obj_add_event_cb(wifi_keyboard,editor_key_event,LV_EVENT_VALUE_CHANGED,nullptr);
  lv_keyboard_set_textarea(wifi_keyboard,wifi_edit_text);
  lv_obj_set_style_bg_color(wifi_keyboard,panel,0);
  lv_obj_set_style_pad_all(wifi_keyboard,3,0);
  lv_obj_set_style_pad_row(wifi_keyboard,4,0);
  lv_obj_set_style_pad_column(wifi_keyboard,2,0);
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
  wifi_sd_status=txt(wifi_dialog,"SD backup: checking",10,270,453,muted);
  (void)meshnetwork::begin_scan();

}
// Field console v0.6: all geometry uses a real 480x320 landscape display.
// Left navigation and contextual right drawer can be independently collapsed.
// Keep Wi-Fi provisioning and the manual screen viewer independent of UI rebuilds.
static lv_obj_t *left_rail=nullptr,*right_rail=nullptr,*context_actions=nullptr;
static lv_obj_t *context_scroll_track=nullptr,*context_scroll_thumb=nullptr;
static lv_obj_t *top_wifi=nullptr,*top_mt=nullptr,*top_mc=nullptr;
static lv_obj_t *top_mt_name=nullptr,*top_mc_name=nullptr,*top_charge=nullptr;
static lv_obj_t *top_battery=nullptr,*top_battery_fill=nullptr,*top_battery_percent=nullptr;
static lv_obj_t *shade=nullptr,*info_dialog=nullptr;
static bool left_compact=true,right_compact=true;
static int nodes_filter=0,groups_filter=0,active_thread=-1,more_view=0;
static uint32_t ble_generation=0;
static lv_obj_t *ble_dialog=nullptr,*ble_pin=nullptr,*ble_pin_keyboard=nullptr,*ble_pin_notice=nullptr;
static lv_obj_t *tab_vectors[6]={};
// Persistent selection is independent of Nodes/Groups filtering and page navigation.
static const char *thread_names[]={"Alex","Jamie","LongFast","#public","Backcountry","SAR Room"};
static const char *thread_networks[]={"MT / DIRECT","MC / DIRECT","MT / CHANNEL",
                                     "MC / CHANNEL","MT / CHANNEL","MC / ROOM"};
static int map_view=0; // 0 map, 1 POIs, 2 tracks, 3 GNSS, 4 team, 5 sharing.
static const lv_color_t line_color=lv_color_hex(0x494039);
static const lv_color_t purple=lv_color_hex(0xB99AFF);
static const lv_color_t green=lv_color_hex(0x72D8A5);
static const lv_color_t cyan=lv_color_hex(0x6CBDE6);
static const lv_color_t red=lv_color_hex(0xFF7777);
static const char *tab_names[6]={"Home","Nodes","Groups","Chats","Map","More"};
static const char *tab_glyphs[6]={LV_SYMBOL_HOME,"","","",LV_SYMBOL_GPS,LV_SYMBOL_SETTINGS};
enum Action {
  A_MAP_HOME,A_INBOX,A_NEW,A_POSITION,A_NETWORKS,A_WIFI,A_POIS,A_TRACKS,A_RECORD,A_SHARE,
  A_GNSS,A_TEAM,A_PATHS,A_SCANNER,A_PACKETS,A_REPEATERS,A_SETUP,A_DIAGNOSTIC,
  A_VIEW,A_PRIVACY,A_ABOUT,A_CHAT_MT,A_CHAT_MC,A_CHAT_ALL,A_QUICK,
  A_NODES,A_GROUPS,A_CHAT_OPEN,A_RADIOS,A_GROUP_NEW,A_THREAD_INFO,A_NODE_DETAILS,A_BLE_SCAN,A_BLE_DISCONNECT,A_BLE_ENABLE,A_SETTINGS,A_RETURN_HOME
};
struct ActionInfo {const char *label;int id;lv_color_t color;};
// No right rail on Home, Nodes, Groups or Settings. Those screens already
// offer primary navigation or filters in their main content.
static const ActionInfo chat_actions[]={
  {"Compose",A_NEW,orange},{"Chat tools",A_PACKETS,cyan},{"Chat info",A_THREAD_INFO,purple}
};
static const ActionInfo map_actions[]={
  {"Map view",A_MAP_HOME,orange},{"POIs",A_POIS,orange},{"Tracks",A_TRACKS,purple},
  {"Record",A_RECORD,red},{"Live share",A_SHARE,green},
  {"GNSS",A_GNSS,cyan},{"Team",A_TEAM,orange}
};
static void draw(); // Forward-declared above Wi-Fi editor callbacks.
static bool edge_swipe_click_in_progress();
static bool has_context_rail(){return page==3||page==4;}
static bool has_left_rail(){return true;} // Main navigation never disappears, even in Settings / Radios.
static int content_width(){return 480-(has_left_rail()?(left_compact?40:79):0)
                                   -(has_context_rail()?(right_compact?40:125):0);}
// Keep labels one line high: wrapping formerly overlapped adjacent touch targets.
static lv_obj_t *single_label(lv_obj_t *parent,const char *value,int x,int y,int w,lv_color_t color){
  lv_obj_t *label=txt(parent,value,x,y,w,color);
  lv_obj_set_height(label,19);
  lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
  return label;
}
static lv_obj_t *panel_at(lv_obj_t *parent,int x,int y,int w,int h,lv_color_t color,int radius=8){
  lv_obj_t *o=lv_obj_create(parent);fill(o,color,radius);
  lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *color_label(lv_obj_t *parent,const char *value,int x,int y,int w,lv_color_t color){
  return txt(parent,value,x,y,w,color);
}
static lv_obj_t *action_button(lv_obj_t *parent,const char *label,int x,int y,int w,int h,
                               lv_color_t color,lv_event_cb_t callback,intptr_t data){
  lv_obj_t *b=lv_btn_create(parent);fill(b,color,7);
  lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
  lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,(void*)data);
  lv_obj_t *l=txt(b,label,6,(h-18)/2,w-12,white);
  lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
  return b;
}
// Lightweight LVGL shapes avoid depending on emoji fonts or extra image assets.
static lv_obj_t *nav_shape(lv_obj_t *parent,int x,int y,int w,int h,int radius){
  lv_obj_t *shape=panel_at(parent,x,y,w,h,orange,radius);
  lv_obj_clear_flag(shape,LV_OBJ_FLAG_CLICKABLE);
  return shape;
}
static void make_nav_vector(int index){
  if(index!=1&&index!=2&&index!=3)return;
  lv_obj_t *v=lv_obj_create(tabs[index]);fill(v,panel2,0);
  lv_obj_set_style_bg_opa(v,LV_OPA_TRANSP,0);
  lv_obj_set_size(v,23,21);
  lv_obj_clear_flag(v,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(v,LV_OBJ_FLAG_SCROLLABLE);
  tab_vectors[index]=v;
  if(index==1){
    // Handheld radio: antenna, chassis, frequency display and rotary knob.
    nav_shape(v,5,0,2,7,0);
    nav_shape(v,3,5,19,15,3);
    nav_shape(v,6,9,9,2,0);
    nav_shape(v,6,13,9,2,0);
    nav_shape(v,17,10,3,3,2);
  }else if(index==2){
    // Two people: heads and shoulders, visible even at 34px compact rail width.
    nav_shape(v,3,2,7,7,5);
    nav_shape(v,13,2,7,7,5);
    nav_shape(v,1,12,11,7,4);
    nav_shape(v,12,12,10,7,4);
  }else{
    // Chat bubble with a pointed tail and two visible message dots.
    nav_shape(v,2,2,19,13,4);
    nav_shape(v,6,14,6,4,0);
    // Dark dots remain visible in both active/inactive buttons.
    lv_obj_t *d1=nav_shape(v,8,8,2,2,2),*d2=nav_shape(v,14,8,2,2,2);
    lv_obj_set_style_bg_color(d1,panel2,0);
    lv_obj_set_style_bg_color(d2,panel2,0);
  }
}
static void update_nav_vector(int index,lv_color_t tint){
  lv_obj_t *v=tab_vectors[index];if(!v)return;
  const uint32_t n=lv_obj_get_child_cnt(v);
  for(uint32_t j=0;j<n;j++){
    lv_obj_t *child=lv_obj_get_child(v,j);
    if((index==3&&j>=n-2)||(index==1&&j>=2))
      lv_obj_set_style_bg_color(child,index==page?orange:panel2,0);
    else lv_obj_set_style_bg_color(child,tint,0);
  }
}
static void info_close_now(void *){
  if(!info_dialog)return;
  lv_obj_t *old=info_dialog;info_dialog=nullptr;lv_obj_del(old);
}
static void info_close(lv_event_t *){lv_async_call(info_close_now,nullptr);}
static void close_shade(){
  if(!shade)return;
  lv_obj_t *old=shade;shade=nullptr;lv_obj_del(old);
}
static void shade_close_now(void *){close_shade();}
static void shade_close(lv_event_t *){lv_async_call(shade_close_now,nullptr);}
static void notice(const char *title,const char *body_text){
  close_shade();
  if(info_dialog)info_close_now(nullptr);
  // Put informational panels on the active screen so read-only capture shows them.
  info_dialog=panel_at(lv_scr_act(),20,45,440,220,panel,10);
  color_label(info_dialog,title,15,13,298,orange);
  action_button(info_dialog,"CLOSE",327,7,103,38,panel2,info_close,0);
  lv_obj_t *message=txt(info_dialog,body_text,15,65,410,white);
  lv_label_set_long_mode(message,LV_LABEL_LONG_WRAP);
  lv_obj_set_height(message,143);
}
namespace mcui {static void tools();static void compose();static void form(uint8_t);static void thread_info();}
static void run_action(void *data){
  int id=(int)(intptr_t)data;
  if(id==A_NEW){mcui::compose();return;}
  if(id==A_GROUP_NEW){mcui::form(32);return;}
  if(id==A_THREAD_INFO){mcui::thread_info();return;}
  if(id==A_PACKETS||id==A_NETWORKS||id==A_NODE_DETAILS){mcui::tools();return;}
  if(id==A_WIFI){close_shade();wifi_setup(nullptr);return;}
  if(id==A_BLE_ENABLE){(void)meshble::request_start();page=5;more_view=1;draw();return;}
  if(id==A_BLE_SCAN){meshble::scan();page=5;more_view=1;draw();return;}
  if(id==A_BLE_DISCONNECT){meshble::disconnect();page=5;more_view=1;draw();return;}
  if(id==A_NODES){page=1;draw();return;}
  if(id==A_GROUPS||id==A_INBOX){page=2;draw();return;}
  if(id==A_CHAT_OPEN){page=3;draw();return;}
  if(id==A_MAP_HOME){page=4;map_view=0;draw();return;}
  if(id==A_RADIOS){page=5;more_view=1;draw();return;}
  if(id==A_SETTINGS){page=5;more_view=0;draw();return;}
  if(id==A_RETURN_HOME){page=0;more_view=0;draw();return;}
  if(id==A_CHAT_ALL||id==A_CHAT_MT||id==A_CHAT_MC){
    int filter=id==A_CHAT_MT?1:(id==A_CHAT_MC?2:0);
    if(page==1)nodes_filter=filter;else if(page==2)groups_filter=filter;
    draw();return;
  }
  if(id==A_POSITION||id==A_POIS||id==A_TRACKS||id==A_RECORD||id==A_SHARE||
     id==A_GNSS||id==A_TEAM||id==A_PATHS){
    if(id==A_POIS)map_view=1;else if(id==A_TRACKS||id==A_PATHS)map_view=2;
    else if(id==A_GNSS)map_view=3;else if(id==A_TEAM)map_view=4;
    else if(id==A_SHARE)map_view=5;else if(id==A_POSITION)map_view=0;
    page=4;draw();
    if(id!=A_RECORD)return;
  }
  if(id==A_NETWORKS||id==A_SCANNER||id==A_PACKETS||id==A_REPEATERS){
    page=5;more_view=1;draw();
  }
  if(id==A_SETUP||id==A_DIAGNOSTIC||id==A_VIEW||id==A_PRIVACY||id==A_ABOUT){
    page=5;more_view=0;draw();
  }
  char summary[320];
  switch(id){
    case A_NEW:
      notice("COMPOSE / DEMO","Radio BLE connections are not established. No message was sent or queued.");break;
    case A_GROUP_NEW:
      notice("GROUPS / DEMO","Creating or importing a Meshtastic channel or MeshCore channel requires the respective radio. No group was created.");break;
    case A_THREAD_INFO:
      if(active_thread<0)notice("CHAT / NONE","Choose a node or group first. No active conversation.");
      else {
        snprintf(summary,sizeof(summary),"%s / %s\nIllustrative conversation only. BLE is offline; no messages are sent.",thread_networks[active_thread],thread_names[active_thread]);
        notice("CHAT DETAILS",summary);
      }
      break;
    case A_POIS:
      notice("POINTS OF INTEREST","POI lists, categories, save and edit are planned. No live coordinates.");break;
    case A_TRACKS:
      notice("SAVED TRACKS","Track folders, GPX import and saved paths will require SD integration. Demo only.");break;
    case A_RECORD:
      notice("TRACK RECORDING / OFF","GNSS is not connected; recording cannot start. Nothing was saved.");break;
    case A_SHARE:
      notice("LIVE LOCATION / OFF","GNSS fix, destination, consent and airtime checks required; sharing disabled.");break;
    case A_GNSS:case A_POSITION:
      notice("GNSS / OFFLINE","Position and satellite data require the MAX-M10S integration.");break;
    case A_TEAM:
      notice("TEAM POSITIONS / DEMO","No live BLE peer positions available.");break;
    case A_PATHS:
      notice("PATHS / DEMO","Routing and downloaded basemap tiles are not yet available.");break;
    case A_SCANNER:
      notice("DISCOVERY / OFFLINE","Radio pods are disconnected. These lists are samples; no BLE scan was started.");break;
    case A_PACKETS:
      notice("PACKET LOG / EMPTY","No live packets have been received.");break;
    case A_NODE_DETAILS:
      notice("NODE / REPEATER / DEMO","This entry is an illustrative relay, not a verified live contact. Repeater details, capabilities and last-heard time require a connected radio.");break;
    case A_REPEATERS:
      notice("REPEATERS / DEMO","A repeater relays packets, not a normal direct-message contact. Management is not enabled.");break;
    case A_NETWORKS:
      notice("DUAL MESH / OFFLINE","Meshtastic and MeshCore are independent BLE radio networks. Neither radio is connected.");break;
    case A_DIAGNOSTIC:{
      const unsigned long up=(unsigned long)(esp_timer_get_time()/1000000LL);
      const mcxkey::Audit audit=mcxkey::device_audit();
      char key_summary[192];
      mcxkey::describe(audit,key_summary,sizeof(key_summary));
      snprintf(summary,sizeof(summary),"Uptime: %lu s\nFree heap: %lu KB\nWi-Fi backup: %s\n%s",
               up,(unsigned long)(esp_get_free_heap_size()/1024),meshbackup::status(),key_summary);
      notice("DEVICE / LIVE",summary);break;
    }
    case A_VIEW:
      snprintf(summary,sizeof(summary),"Open http://DEVICE_IP/ on your trusted local network.\nNo PIN required.\nManual screenshots only; remote touch disabled.");
      notice("REMOTE VIEW / READ ONLY",summary);break;
    case A_PRIVACY:
      notice("PRIVACY","Location sharing and radio transmission are off. SD Wi-Fi backup is plaintext; protect your SD card.");break;
    case A_ABOUT:
      notice("MESH CROSSWAY v0.7.8","Dual mesh field console / demonstration. No operational radio or GNSS yet.");break;
    case A_QUICK:
      notice("QUICK SETTINGS","Tap the top bar for quick settings. Wi-Fi works; other controls are previews.");break;
    default:break;
  }
}
static void on_action(lv_event_t *e){
  if(edge_swipe_click_in_progress())return;
  lv_async_call(run_action,lv_event_get_user_data(e));
}
static bool suppress_left_nav_click=false;
static void on_nav(lv_event_t *e){
  if(suppress_left_nav_click||edge_swipe_click_in_progress())return;
  page=(int)(intptr_t)lv_event_get_user_data(e);
  if(page==5)more_view=0;
  close_shade();draw();
}
// Poll the existing LVGL touch device from the LVGL thread. Unlike
// LV_EVENT_GESTURE, this also works when a scrollable chat or map is visible;
// only edge-armed horizontal movements may change layout.
static mcxswipe::State swipe_state={};
static lv_indev_t *swipe_pointer=nullptr;
static bool left_drag_detected=false;
static lv_point_t left_drag_start={};
static bool left_drag_was_compact=true;
static void finish_left_drag(void *){
  left_compact=!left_drag_was_compact;
  left_drag_detected=false;
  suppress_left_nav_click=false;
  mcxswipe::reset(swipe_state);
  draw();
}
static void left_tab_touch(lv_event_t *e){
  const lv_event_code_t kind=lv_event_get_code(e);
  lv_indev_t *dev=lv_indev_get_act();
  if(!dev||lv_indev_get_type(dev)!=LV_INDEV_TYPE_POINTER)return;
  lv_point_t p={};lv_indev_get_point(dev,&p);
  if(kind==LV_EVENT_PRESSED){
    left_drag_start=p;
    left_drag_was_compact=left_compact;
    left_drag_detected=false;
    suppress_left_nav_click=false;
  }else if(kind==LV_EVENT_PRESSING){
    const int dx=int(p.x)-left_drag_start.x;
    const int dy=int(p.y)-left_drag_start.y;
    if(!left_drag_detected&&mcxswipe::iabs(dx)>=28 &&
       mcxswipe::iabs(dx)>=2*mcxswipe::iabs(dy) &&
       ((left_drag_was_compact&&dx>0)||(!left_drag_was_compact&&dx<0))){
      left_drag_detected=true;
      suppress_left_nav_click=true;
    }
  }else if(kind==LV_EVENT_RELEASED && left_drag_detected){
    lv_async_call(finish_left_drag,nullptr);
  }
}
static void edge_swipe_poll(lv_timer_t *){
  if(!swipe_pointer){
    for(lv_indev_t *d=lv_indev_get_next(nullptr);d;d=lv_indev_get_next(d))
      if(lv_indev_get_type(d)==LV_INDEV_TYPE_POINTER){swipe_pointer=d;break;}
  }
  if(!swipe_pointer)return;
  lv_point_t point={};lv_indev_get_point(swipe_pointer,&point);
  // ESP-IDF's pinned LVGL 8.4 has no lv_indev_get_state() API (LVGL 9 only).
  // The v8 public lv_hal_indev.h exposes the current state in proc.state.
  const bool pressed=swipe_pointer->proc.state==LV_INDEV_STATE_PRESSED;
  const bool modal=wifi_dialog||wifi_editor||ble_dialog||info_dialog||shade;
  const int lw=has_left_rail()?(left_compact?40:79):0;
  const int rw=has_context_rail()?(right_compact?40:125):0;
  const mcxswipe::Result gesture=mcxswipe::sample(
    swipe_state,pressed,point.x,point.y,page,has_left_rail(),
    has_context_rail(),left_compact,right_compact,lw,rw,modal,lv_tick_get());
  switch(gesture){
    case mcxswipe::OPEN_LEFT:left_compact=false;break;
    case mcxswipe::CLOSE_LEFT:left_compact=true;break;
    case mcxswipe::OPEN_RIGHT:right_compact=false;break;
    case mcxswipe::CLOSE_RIGHT:right_compact=true;break;
    default:return;
  }
  // Redraw only once, after release. Never destroy widgets while pressed.
  draw();
}
// A horizontal swipe can end on a clickable icon. LVGL 8.x can emit CLICKED
// before the 20ms polling timer sees the released state. Suppress that click
// so dragging a rail never accidentally switches page or activates Compose.
static bool edge_swipe_click_in_progress(){
  if(!swipe_state.down||swipe_state.origin==mcxswipe::NOT_ARMED)return false;
  lv_indev_t *dev=lv_indev_get_act();
  if(!dev||lv_indev_get_type(dev)!=LV_INDEV_TYPE_POINTER)return false;
  lv_point_t p={};lv_indev_get_point(dev,&p);
  const int dx=int(p.x)-int(swipe_state.start_x);
  const int dy=int(p.y)-int(swipe_state.start_y);
  if(mcxswipe::iabs(dx)<44||mcxswipe::iabs(dx)<2*mcxswipe::iabs(dy))return false;
  const bool correct_direction=
    (swipe_state.origin==mcxswipe::LEFT_OPEN_ZONE&&dx>0)||
    (swipe_state.origin==mcxswipe::LEFT_CLOSE_ZONE&&dx<0)||
    (swipe_state.origin==mcxswipe::RIGHT_OPEN_ZONE&&dx<0)||
    (swipe_state.origin==mcxswipe::RIGHT_CLOSE_ZONE&&dx>0);
  if(!correct_direction)return false;
  swipe_state.last_x=p.x;swipe_state.last_y=p.y;
  return true;
}
static void apply_filter(void *value){
  const int filter=(int)(intptr_t)value;
  if(page==1)nodes_filter=filter;
  else if(page==2)groups_filter=filter;
  draw();
}
static void on_filter(lv_event_t *e){lv_async_call(apply_filter,lv_event_get_user_data(e));}
static void open_thread_now(void *value){active_thread=(int)(intptr_t)value;page=3;draw();}
static void on_thread(lv_event_t *e){lv_async_call(open_thread_now,lv_event_get_user_data(e));}
static void quick_action_run(void *value){
  int mode=(int)(intptr_t)value;
  if(mode==2){close_shade();wifi_setup(nullptr);return;}
  if(mode==1){close_shade();page=5;more_view=1;draw();return;}
  const char *names[]={"GPS","Bluetooth","","Sound","Brightness"};
  char text[160];snprintf(text,sizeof(text),"%s is a UI preview only. No device setting was changed.",
                           names[mode]);
  notice("QUICK SETTINGS / DEMO",text);
}
static void quick_action(lv_event_t *e){lv_async_call(quick_action_run,lv_event_get_user_data(e));}
static void open_shade(lv_event_t *){
  if(shade){close_shade();return;}
  if(wifi_dialog||info_dialog)return;
  // Full-height modal background hides the underlying page without touching Wi-Fi setup.
  shade=panel_at(lv_scr_act(),0,34,480,286,bg,0);
  lv_obj_set_style_bg_opa(shade,LV_OPA_80,0);
  lv_obj_t *sheet=panel_at(shade,8,8,464,235,panel,9);
  single_label(sheet,"QUICK SETTINGS / STATUS",12,10,340,orange);
  action_button(sheet,"CLOSE",366,6,89,37,panel2,shade_close,0);
  action_button(sheet,"GPS --",9,54,108,48,panel2,quick_action,0);
  action_button(sheet,"BLE --",122,54,108,48,panel2,quick_action,1);
  action_button(sheet,"WI-FI",235,54,108,48,orange,quick_action,2);
  action_button(sheet,"SOUND",348,54,107,48,panel2,quick_action,3);
  single_label(sheet,"Brightness is a preview; no hardware change.",12,119,438,muted);
  action_button(sheet,"DISPLAY / DEMO",9,152,219,48,panel2,quick_action,4);
  action_button(sheet,"DEVICE SETTINGS",235,152,220,48,orange,on_action,A_DIAGNOSTIC);
  single_label(sheet,"GPS / BLE / SOUND: DEMO ONLY",12,209,438,muted);
}
static void show_center_heading(const char *heading_text,const char *subtitle){
  const int w=lv_obj_get_width(body);
  single_label(body,heading_text,8,8,w-16,white);
  if(subtitle)single_label(body,subtitle,8,30,w-16,muted);
}
static lv_obj_t *center_card(int x,int y,int w,int h,const char *title,
                        const char *first,const char *second,lv_color_t color){
  lv_obj_t *card=panel_at(body,x,y,w,h,panel,8);
  single_label(card,title,8,8,w-16,color);
  if(first)single_label(card,first,8,32,w-16,white);
  if(second)single_label(card,second,8,h-23,w-16,muted);
  return card;
}
static void chat_row(int y,int w,const char *network,const char *who,const char *detail,lv_color_t color){
  lv_obj_t *button=action_button(body,"",8,y,w-16,53,panel,on_action,A_NEW);
  single_label(button,network,8,5,32,color);
  single_label(button,who,47,6,w-61,white);
  single_label(button,detail,47,30,w-61,muted);
}
static void demo_row(int y,int w,const char *network,const char *title,const char *detail,
                     lv_color_t tint,int thread){
  lv_obj_t *b=action_button(body,"",8,y,w-16,57,panel,
                            thread>=0?on_thread:on_action,
                            thread>=0?thread:A_NODE_DETAILS);
  single_label(b,network,8,5,45,tint);
  single_label(b,title,54,5,w-71,white);
  single_label(b,detail,54,29,w-71,muted);
}
static void filter_tabs(int w,int filter){
  const int third=(w-22)/3;
  const char *labels[]={"ALL","MT","MC"};
  for(int i=0;i<3;i++){
    lv_obj_t *b=action_button(body,labels[i],8+i*(third+3),52,third,38,
                              filter==i?orange:panel2,on_filter,i);
    if(filter==i)lv_obj_set_style_text_color(lv_obj_get_child(b,0),bg,0);
  }
}
static void map_preview(int w){
  const int width=w-16;
  lv_obj_t *map=panel_at(body,8,56,width,155,lv_color_hex(0x151B1B),6);
  for(int i=1;i<5;i++){
    lv_obj_t *vertical=lv_obj_create(map);fill(vertical,lv_color_hex(0x293B36),0);
    lv_obj_set_pos(vertical,i*(width/5),0);lv_obj_set_size(vertical,1,155);
  }
  for(int j=1;j<4;j++){
    lv_obj_t *horizontal=lv_obj_create(map);fill(horizontal,lv_color_hex(0x293B36),0);
    lv_obj_set_pos(horizontal,0,j*38);lv_obj_set_size(horizontal,width,1);
  }
  // Topographic-style contours are illustrative, not terrain or loaded map tiles.
  static lv_point_t topo_a[5]={{0,113},{50,92},{102,118},{160,83},{250,100}};
  static lv_point_t topo_b[5]={{0,68},{45,54},{90,74},{158,38},{250,52}};
  static lv_point_t topo_c[5]={{0,141},{45,125},{108,144},{178,117},{250,131}};
  // Keep sample contours proportional when the content area grows.
  const int ax[5]={0,50,102,160,250},bx[5]={0,45,90,158,250},cx[5]={0,45,108,178,250};
  for(int i=0;i<5;i++){
    topo_a[i].x=(lv_coord_t)(ax[i]*(width-16)/250);
    topo_b[i].x=(lv_coord_t)(bx[i]*(width-16)/250);
    topo_c[i].x=(lv_coord_t)(cx[i]*(width-16)/250);
  }
  lv_obj_t *contours[3]={lv_line_create(map),lv_line_create(map),lv_line_create(map)};
  lv_line_set_points(contours[0],topo_a,5);
  lv_line_set_points(contours[1],topo_b,5);
  lv_line_set_points(contours[2],topo_c,5);
  for(int k=0;k<3;k++){
    lv_obj_set_style_line_width(contours[k],2,0);
    lv_obj_set_style_line_color(contours[k],lv_color_hex(0x5B6B4C),0);
    lv_obj_set_pos(contours[k],0,0);
  }
  lv_obj_t *pin=lv_obj_create(map);
  fill(pin,orange,11);lv_obj_set_pos(pin,width/2-5,73);lv_obj_set_size(pin,11,11);
  lv_obj_set_style_border_color(pin,white,0);lv_obj_set_style_border_width(pin,2,0);
  lv_obj_t *label=panel_at(map,7,124,width-14,24,panel,4);
  color_label(label,"ILLUSTRATIVE / NO OFFLINE TILES",5,5,width-24,muted);
}
// Icons stay active even when the right rail is collapsed.
static const char *context_glyph(int id){
  switch(id){
    case A_NEW:return LV_SYMBOL_EDIT;
    case A_PACKETS:return LV_SYMBOL_LIST;
    case A_THREAD_INFO:return LV_SYMBOL_FILE;
    case A_MAP_HOME:return LV_SYMBOL_GPS;
    case A_POIS:return LV_SYMBOL_HOME;
    case A_TRACKS:return LV_SYMBOL_LIST;
    case A_RECORD:return LV_SYMBOL_PLAY;
    case A_SHARE:return LV_SYMBOL_UPLOAD;
    case A_GNSS:return LV_SYMBOL_WIFI;
    case A_TEAM:return LV_SYMBOL_DIRECTORY;
    default:return LV_SYMBOL_SETTINGS;
  }
}
// Place a separate, thin scroll indicator LEFT of the action buttons.
static void update_context_scroll_track(){
  if(!context_scroll_track||!context_scroll_thumb||!context_actions)return;
  if(page!=4||!has_context_rail()){
    lv_obj_add_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);return;
  }
  lv_obj_clear_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);
  // LVGL can temporarily report negative offsets during elastic overscroll.
  // Clamp the thumb to its own gutter and size it to the visible fraction.
  const int viewport=lv_obj_get_height(context_actions);
  const int scroll_y=lv_obj_get_scroll_y(context_actions);
  const int bottom=lv_obj_get_scroll_bottom(context_actions);
  const int range=(scroll_y>0?scroll_y:0)+(bottom>0?bottom:0);
  const int track=266;
  const int content=viewport+range;
  int thumb=content>0?(track*viewport)/content:track;
  if(thumb<32)thumb=32;
  if(thumb>track)thumb=track;
  const int offset=scroll_y<0?0:(scroll_y>range?range:scroll_y);
  const int thumb_y=range>0?(offset*(track-thumb))/range:0;
  lv_obj_set_pos(context_scroll_thumb,0,thumb_y);
  lv_obj_set_height(context_scroll_thumb,thumb);
}
static void context_scrolled(lv_event_t *){update_context_scroll_track();}
static void rebuild_context(){
  lv_obj_clean(context_actions);
  lv_obj_scroll_to_y(context_actions,0,LV_ANIM_OFF);
  if(!has_context_rail()){
    update_context_scroll_track();return;
  }
  const ActionInfo *choices=page==3?chat_actions:map_actions;
  const size_t count=page==3?sizeof(chat_actions)/sizeof(chat_actions[0]):
                             sizeof(map_actions)/sizeof(map_actions[0]);
  if(!right_compact)single_label(context_actions,page==3?"CHAT":"MAP",7,6,109,orange);
  int y=right_compact?4:32;
  for(size_t i=0;i<count;i++){
    const int id=choices[i].id;
    const bool selected=page==4&&
      ((id==A_MAP_HOME&&map_view==0)||(id==A_POIS&&map_view==1)||
       (id==A_TRACKS&&map_view==2)||(id==A_GNSS&&map_view==3)||
       (id==A_TEAM&&map_view==4)||(id==A_SHARE&&map_view==5));
    lv_obj_t *button=action_button(context_actions,
                                   right_compact?context_glyph(id):choices[i].label,
                                   right_compact?0:4,y,right_compact?32:111,38,
                                   selected?orange:panel2,on_action,(intptr_t)id);
    lv_obj_set_style_border_width(button,selected?0:1,0);
    // Match the left primary navigation palette: orange icons on dark panels,
    // dark glyph on orange for the selected action.
    lv_obj_set_style_border_color(button,orange,0);
    lv_obj_set_style_text_color(lv_obj_get_child(button,0),selected?bg:orange,0);
    y+=41;
  }
  lv_obj_update_layout(context_actions);
  update_context_scroll_track();
}
static void ble_select_now(void *v){meshble::connect((int)(intptr_t)v);draw();}
static void ble_select(lv_event_t *e){lv_async_call(ble_select_now,lv_event_get_user_data(e));}
static void ble_dialog_close(){
  if(ble_pin_keyboard)lv_keyboard_set_textarea(ble_pin_keyboard,nullptr);
  if(ble_dialog)lv_obj_del(ble_dialog);
  ble_dialog=nullptr;ble_pin=nullptr;ble_pin_keyboard=nullptr;ble_pin_notice=nullptr;
}
static void ble_dialog_close_later(void *){ble_dialog_close();}
static void ble_pin_action(lv_event_t *e){
  if((intptr_t)lv_event_get_user_data(e)==0){meshble::disconnect();lv_async_call(ble_dialog_close_later,nullptr);return;}
  if(!ble_pin)return;
  const char *p=lv_textarea_get_text(ble_pin);
  if(strlen(p)!=6){
    if(ble_pin_notice)lv_label_set_text(ble_pin_notice,"Enter all six digits before pairing.");
    return;
  }
  uint32_t code=0;
  for(int i=0;i<6;i++){if(p[i]<'0'||p[i]>'9')return;code=code*10+(uint32_t)(p[i]-'0');}
  if(meshble::submit_pin(code))lv_async_call(ble_dialog_close_later,nullptr);
  else if(ble_pin_notice)lv_label_set_text(ble_pin_notice,"PIN not submitted. Cancel and reconnect.");
}
static void ble_confirmation(lv_event_t *e){
  (void)meshble::confirm_number((intptr_t)lv_event_get_user_data(e)!=0);
  lv_async_call(ble_dialog_close_later,nullptr);
}
#define MCX_PIN_KEY (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | 1)
static const char *ble_pin_map[]={"1","2","3","\n","4","5","6","\n","7","8","9","\n",LV_SYMBOL_BACKSPACE,"0",LV_SYMBOL_OK,""};
static const lv_btnmatrix_ctrl_t ble_pin_ctrl[]={MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,
                                               MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY,MCX_PIN_KEY};
#undef MCX_PIN_KEY
static void ble_show_pairing(){
  const meshble::Snapshot s=meshble::snapshot();
  if(ble_dialog||(!s.pin_required&&!s.confirm_required))return;
  ble_dialog=panel_at(lv_layer_top(),0,0,480,320,bg,0);
  lv_obj_move_foreground(ble_dialog);
  action_button(ble_dialog,"CANCEL",366,4,106,34,panel2,ble_pin_action,0);
  single_label(ble_dialog,"BLUETOOTH PAIRING",10,8,260,orange);
  if(s.pin_required){
    ble_pin_notice=single_label(ble_dialog,"Enter your radio's six-digit Bluetooth PIN.",10,37,460,white);
    ble_pin=lv_textarea_create(ble_dialog);
    lv_obj_set_pos(ble_pin,10,62);lv_obj_set_size(ble_pin,278,42);
    lv_textarea_set_one_line(ble_pin,true);
    lv_obj_set_height(ble_pin,42);
    lv_textarea_set_accepted_chars(ble_pin,"0123456789");
    lv_textarea_set_max_length(ble_pin,6);
    lv_textarea_set_password_mode(ble_pin,true);
    lv_obj_add_state(ble_pin,LV_STATE_FOCUSED);
    action_button(ble_dialog,"PAIR",300,62,172,42,orange,ble_pin_action,1);
    ble_pin_keyboard=lv_keyboard_create(ble_dialog);
    // lv_keyboard defaults to BOTTOM_MID: positive y offsets otherwise clip it.
    lv_obj_set_align(ble_pin_keyboard,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(ble_pin_keyboard,4,112);lv_obj_set_size(ble_pin_keyboard,472,204);
    lv_keyboard_set_map(ble_pin_keyboard,LV_KEYBOARD_MODE_NUMBER,ble_pin_map,ble_pin_ctrl);
    lv_keyboard_set_mode(ble_pin_keyboard,LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ble_pin_keyboard,ble_pin);
    lv_obj_set_style_bg_color(ble_pin_keyboard,panel,0);
    lv_obj_set_style_text_font(ble_pin_keyboard,&lv_font_montserrat_20,LV_PART_ITEMS);
    lv_obj_set_style_pad_all(ble_pin_keyboard,3,0);
    lv_obj_set_style_pad_row(ble_pin_keyboard,4,0);
    lv_obj_set_style_pad_column(ble_pin_keyboard,4,0);
    lv_obj_add_event_cb(ble_pin_keyboard,ble_pin_action,LV_EVENT_READY,(void*)1);
    lv_obj_add_event_cb(ble_pin_keyboard,ble_pin_action,LV_EVENT_CANCEL,(void*)0);
  }else{
    char number[48];snprintf(number,sizeof(number),"Verify: %06lu",(unsigned long)s.compare_number);
    single_label(ble_dialog,number,10,67,430,white);
    action_button(ble_dialog,"MATCHES",10,142,208,50,orange,ble_confirmation,1);
    action_button(ble_dialog,"REJECT",227,142,215,50,panel2,ble_confirmation,0);
  }
}
// MeshCore workspace
#include "meshcrossway_meshcore_ui.h"

static void draw(){
  if(!body||!left_rail||!right_rail||!context_actions)return;
  const int lw=has_left_rail()?(left_compact?40:79):0;
  const int rw=has_context_rail()?(right_compact?40:125):0;
  const int cw=480-lw-rw;
  uptime_value=nullptr;heap_value=nullptr;network_value=nullptr;backup_value=nullptr;
  mcui::radio_status=mcui::home_name=mcui::home_status=nullptr;
  if(lw)lv_obj_clear_flag(left_rail,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(left_rail,LV_OBJ_FLAG_HIDDEN);
  if(rw)lv_obj_clear_flag(right_rail,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(right_rail,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(left_rail,0,34);lv_obj_set_size(left_rail,lw?lw:40,286);
  lv_obj_set_pos(body,lw,34);lv_obj_set_size(body,cw,286);
  lv_obj_set_pos(right_rail,480-rw,34);lv_obj_set_size(right_rail,rw?rw:40,286);
  // All rail height is now available for icons; no arrow-button row.
  // Reserve a dedicated 5px gutter at left of the contextual icons.
  lv_obj_set_pos(context_actions,5,0);
  lv_obj_set_size(context_actions,rw?rw-5:35,286);
  if(context_scroll_track)lv_obj_set_pos(context_scroll_track,1,9);
  for(int i=0;i<6;i++){
    if(!lw){lv_obj_add_flag(tabs[i],LV_OBJ_FLAG_HIDDEN);continue;}
    lv_obj_clear_flag(tabs[i],LV_OBJ_FLAG_HIDDEN);
    const int button_width=lw-6;
    lv_obj_set_pos(tabs[i],3,7+i*45);
    lv_obj_set_size(tabs[i],button_width,38);
    const bool active=i==page;
    const lv_color_t tint=active?bg:orange;
    lv_obj_set_style_bg_color(tabs[i],active?orange:panel2,0);
    lv_obj_t *glyph=lv_obj_get_child(tabs[i],0);
    lv_label_set_text(glyph,tab_glyphs[i]);
    lv_obj_set_style_text_color(glyph,tint,0);
    lv_obj_set_pos(glyph,0,left_compact?9:0);
    lv_obj_set_size(glyph,button_width,19);
    lv_obj_set_style_text_align(glyph,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *caption=lv_obj_get_child(tabs[i],1);
    lv_obj_set_pos(caption,0,21);
    lv_obj_set_size(caption,button_width,16);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(caption,active?bg:white,0);
    if(left_compact)lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(caption,LV_OBJ_FLAG_HIDDEN);
    if(tab_vectors[i]){
      lv_obj_set_pos(tab_vectors[i],(button_width-23)/2,left_compact?9:0);
      update_nav_vector(i,tint);
    }
  }
  lv_obj_clean(body);
  lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
  rebuild_context();
  const int w=cw;
  if(page==1||page==2||page==3){mcui::render();return;}
  if(page==0){
    show_center_heading("FIELD CONSOLE","MeshCore Companion / Meshtastic planned");
    const int col=(w-24)/2;
    const bool narrow=col<155;
    center_card(8,54,col,95,"MESHTASTIC",narrow?"BLE: OFF":"MX-M / BLE OFF",narrow?"No packets":"No live packets",green);
    lv_obj_t *mc_card=center_card(col+16,54,col,95,"MESHCORE",mcui::ready()?meshmc::client->name:"BLE: OFF",mcui::ready()?"MeshCore connected":"Connect in More / Radios",purple);
    mcui::home_name=lv_obj_get_child(mc_card,1);mcui::home_status=lv_obj_get_child(mc_card,2);
    if(mcui::ready())mcui::set_home_name(mcui::home_name,meshmc::client->name);
    center_card(8,157,col,95,"POSITION","GNSS: OFFLINE","No valid fix",cyan);
    center_card(col+16,157,col,95,"CROSSWAY","Bridge: OFF",narrow?"Demo only":"Demo data only",orange);
  } else if(page==4){
    static const char *modes[]={"MAP / OFFLINE","POINTS OF INTEREST","SAVED TRACKS",
                                "GNSS STATUS","TEAM POSITIONS","LIVE LOCATION SHARE"};
    show_center_heading(modes[map_view],"Navigation workspace / DEMO");
    if(map_view==0){
      map_preview(w);
      color_label(body,"Fix: NONE   Accuracy: --   Age: --",8,221,w-16,orange);
      color_label(body,"Sharing OFF  /  demo basemap only",8,249,w-16,muted);
    }else if(map_view==1){
      center_card(8,58,w-16,66,"POI / DEMO","Camp Alpha  [sample]","Not saved to SD",orange);
      center_card(8,131,w-16,66,"WAYPOINT / DEMO","Trail Junction  [sample]","No coordinates recorded",orange);
      color_label(body,"Real POIs require GNSS and SD storage.",8,229,w-16,muted);
    }else if(map_view==2){
      center_card(8,58,w-16,69,"TRACKS / DEMO","Ridge Loop  [sample]","No GPX file loaded",purple);
      center_card(8,134,w-16,69,"FOLDERS / DEMO","Expedition A  [sample]","No tracks stored",purple);
      color_label(body,"GPX import/export is not implemented.",8,232,w-16,muted);
    }else if(map_view==3){
      center_card(8,58,w-16,98,"GNSS / OFFLINE","No MAX-M10S connected","Fix: NONE  /  Sats: --",cyan);
      center_card(8,165,w-16,78,"SKY VIEW / OFF","No satellite observations",nullptr,cyan);
    }else if(map_view==4){
      center_card(8,58,w-16,78,"TEAM / DEMO","No nearby radios connected","All peer positions unavailable",green);
      color_label(body,"No live teammates or location data.",8,163,w-16,muted);
    }else{
      center_card(8,58,w-16,90,"LIVE SHARE / OFF","No session running","No position transmitted",green);
      center_card(8,156,w-16,86,"PRIVACY / REQUIRED","Destination + consent needed","Auto-stop required before share",orange);
    }
  } else {
    if(more_view==1){
      action_button(body,"SETTINGS",w-112,5,104,35,panel2,on_action,A_SETTINGS);
      const meshble::Snapshot s=meshble::snapshot();
      show_center_heading("BLE / RADIO PODS","Heltec V3 / service discovery");
      const char *phase=s.phase==meshble::OFF?"DISABLED":s.phase==meshble::STARTING?"STARTING":s.phase==meshble::READY?"SECURE":s.phase==meshble::SCANNING?"SCANNING":s.phase==meshble::CONNECTING?"CONNECTING":s.phase==meshble::VERIFYING?"VERIFYING":s.phase==meshble::PAIRING?"PAIRING":s.phase==meshble::FAILED?"ERROR":"IDLE";
      char detail[64];snprintf(detail,sizeof(detail),"%s / %s",phase,meshble::protocol_name(s.radio));
      single_label(body,detail,8,53,w-16,s.phase==meshble::READY?green:orange);
      single_label(body,s.phase==meshble::READY?(s.bonded?"BLE bond saved / PIN remembered":"Secure link / bond not supported"):s.status,8,76,w-16,muted);
      if(s.phase==meshble::OFF){
        single_label(body,"Safe boot: BLE is off until enabled.",8,106,w-16,green);
        action_button(body,"ENABLE BLE / TEST",8,151,w-16,49,orange,on_action,A_BLE_ENABLE);
        single_label(body,"Wi-Fi and touch should work before BLE.",8,219,w-16,muted);
      }else if(s.phase==meshble::STARTING){
        single_label(body,"BLE starting in dedicated worker...",8,113,w-16,white);
        single_label(body,"Watch Wi-Fi / touch; report boot log.",8,152,w-16,muted);
      }else if(s.phase==meshble::READY){
        lv_obj_t *mc_card=center_card(8,104,w-16,80,"CONNECTED",s.active_name,s.radio==meshble::MESHCORE&&meshmc::client?meshmc::client->status:"Meshtastic transport only",green);
        mcxemoji::view::set_message_text(lv_obj_get_child(mc_card,1),s.active_name);
        if(s.radio==meshble::MESHCORE)mcui::radio_status=lv_obj_get_child(mc_card,2);
        action_button(body,"MESHCORE TOOLS",8,190,w-16,40,orange,mcui::on_tools,0);
        action_button(body,"DISCONNECT",8,236,w-16,40,panel2,on_action,A_BLE_DISCONNECT);
      }else if(s.phase==meshble::CONNECTING||s.phase==meshble::VERIFYING||s.phase==meshble::PAIRING){
        single_label(body,"BLE handshake / PIN pairing",8,116,w-16,white);
        action_button(body,"DISCONNECT",8,179,w-16,43,panel2,on_action,A_BLE_DISCONNECT);
      }else{
        action_button(body,"SCAN NEARBY BLE",8,105,w-16,40,orange,on_action,A_BLE_SCAN);
        int y=155;
        for(int k=0;k<s.count;k++){
          char name[60];snprintf(name,sizeof(name),"%s / %d dBm",s.peers[k].name,s.peers[k].rssi);
          lv_obj_t *b=action_button(body,"",8,y,w-16,52,panel,ble_select,k);
          lv_obj_t *peer_name=single_label(b,name,7,6,w-28,white);
          mcxemoji::view::set_message_text(peer_name,name);
          single_label(b,meshble::protocol_name(s.peers[k].advertised),7,30,w-28,muted);
          y+=56;
        }
      }
      return;
    }
    show_center_heading("SETTINGS / DEVICE","Core configuration and system status");
    char uptime[42],memory[42],ip[16],wifi[30],status[55];
    const uint64_t seconds=(uint64_t)(esp_timer_get_time()/1000000LL);
    snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
             (unsigned long long)(seconds/3600),(unsigned long long)((seconds/60)%60));
    snprintf(memory,sizeof(memory),"Heap: %lu KB",(unsigned long)(esp_get_free_heap_size()/1024));
    meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
    char backup_detail[48];meshbackup::describe(backup_detail,sizeof(backup_detail));
    snprintf(status,sizeof(status),"SD: %.42s",backup_detail);
    const int col=(w-26)/2;
    lv_obj_t *device_card=panel_at(body,8,53,col,92,panel);
    single_label(device_card,"DEVICE",10,7,col-20,orange);
    uptime_value=single_label(device_card,uptime,10,34,col-20,white);
    heap_value=single_label(device_card,memory,10,60,col-20,muted);
    lv_obj_t *wifi_card=panel_at(body,col+18,53,col,92,panel);
    single_label(wifi_card,"WI-FI / STORAGE",10,7,col-20,orange);
    network_value=single_label(wifi_card,ip[0]?ip:wifi,10,34,col-20,white);
    backup_value=single_label(wifi_card,status,10,60,col-20,muted);
    action_button(body,"WI-FI NETWORKS",8,153,col,39,orange,on_action,A_WIFI);
    action_button(body,"BLUETOOTH / RADIOS",col+18,153,col,39,panel2,on_action,A_RADIOS);
    action_button(body,"REMOTE VIEW",8,198,col,39,panel2,on_action,A_VIEW);
    action_button(body,"DEVICE DIAGNOSTICS",col+18,198,col,39,panel2,on_action,A_DIAGNOSTIC);
    action_button(body,"PRIVACY",8,243,col,35,panel2,on_action,A_PRIVACY);
    action_button(body,"ABOUT",col+18,243,col,35,panel2,on_action,A_ABOUT);
  }
}
static void refresh(lv_timer_t *){
  update_wifi_scan();
  const meshble::Snapshot bs=meshble::snapshot();
  if(bs.generation!=ble_generation){
    ble_generation=bs.generation;
    if(ble_dialog&&!bs.pin_required&&!bs.confirm_required)ble_dialog_close();
    if(page==5&&more_view==1&&!wifi_dialog&&!info_dialog&&!ble_dialog)draw();
    if(bs.pin_required||bs.confirm_required)ble_show_pairing();
  }
  if(wifi_sd_status){
    char detail[64],backup[88];
    meshbackup::describe(detail,sizeof(detail));
    snprintf(backup,sizeof(backup),"SD backup: %s",detail);
    if(strcmp(lv_label_get_text(wifi_sd_status),backup))lv_label_set_text(wifi_sd_status,backup);
  }
  // Compact status bar: real Wi-Fi STA IP state and actual current BLE
  // GATT identity. The current radio adapter has ONE active BLE session.
  if(top_wifi&&top_mt&&top_mc&&top_mt_name&&top_mc_name){
    char ip[16]={},state[30]={};
    meshnetwork::get_status(ip,sizeof(ip),state,sizeof(state));
    const bool wifi_online=ip[0]!=0;
    const bool mt_online=bs.phase==meshble::READY &&
                         bs.radio==meshble::MESHTASTIC;
    const bool mc_online=bs.phase==meshble::READY &&
                         bs.radio==meshble::MESHCORE && mcui::ready();
    lv_obj_set_style_text_color(top_wifi,wifi_online?green:muted,0);
    lv_obj_set_style_text_color(top_mt,mt_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mc,mc_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mt_name,mt_online?orange:muted,0);
    lv_obj_set_style_text_color(top_mc_name,mc_online?orange:muted,0);
  }
  // PMU readout never fabricates a level for an absent battery.
  if(top_battery&&top_battery_fill&&top_battery_percent&&top_charge){
    const mcxbattery::Reading b=meshpower::snapshot();
    const bool present=b.valid&&b.battery_present&&b.percent<=100;
    char number[8];
    if(present)snprintf(number,sizeof(number),"%u%%",(unsigned)b.percent);
    else snprintf(number,sizeof(number),"--");
    if(strcmp(lv_label_get_text(top_battery_percent),number))
      lv_label_set_text(top_battery_percent,number);
    lv_obj_set_style_border_color(top_battery,present?
      (b.percent<=15?red:green):muted,0);
    lv_obj_set_width(top_battery_fill,present?int(b.percent)*34/100:0);
    lv_obj_set_style_bg_color(top_battery_fill,b.percent<=15?red:green,0);
    const char *charging=b.charging?LV_SYMBOL_CHARGE:
                         (b.valid&&b.usb_present?LV_SYMBOL_USB:"");
    if(strcmp(lv_label_get_text(top_charge),charging))
      lv_label_set_text(top_charge,charging);
    lv_obj_set_style_text_color(top_charge,b.charging?orange:muted,0);
  }
  printf("[MCX-DIAG v0.7.8] uptime_ms=%llu heap=%lu page=%d capture=%lu keys=%lu\n",
         (unsigned long long)(esp_timer_get_time()/1000),
         (unsigned long)esp_get_free_heap_size(),page,
         (unsigned long)meshcapture::captures.load(),(unsigned long)editor_key_count);
  // Never destroy and rebuild LVGL settings cards in the periodic timer.
  if(page!=5||more_view!=0||wifi_dialog||!uptime_value||!heap_value||!network_value||!backup_value)return;
  char uptime[42],memory[42],ip[16],wifi[30],sd[55],backup_detail[48];
  const uint64_t seconds=(uint64_t)(esp_timer_get_time()/1000000LL);
  snprintf(uptime,sizeof(uptime),"Uptime: %lluh %02llum",
           (unsigned long long)(seconds/3600),(unsigned long long)((seconds/60)%60));
  snprintf(memory,sizeof(memory),"Heap: %lu KB",(unsigned long)(esp_get_free_heap_size()/1024));
  meshnetwork::get_status(ip,sizeof(ip),wifi,sizeof(wifi));
  meshbackup::describe(backup_detail,sizeof(backup_detail));
  snprintf(sd,sizeof(sd),"SD: %.42s",backup_detail);
  lv_obj_t *objects[]={uptime_value,heap_value,network_value,backup_value};
  const char *values[]={uptime,memory,ip[0]?ip:wifi,sd};
  for(int i=0;i<4;i++)
    if(strcmp(lv_label_get_text(objects[i]),values[i]))lv_label_set_text(objects[i],values[i]);
}
static void init(){
  printf("[MCX-DIAG v0.7.8] boot reset_reason=%d heap=%lu http=MANUAL_CAPTURE touch=OFF wifi=ON\n",
         (int)esp_reset_reason(),(unsigned long)esp_get_free_heap_size());
  lv_obj_t *screen=lv_scr_act();fill(screen,bg);
  lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *header=panel_at(screen,0,0,480,34,panel,0);
  lv_obj_t *top=lv_btn_create(header);fill(top,panel,0);
  lv_obj_set_pos(top,0,0);lv_obj_set_size(top,480,34);
  lv_obj_add_event_cb(top,open_shade,LV_EVENT_CLICKED,nullptr);
  // Use the official mountain mark and traced MeshCrossway lettering.
  lv_obj_t *brand=lv_img_create(top);
  lv_img_set_src(brand,&mcxbrand::official_lockup);
  lv_obj_set_pos(brand,6,5);
  // No DEV, IP, overlapping MT/MC symbols or external battery percentage.
  lv_obj_t *version=color_label(top,"0.7.8",187,11,43,muted);
  lv_obj_set_style_text_font(version,&lv_font_montserrat_10,0);
  top_wifi=color_label(top,LV_SYMBOL_WIFI,242,7,22,muted);
  top_mt=color_label(top,LV_SYMBOL_BLUETOOTH,273,7,20,muted);
  top_mt_name=color_label(top,"MT",293,10,27,muted);
  top_mc=color_label(top,LV_SYMBOL_BLUETOOTH,323,7,20,muted);
  top_mc_name=color_label(top,"MC",343,10,27,muted);
  top_charge=color_label(top,"",403,8,23,muted);
  top_battery=panel_at(top,430,7,44,20,panel,2);
  lv_obj_set_style_border_width(top_battery,1,0);
  lv_obj_set_style_border_color(top_battery,muted,0);
  lv_obj_set_style_radius(top_battery,3,0);
  panel_at(top,475,13,4,8,muted,1);
  top_battery_fill=panel_at(top_battery,3,3,0,14,green,1);
  lv_obj_set_style_bg_opa(top_battery_fill,LV_OPA_40,0);
  top_battery_percent=color_label(top_battery,"--",2,1,40,white);
  lv_obj_set_style_text_align(top_battery_percent,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_text_font(top_battery_percent,&lv_font_montserrat_10,0);
  lv_label_set_long_mode(top_battery_percent,LV_LABEL_LONG_CLIP);
  left_rail=panel_at(screen,0,34,79,286,panel,0);
  right_rail=panel_at(screen,355,34,125,286,panel,0);
  for(int i=0;i<6;i++){
    tabs[i]=action_button(left_rail,"",3,7+i*45,73,38,panel2,on_nav,i);
    // Keep tracking this finger after it leaves the icon bounds during drag.
    lv_obj_add_flag(tabs[i],LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_PRESSED,nullptr);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_PRESSING,nullptr);
    lv_obj_add_event_cb(tabs[i],left_tab_touch,LV_EVENT_RELEASED,nullptr);
    single_label(tabs[i],tab_names[i],0,19,73,white);
    make_nav_vector(i);
  }
  context_actions=panel_at(right_rail,5,0,120,286,panel,0);
  context_scroll_track=panel_at(right_rail,1,9,3,266,muted,0);
  lv_obj_set_style_bg_opa(context_scroll_track,LV_OPA_30,0);
  context_scroll_thumb=panel_at(context_scroll_track,0,0,3,200,orange,1);
  lv_obj_add_flag(context_scroll_track,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(context_actions,context_scrolled,LV_EVENT_SCROLL,nullptr);
  // The Map context has more actions than fit vertically; allow finger scrolling.
  lv_obj_add_flag(context_actions,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(context_actions,LV_DIR_VER);
  // Keep touch scrolling, but never draw an overlay over the action icons.
  lv_obj_set_scrollbar_mode(context_actions,LV_SCROLLBAR_MODE_OFF);
  body=panel_at(screen,79,34,276,286,bg,0);
  lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
  draw();
  meshpower::start(); // Read-only 15s AXP2101 samples on a background task.
  lv_timer_create(edge_swipe_poll,20,nullptr);
  lv_timer_create(refresh,3000,nullptr);
  lv_timer_create(mcui::poll,50,nullptr);
}
} // namespace meshcrossway
