#!/usr/bin/env python3
"""Run the actual pairing dialog on host LVGL 8.4, with a simulated pointer.

Uses the LVGL copy in the pinned Waveshare BSP; BLE is mocked, UI code is extracted
unchanged from firmware. Tests layout, real key taps, submission and cancellation.
"""
import argparse
import concurrent.futures
import hashlib
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def run(lvgl, render):
    assert '#define LVGL_VERSION_MINOR 4' in (lvgl / 'lvgl.h').read_text()
    ui = (ROOT / 'firmware/display-prototype/meshcrossway_ui.h').read_text()
    def segment(start, end):
        begin = ui.index(start)
        return ui[begin:ui.index(end, begin)]
    pieces = [segment('static const lv_color_t bg=', 'static lv_obj_t *tile('),
              segment('static lv_obj_t *single_label(', '// Lightweight LVGL shapes'),
              segment('static void ble_dialog_close()', '// MeshCore workspace')]
    test = r'''
#include "lvgl.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
namespace meshble {
struct Snapshot { bool pin_required; bool confirm_required; uint32_t compare_number; };
static Snapshot state={true,false,0};
static int submissions=0, disconnects=0;
static uint32_t last_code=0;
static bool fail_submission=false;
static Snapshot snapshot(){return state;}
static bool submit_pin(uint32_t code){
  if(fail_submission)return false;
  ++submissions;last_code=code;state.pin_required=false;return true;
}
static void disconnect(){++disconnects;state.pin_required=false;state.confirm_required=false;}
static bool confirm_number(bool){state.confirm_required=false;return true;}
}
static lv_obj_t *ble_dialog=nullptr,*ble_pin=nullptr,*ble_pin_keyboard=nullptr,*ble_pin_notice=nullptr;
''' + '\n'.join(pieces) + r'''
static lv_color_t framebuffer[480*320],draw_buffer[480*32];
static int pointer_x=0,pointer_y=0;
static bool pressed=false;
static void flush(lv_disp_drv_t *driver,const lv_area_t *a,lv_color_t *pixels){
  for(int y=a->y1;y<=a->y2;++y)
    for(int x=a->x1;x<=a->x2;++x)framebuffer[y*480+x]=*pixels++;
  lv_disp_flush_ready(driver);
}
static void read_pointer(lv_indev_drv_t *,lv_indev_data_t *data){
  data->point.x=pointer_x;data->point.y=pointer_y;
  data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void advance(int ms=80){for(int i=0;i<ms;i+=5){lv_tick_inc(5);lv_timer_handler();}}
static void tap(int x,int y,int hold=80){
  pointer_x=x;pointer_y=y;pressed=true;advance(hold);pressed=false;advance();
}
static void tap_object(lv_obj_t *obj){
  lv_area_t a;lv_obj_get_coords(obj,&a);tap((a.x1+a.x2)/2,(a.y1+a.y2)/2);
}
static void key(const char *text,int hold=80){
  auto kb=(lv_btnmatrix_t *)ble_pin_keyboard;
  for(uint16_t i=0;i<12;++i){
    if(strcmp(lv_btnmatrix_get_btn_text(ble_pin_keyboard,i),text))continue;
    lv_area_t outer;lv_obj_get_coords(ble_pin_keyboard,&outer);
    auto a=kb->button_areas[i];
    assert(a.x2-a.x1>=100 && a.y2-a.y1>=40);
    tap(outer.x1+(a.x1+a.x2)/2,outer.y1+(a.y1+a.y2)/2,hold);return;
  }
  assert(false && "Numeric key missing");
}
static lv_obj_t *button(const char *text){
  for(uint32_t i=0;i<lv_obj_get_child_cnt(ble_dialog);++i){
    lv_obj_t *child=lv_obj_get_child(ble_dialog,i);
    if(!lv_obj_check_type(child,&lv_btn_class))continue;
    if(!strcmp(lv_label_get_text(lv_obj_get_child(child,0)),text))return child;
  }
  assert(false && "Dialog button missing");return nullptr;
}
static void open_pin(){meshble::state={true,false,0};ble_show_pairing();advance();}
int main(int argc,char **argv){
  lv_init();
  lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,draw_buffer,nullptr,480*32);
  lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=480;display.ver_res=320;
  display.flush_cb=flush;display.draw_buf=&buffer;lv_disp_drv_register(&display);
  lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;
  input.read_cb=read_pointer;lv_indev_drv_register(&input);

  // Reproduce the v0.6.6 bug with LVGL's real default BOTTOM_MID alignment.
  lv_obj_t *old=panel_at(lv_scr_act(),12,42,456,236,panel,9);
  lv_obj_t *old_kb=lv_keyboard_create(old);
  lv_obj_set_pos(old_kb,10,112);lv_obj_set_size(old_kb,432,112);
  lv_obj_update_layout(old);
  lv_area_t outer,inner;lv_obj_get_coords(old,&outer);lv_obj_get_coords(old_kb,&inner);
  assert(inner.y1>outer.y2 && "Original keyboard must reproduce the clipping defect");
  lv_obj_del(old);

  open_pin();
  lv_obj_get_coords(ble_dialog,&outer);lv_obj_get_coords(ble_pin_keyboard,&inner);
  assert(outer.x1==0 && outer.y1==0 && outer.x2==479 && outer.y2==319);
  assert(inner.x1>=outer.x1 && inner.y1>=outer.y1 && inner.x2<=outer.x2 && inner.y2<=outer.y2);
  assert(lv_obj_get_parent(ble_dialog)==lv_layer_top());
  assert(lv_keyboard_get_textarea(ble_pin_keyboard)==ble_pin);
  assert(lv_textarea_get_password_mode(ble_pin));

  if(argc>1){
    lv_refr_now(nullptr);
    FILE *f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\n480 320\n255\n");
    for(auto color:framebuffer){lv_color32_t c;c.full=lv_color_to32(color);
      unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,f);}
    fclose(f);
  }
  tap_object(button("PAIR"));assert(meshble::submissions==0 && ble_dialog);
  assert(strstr(lv_label_get_text(ble_pin_notice),"six digits"));
  key("0",650);assert(!strcmp(lv_textarea_get_text(ble_pin),"0")); // No repeat on hold.
  key("0");key("1");key("2");key("3");key("9");
  key("8");assert(!strcmp(lv_textarea_get_text(ble_pin),"001239")); // Six-digit limit.
  key(LV_SYMBOL_BACKSPACE);key("4");assert(!strcmp(lv_textarea_get_text(ble_pin),"001234"));
  key(LV_SYMBOL_OK);advance();
  assert(meshble::submissions==1 && meshble::last_code==1234 && !ble_dialog);
  assert(!ble_pin && !ble_pin_keyboard && !ble_pin_notice);

  open_pin();for(const char *d:{"1","2","3","4","5","6"})key(d);
  tap_object(button("PAIR"));advance();assert(meshble::submissions==2 && !ble_dialog);
  open_pin();key("1");tap_object(button("CANCEL"));advance();
  assert(!ble_dialog && meshble::disconnects==1 && meshble::submissions==2);

  open_pin();for(const char *d:{"1","2","3","4","5","6"})key(d);
  meshble::fail_submission=true;tap_object(button("PAIR"));
  assert(ble_dialog && strstr(lv_label_get_text(ble_pin_notice),"not submitted"));
  tap_object(button("CANCEL"));advance();meshble::fail_submission=false;
  for(int i=0;i<20;++i){open_pin();ble_dialog_close();assert(!ble_dialog);}
  meshble::state={false,true,123456};ble_show_pairing();advance();
  assert(button("MATCHES") && button("REJECT"));
  tap_object(button("CANCEL"));advance();assert(!ble_dialog);
  puts("PASS: real LVGL reproduces old clipping; keypad visible, large touch keys, leading zeros, length, backspace, OK/PAIR, cancel, reopen and failure feedback");
}
'''
    # Compile and exercise the actual MeshCore workspace with real LVGL too.
    companion_setup = r'''
#include "meshcrossway_conversations.h"
#include <atomic>
static int64_t esp_timer_get_time(){return 6000000;}
namespace mcxhistory {
static mcxchat::Workspace memory;
static bool init_workspace(){return true;}
static mcxchat::Workspace *workspace(){return &memory;}
static mcxchat::Local &preferences(){return memory.local;}
static bool restored=true;
static const char *status(){return "SD: test fixture";}
static void poll(meshmc::Client &,uint32_t,uint32_t){}
}
namespace meshmc {
static std::atomic<uint32_t> connection_epoch{1};
static Client *client=nullptr;
static void poll(){}
static bool channel_secret(const char *,uint8_t *p){memset(p,0xA5,16);return true;}
}
static lv_obj_t *body=nullptr,*wifi_dialog=nullptr;
static int page=1,nodes_filter=0,groups_filter=0,chats_filter=0,redraws=0;
static const lv_color_t green=lv_color_hex(0x72D8A5);
static int content_width(){return 400;}
static void draw(){++redraws;}
static void notice(const char *,const char *){}
static void filter_tabs(int,int){}
static void show_center_heading(const char *,const char *){}
''' + segment('// Static maps:', '#undef MCX_KEY_FLAGS') + '\n#undef MCX_KEY_FLAGS\n' + r'''
#include "meshcrossway_meshcore_ui.h"
static lv_obj_t *mc_button(lv_obj_t *parent,const char *text){
  for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);i++){
    lv_obj_t *b=lv_obj_get_child(parent,i);
    if(lv_obj_check_type(b,&lv_btn_class)&&lv_obj_get_child_cnt(b)&&!strcmp(lv_label_get_text(lv_obj_get_child(b,0)),text))return b;
  }fprintf(stderr,"Missing MeshCore button: %s / children %u\n",text,(unsigned)lv_obj_get_child_cnt(parent));for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i){auto *b=lv_obj_get_child(parent,i);if(lv_obj_get_child_cnt(b))fprintf(stderr,"child %u: %s\n",i,lv_label_get_text(lv_obj_get_child(b,0)));}assert(false);return nullptr;
}
static void test_companion_ui(const char *preview){
  meshmc::client=new meshmc::Client();auto &c=*meshmc::client;c.attach(173);c.initialized=true;c.protocol=14;c.channel_count=2;c.max_power=22;
  c.clock=1800000000;c.contacts[0].key[0]=0xab;c.contact_count=1;strcpy(c.contacts[0].name,"Heltec");
  c.repeat_enabled=true;c.manual_contacts=1;c.telemetry_policy=21;c.location_policy=2;c.multi_acks=3;
  mcui::form(11);assert(mcui::field_count==5&&!strcmp(lv_textarea_get_text(mcui::fields[4]),"1"));mcui::close();
  mcui::form(38);assert(!strcmp(lv_textarea_get_text(mcui::fields[1]),"21")&&!strcmp(lv_textarea_get_text(mcui::fields[3]),"3"));mcui::close();
  mcui::form(32);advance();assert(mcui::field_count==3);
  lv_textarea_set_text(mcui::fields[0],"1");lv_textarea_set_text(mcui::fields[1],"#test");lv_textarea_set_text(mcui::fields[2],"");
  lv_event_send(mcui::fields[1],LV_EVENT_CLICKED,nullptr);advance();
  lv_area_t kb;lv_obj_get_coords(mcui::keyboard,&kb);assert(kb.x1>=0&&kb.y1>=0&&kb.x2<480&&kb.y2<320);
  assert(lv_keyboard_get_textarea(mcui::keyboard)==mcui::edit_value);
  if(preview){lv_refr_now(nullptr);FILE *f=fopen(preview,"wb");assert(f);fprintf(f,"P6\n480 320\n255\n");for(auto color:framebuffer){lv_color32_t c;c.full=lv_color_to32(color);unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,f);}fclose(f);}
  tap_object(mc_button(mcui::editor,"DONE"));advance();assert(!mcui::editor);
  tap_object(mc_button(mcui::dialog,"APPLY"));advance();assert(c.qcount==0&&mcui::confirm_pending);
  tap_object(mc_button(mcui::dialog,"CONFIRM"));advance();assert(c.qcount==1&&c.queue[0].frame.data[0]==32&&c.queue[0].frame.len==50);
  assert(c.queue[0].frame.data[34]==0xa5);mcui::close();c.qcount=0;
  mcui::select(mcxchat::key_for(c,false,0));mcui::compose();advance();
  lv_textarea_set_text(mcui::fields[0],"hello from display");tap_object(mc_button(mcui::dialog,"SEND"));advance();
  assert(!mcui::dialog&&c.qcount==1&&c.queue[0].frame.data[0]==2&&c.message_count==1);
  body=panel_at(lv_scr_act(),0,34,400,286,bg);page=1;mcui::render();advance();
  page=3;lv_obj_clean(body);mcui::render();advance();
  // Exercise phase-1 controls against the real LVGL workspace and production model.
  c.qcount=0;c.messages[0].delivery=meshmc::UNCERTAIN;
  mcui::compose();lv_textarea_set_text(mcui::fields[0],"draft kept offline");mcui::close();
  assert(!strcmp(mcxhistory::preferences().get(mcui::selected)->draft,"draft kept offline"));
  assert(mcui::conversation);mcui::chat_tools();advance();tap_object(mc_button(mcui::scroller,"FAVORITE"));advance();
  assert(mcxhistory::preferences().get(mcui::selected)->favorite);mcui::close();
  mcui::alias_event(nullptr);advance();lv_textarea_set_text(mcui::fields[0],"Local Heltec");advance();
  tap_object(mc_button(mcui::dialog,"SAVE"));advance();assert(!strcmp(mcui::thread_name(mcui::selected),"Local Heltec"));
  mcui::message_details_now((void *)(intptr_t)1);advance();tap_object(mc_button(mcui::dialog,"RESEND"));advance();
  assert(c.qcount==0&&mcui::confirm_pending);tap_object(mc_button(mcui::dialog,"RESEND"));advance();
  assert(c.qcount==1&&c.message_count==2&&!mcui::dialog);
  mcui::show_list();assert(!mcui::conversation);lv_obj_clean(body);mcui::render();advance();assert(mcui::thread_count==1);
  c.qcount=0;mcui::select(mcxchat::key_for(c,false,0));
  for(int i=0;i<20;i++){mcui::form(26);assert(lv_textarea_get_password_mode(mcui::fields[1]));mcui::close();}
  mcui::tools();advance();assert(lv_obj_get_child_cnt(mcui::scroller)==meshmc::COMMAND_COUNT);mcui::close();
  c.protocol=7;mcui::tools();lv_obj_scroll_to_y(mcui::scroller,4000,LV_ANIM_OFF);advance();assert(!strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(mcui::scroller,meshmc::COMMAND_COUNT-1),0)),"Local radio console"));assert(lv_obj_has_state(lv_obj_get_child(mcui::scroller,meshmc::COMMAND_COUNT-1),LV_STATE_DISABLED));mcui::close();
  mcui::radio_status=txt(body,"stale sync",8,8,350,white);advance();c.busy=false;c.fault=false;c.initialized=true;mcui::poll(nullptr);advance();assert(!strcmp(lv_label_get_text(mcui::radio_status),"MeshCore ready"));mcui::radio_status=nullptr;
  lv_obj_del(body);body=nullptr;delete meshmc::client;meshmc::client=nullptr;
  puts("PASS: MeshCore real-LVGL forms, keyboard geometry, confirmation, channel command, compose/send, lists, history and firmware gating");
}
'''
    test = test.replace('int main(int argc,char **argv){', companion_setup + '\nint main(int argc,char **argv){')
    test = test.replace('  puts("PASS: real LVGL', '  test_companion_ui(argc>2?argv[2]:nullptr);\n  puts("PASS: real LVGL')

    with tempfile.TemporaryDirectory() as temporary:
        temp = Path(temporary)
        flags = ['-O1', '-DLV_CONF_SKIP', '-DLV_MEM_CUSTOM=1', '-DLV_FONT_MONTSERRAT_20=1', '-I'+str(lvgl), '-I'+str(ROOT/'firmware/display-prototype')]
        def compile_c(source):
            obj = temp / (hashlib.sha256(str(source).encode()).hexdigest()[:16]+'.o')
            subprocess.run(['cc', '-std=c99', *flags, '-c', str(source), '-o', str(obj)],check=True,capture_output=True)
            return str(obj)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            objects = list(pool.map(compile_c, sorted((lvgl/'src').rglob('*.c'))))
        source = temp / 'pairing.cpp';source.write_text(test)
        binary = temp / 'pairing'
        subprocess.run(['c++', '-std=c++17', *flags, str(source), *objects, '-lm', '-o', str(binary)],check=True)
        args = [str(binary)]
        if render:
            render.parent.mkdir(parents=True, exist_ok=True)
            args.extend([str(render),str(render.with_name(render.stem+'-meshcore.ppm'))])
        subprocess.run(args,check=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', type=Path, required=True)
    parser.add_argument('--render', type=Path, help='Optional PPM screenshot path')
    args = parser.parse_args()
    run(args.lvgl.resolve(), args.render)
