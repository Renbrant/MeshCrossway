#include "lvgl.h"
#include "meshcrossway_map_tiles.h"
#include "meshcrossway_gnss_parser.h"
#include "meshcrossway_gnss_settings.h"
#include "meshcrossway_navigation_store.h"
#include "meshcrossway_clock_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
namespace meshnetwork { bool connected=false; }
namespace mcxgnss {
struct Diagnostics {unsigned baud=9600;int error=0;uint32_t bytes=100;bool saved=true;};
static Settings cfg;static Fix fix;
static Settings configuration(){return cfg;}
static void configure(Settings c){assert(valid_settings(c));cfg=c;}
static Diagnostics status(){return {};}
static Fix snapshot(){return fix;}
}
namespace mcxnav {
static Snapshot mock_state;static Action last_action=NONE;static uint32_t last_id=0;static char last_name[40]={};
static bool start(){return true;}
static bool snapshot(Snapshot &s){s=mock_state;return true;}
static bool submit(Action a,uint32_t id=0,const char *name=nullptr){last_action=a;last_id=id;if(name)snprintf(last_name,sizeof(last_name),"%s",name);return a!=SAVE_POI||mcxgnss::fix.valid;}
}
namespace meshcrossway {
static int page=4,map_view=0;static void draw();
static lv_obj_t *body=nullptr,*info_dialog=nullptr;
static bool navigation_panel=false;
static const lv_color_t purple=lv_color_hex(0x9C88FF);
static const lv_color_t green=lv_color_hex(0x72D8A5);
static const lv_color_t bg=lv_color_hex(0x090C10),panel2=lv_color_hex(0x242B34),orange=lv_color_hex(0xFF8A24),white=lv_color_hex(0xF2F4F7),muted=lv_color_hex(0x9AA6B4),cyan=lv_color_hex(0x6CBDE6),red=lv_color_hex(0xFF7777);
static void fill(lv_obj_t *o,lv_color_t c,int radius=0){lv_obj_set_style_bg_color(o,c,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,radius,0);}
static lv_obj_t *panel_at(lv_obj_t *p,int x,int y,int w,int h,lv_color_t c,int r=0){auto *o=lv_obj_create(p);fill(o,c,r);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
static lv_obj_t *single_label(lv_obj_t *p,const char *s,int x,int y,int w,lv_color_t c){auto *o=lv_label_create(p);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_set_style_text_color(o,c,0);return o;}
static lv_obj_t *action_button(lv_obj_t *p,const char *s,int x,int y,int w,int h,lv_color_t c,lv_event_cb_t cb,intptr_t value){auto *o=lv_btn_create(p);fill(o,c);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);auto *l=single_label(o,s,0,5,w,white);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void *)value);return o;}
static void info_close_now(void *){if(info_dialog){lv_obj_del(info_dialog);info_dialog=nullptr;}}
static void info_close(lv_event_t *){info_close_now(nullptr);}
namespace chatview {static void label(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}}
// PRODUCTION_ACTIONS
static lv_obj_t *context_actions=nullptr;static bool right_compact=false;static int clicked=-1;
static bool has_context_rail(){return page==4;}
static void update_context_scroll_track(){}
static void on_action(lv_event_t *e){clicked=int(intptr_t(lv_event_get_user_data(e)));}
// PRODUCTION_CONTEXT
#include "meshcrossway_navigation_ui.h"
static void draw(){navui::invalidate();lv_obj_clean(body);navui::after_clean();navui::create(lv_obj_get_width(body));}
static lv_obj_t *gps_position=nullptr,*gps_status=nullptr,*gps_extra=nullptr,*gps_icon=nullptr,*gps_satellites=nullptr,*top_charge=nullptr,*top_clock=nullptr;
static lv_obj_t *color_label(lv_obj_t *p,const char *s,int x,int y,int w,lv_color_t c){return single_label(p,s,x,y,w,c);}
// PRODUCTION_GPS_REFRESH
}
static lv_color_t drawbuf[480*20];static bool down=false;static int pointer_x=0,pointer_y=0;
static void flush(lv_disp_drv_t *d,const lv_area_t *,lv_color_t *){lv_disp_flush_ready(d);}
static void read_pointer(lv_indev_drv_t *,lv_indev_data_t *d){d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;d->point.x=pointer_x;d->point.y=pointer_y;}
static void advance(unsigned ms=80){for(unsigned n=0;n<ms;n+=5){lv_tick_inc(5);lv_timer_handler();}}
static void tap(lv_obj_t *obj){lv_area_t a;lv_obj_update_layout(obj);lv_obj_get_coords(obj,&a);pointer_x=(a.x1+a.x2)/2;pointer_y=(a.y1+a.y2)/2;down=true;advance();down=false;advance();}
static void make_tiles(const mcxmap::View &v){
 const auto center=mcxmap::project(v.latitude,v.longitude,v.zoom);const int left=int(floor(center.x-v.width/2.0)),top=int(floor(center.y-v.height/2.0));
 for(int y=int(floor(top/256.0));y<=int(floor((top+int(v.height)-1)/256.0));++y)for(int x=int(floor(left/256.0));x<=int(floor((left+int(v.width)-1)/256.0));++x){
  auto path=std::filesystem::path(mcxmap::SD_ROOT)/"tiles"/std::to_string(v.zoom)/std::to_string(x);std::filesystem::create_directories(path);
  std::vector<unsigned char> rgba(256*256*4);for(size_t i=0;i<rgba.size();i+=4){rgba[i]=25;rgba[i+1]=150;rgba[i+2]=75;rgba[i+3]=255;}
  unsigned char *png=nullptr;size_t size=0;assert(lodepng_encode32(&png,&size,rgba.data(),256,256)==0);
  fake_http_body.assign(png,png+size);
  auto file=path/(std::to_string(y)+".png");FILE *f=fopen(file.c_str(),"wb");assert(f);assert(fwrite(png,1,size,f)==size);assert(!fclose(f));lv_mem_free(png);
 }
}
int main(int argc,char **argv){
 assert(argc==2);mcxmap::SD_ROOT=argv[1];
 lv_init();lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,drawbuf,nullptr,480*20);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=480;d.ver_res=320;d.draw_buf=&buf;d.flush_cb=flush;lv_disp_drv_register(&d);
 lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=read_pointer;lv_indev_drv_register(&input);
 mcxmap::View v;v.latitude=45.123456;v.longitude=-110.654321;v.width=400;v.height=155;v.generation=1;mcxmap::wanted=1;mcxmap::Config c;c.online=0;make_tiles(v);
 auto *frame=mcxmap::render(v,c);assert(frame&&frame->loaded>=2&&!frame->missing);const auto *pixels=(const lv_color_t *)frame->image.data;assert(pixels[100].full==lv_color_make(25,150,75).full);mcxmap::release(frame);
 const auto decoded=mcxmap::decode_count.load();
 const auto hits=mcxmap::cache_hits.load();
 frame=mcxmap::render(v,c);assert(frame&&!frame->missing&&mcxmap::decode_count.load()==decoded&&mcxmap::cache_hits.load()>hits);mcxmap::release(frame);
 mcxmap::wanted=2;assert(!mcxmap::render(v,c));
 assert(fake_http_calls==0);mcxmap::wifi_ready=[](){return true;};fake_http_enabled=true;c.online=1;
 auto online_view=v;online_view.latitude+=0.2;online_view.generation=3;mcxmap::wanted=3;
 frame=mcxmap::render(online_view,c);assert(frame&&frame->loaded>=2&&!frame->missing&&fake_http_calls>=2);mcxmap::release(frame);
 const int downloads=fake_http_calls;c.online=0;online_view.generation=4;mcxmap::wanted=4;
 frame=mcxmap::render(online_view,c);assert(frame&&!frame->missing&&fake_http_calls==downloads);mcxmap::release(frame);
 const auto pc=mcxmap::project(online_view.latitude,online_view.longitude,online_view.zoom);char cached_file[192];
 mcxmap::paths(cached_file,sizeof(cached_file),c,online_view.zoom,unsigned(floor((pc.x-online_view.width/2.0)/256)),unsigned(floor((pc.y-online_view.height/2.0)/256)));
 mcxmap::clear_tiles(); // Simulate cold cache before testing SD corruption.
 FILE *broken=fopen(cached_file,"wb");assert(broken);fputs("broken",broken);fclose(broken);
 c.online=1;online_view.generation=5;mcxmap::wanted=5;frame=mcxmap::render(online_view,c);
 assert(frame&&!frame->missing&&fake_http_calls==downloads+1);mcxmap::release(frame);
 auto unavailable=online_view;unavailable.longitude+=1;unavailable.generation=6;mcxmap::wanted=6;fake_http_status=503;
 frame=mcxmap::render(unavailable,c);assert(frame&&frame->missing&&fake_http_calls==downloads+2);mcxmap::release(frame);
 unavailable.generation=7;mcxmap::wanted=7;frame=mcxmap::render(unavailable,c);assert(frame&&frame->missing&&fake_http_calls==downloads+2);mcxmap::release(frame);
 fake_free_space=1024*1024;mcxmap::backing_off=false;fake_http_status=200;unavailable.longitude+=1;unavailable.generation=8;mcxmap::wanted=8;
 frame=mcxmap::render(unavailable,c);assert(frame&&frame->missing&&fake_http_calls==downloads+2&&mcxmap::cache_full);mcxmap::release(frame);fake_free_space=1024ULL*1024*1024;
 fake_sd_size=0;unavailable.generation=9;mcxmap::wanted=9;frame=mcxmap::render(unavailable,c);assert(frame&&frame->missing&&fake_http_calls==downloads+2);mcxmap::release(frame);fake_sd_size=1000000;

 auto retry=online_view;retry.longitude+=2;retry.generation=10;mcxmap::wanted=10;mcxmap::backing_off=false;fake_http_enabled=false;
 const int before_retry=fake_http_calls;frame=mcxmap::render(retry,c);assert(frame&&frame->missing&&fake_http_calls==before_retry+1);mcxmap::release(frame);
 lv_tick_inc(10001);fake_http_enabled=true;fake_http_status=200;retry.generation=11;mcxmap::wanted=11;
 frame=mcxmap::render(retry,c);assert(frame&&!frame->missing&&fake_http_calls>before_retry+1);mcxmap::release(frame);

 retry.longitude+=2;retry.generation=12;mcxmap::wanted=12;mcxmap::backing_off=false;fake_read_failure=true;
 const int before_partial=fake_http_calls;frame=mcxmap::render(retry,c);assert(frame&&frame->missing&&fake_http_calls==before_partial+1);mcxmap::release(frame);
 lv_tick_inc(10001);fake_read_failure=false;retry.generation=13;mcxmap::wanted=13;frame=mcxmap::render(retry,c);assert(frame&&!frame->missing);mcxmap::release(frame);
 const auto cancel_generation=mcxmap::wanted.load();mcxmap::queued=true;auto cancelled=mcxmap::configuration();cancelled.online=0;mcxmap::configure(cancelled);
 assert(mcxmap::wanted.load()!=cancel_generation&&!mcxmap::queued);
 unsigned char invalid[33]={137,80,78,71,13,10,26,10};memcpy(invalid+12,"IHDR",4);unsigned char *rgba=nullptr;assert(!mcxmap::decode(invalid,sizeof(invalid),&rgba)&&!rgba);
 c.url[0]=0;assert(!mcxmap::valid_config(c));c=mcxmap::Config{};strcpy(c.url,"http://not-tls.example");assert(!mcxmap::valid_config(c));
 using namespace meshcrossway;
 auto *top=panel_at(lv_scr_act(),0,0,480,34,bg);
 // PRODUCTION_GPS_HEADER
 mcxgnss::fix.valid=true;mcxgnss::fix.satellites=12;mcxgnss::fix.have_satellites=true;mcxgnss::fix.frames=10;gps_refresh(nullptr);lv_obj_update_layout(top);
 assert(!strcmp(lv_label_get_text(gps_satellites),"12")&&lv_obj_get_style_text_color(gps_icon,0).full==green.full);
 lv_area_t badge_area,charge_area;lv_obj_get_coords(gps_satellites,&badge_area);lv_obj_get_coords(top_charge,&charge_area);assert(badge_area.x2<charge_area.x1&&charge_area.x2<430);
 mcxgnss::cfg.enabled=0;gps_refresh(nullptr);assert(!strcmp(lv_label_get_text(gps_satellites),"--"));mcxgnss::cfg.enabled=1;mcxgnss::fix.valid=false;gps_refresh(nullptr);assert(lv_obj_get_style_text_color(gps_icon,0).full==orange.full);
 body=panel_at(lv_scr_act(),40,34,400,286,bg);
 mcxgnss::fix.valid=false;navui::centered=false;mcxmap::queued=false;navui::create(400);advance();
 pointer_x=210;pointer_y=150;down=true;advance();pointer_x=260;advance();down=false;advance();
 navui::request();assert(!mcxmap::queued&&!navui::inflight&&!navui::centered);
 navui::invalidate();lv_obj_clean(body);navui::after_clean();mcxgnss::fix.valid=true;mcxgnss::fix.latitude=v.latitude;mcxgnss::fix.longitude=v.longitude;mcxgnss::fix.satellites=12;mcxgnss::fix.have_satellites=true;
 mcxmap::config.online=0;navui::create(400);advance();
 assert(lv_obj_get_x(navui::canvas)==0&&lv_obj_get_y(navui::canvas)==0&&lv_obj_get_height(navui::canvas)==286);
 assert(lv_obj_get_child_cnt(body)==1); // Map owns the entire body; no heading/footer/buttons.
 assert(lv_obj_get_style_bg_opa(navui::attribution,0)==LV_OPA_TRANSP);
 navui::toggle_zoom();advance();assert(navui::zoom_overlay&&navui::zoom_slider&&navui::zoom_open);
 const auto before_slider=mcxmap::wanted.load();
 lv_event_send(navui::zoom_slider,LV_EVENT_PRESSED,nullptr);
 for(int z=5;z<=17;++z){lv_slider_set_value(navui::zoom_slider,z,LV_ANIM_OFF);lv_event_send(navui::zoom_slider,LV_EVENT_VALUE_CHANGED,nullptr);}
 assert(navui::zoom==14&&navui::pending_zoom==17&&mcxmap::wanted.load()==before_slider+1); // Only cancellation during drag.
 lv_event_send(navui::zoom_slider,LV_EVENT_RELEASED,nullptr);assert(navui::zoom==17&&mcxmap::requested.zoom==17&&!navui::zoom_adjusting);
 assert(navui::zoom_overlay);navui::toggle_zoom();assert(!navui::zoom_overlay&&!navui::zoom_open);
 navui::zoom=14;auto restore=mcxmap::configuration();restore.zoom=14;mcxmap::configure(restore);navui::request();auto current=mcxmap::requested;make_tiles(current);mcxmap::ready=mcxmap::render(current,mcxmap::config);mcxmap::queued=false;navui::poll();assert(navui::front&&!navui::front->missing&&!lv_obj_has_flag(navui::marker,LV_OBJ_FLAG_HIDDEN));
 navui::toggle_zoom();advance();
 lv_event_send(navui::zoom_slider,LV_EVENT_PRESSED,nullptr);lv_slider_set_value(navui::zoom_slider,15,LV_ANIM_OFF);lv_event_send(navui::zoom_slider,LV_EVENT_VALUE_CHANGED,nullptr);
 assert(lv_img_get_zoom(navui::picture)==512&&lv_obj_has_flag(navui::marker,LV_OBJ_FLAG_HIDDEN));
 lv_event_send(navui::zoom_slider,LV_EVENT_RELEASED,nullptr);navui::toggle_zoom();
 navui::zoom=14;restore.zoom=14;mcxmap::configure(restore);navui::request();
 current=mcxmap::requested;mcxmap::ready=mcxmap::render(current,restore);navui::poll();assert(lv_img_get_zoom(navui::picture)==256);
 const double before=navui::longitude;pointer_x=210;pointer_y=150;down=true;advance();for(int x=215;x<=265;x+=5){pointer_x=x;advance(35);}down=false;advance();assert(!navui::following&&navui::longitude<before);
 gpsui::open();advance();assert(gpsui::enabled_button);tap(gpsui::enabled_button);assert(!mcxgnss::configuration().enabled);tap(gpsui::baud_button);assert(mcxgnss::configuration().baud==9600);tap(gpsui::timeout_button);assert(mcxgnss::configuration().timeout_ms==5000);
 gpsui::clock_settings(nullptr);advance();assert(gpsui::clock_status);
 auto *zone_picker=lv_obj_get_child(info_dialog,3);assert(lv_obj_check_type(zone_picker,&lv_dropdown_class));
 lv_dropdown_set_selected(zone_picker,1);lv_event_send(zone_picker,LV_EVENT_VALUE_CHANGED,nullptr);
 assert(mcxclock::selection(mcxgnss::configuration())==1);
 for(unsigned i=0;i<lv_obj_get_child_cnt(info_dialog);++i){lv_area_t a;lv_obj_get_coords(lv_obj_get_child(info_dialog,i),&a);assert(a.x1>=0&&a.x2<480&&a.y1>=0&&a.y2<320);}
 gpsui::map_options();advance();assert(gpsui::provider_picker&&gpsui::online_switch);
 lv_dropdown_set_selected(gpsui::provider_picker,1);lv_event_send(gpsui::provider_picker,LV_EVENT_VALUE_CHANGED,nullptr);
 assert(mcxmap::provider(mcxmap::configuration())==mcxmap::TOPO&&strstr(mcxmap::configuration().attribution,"OpenTopoMap"));
 lv_obj_clear_state(gpsui::online_switch,LV_STATE_CHECKED);lv_event_send(gpsui::online_switch,LV_EVENT_VALUE_CHANGED,nullptr);assert(!mcxmap::configuration().online);
 gpsui::new_poi();advance();lv_textarea_set_text(gpsui::poi_name,"Trail start");gpsui::save_poi(nullptr);assert(mcxnav::last_action==mcxnav::SAVE_POI&&!strcmp(mcxnav::last_name,"Trail start"));
 gpsui::recording();advance();
 for(unsigned i=0;i<4;++i){tap(lv_obj_get_child(gpsui::dialog,i+2));assert(mcxnav::last_action==mcxnav::Action(i+1));}
 gpsui::library(true);advance();assert(info_dialog);gpsui::library(false);advance();assert(info_dialog);
 gpsui::map_diagnostics();advance();assert(info_dialog);gpsui::map_credits();advance();assert(info_dialog);
 info_close_now(nullptr);navui::invalidate();lv_obj_clean(body);navui::after_clean();assert(!navui::front);
 // Full viewport/overlay geometry at all four left/right rail combinations.
 mcxgnss::fix.valid=false;navui::centered=false;
 for(const int width:{276,315,361,400}){
  lv_obj_set_width(body,width);navui::create(width);navui::toggle_zoom();advance();
  lv_area_t canvas_area,slider_area;lv_obj_get_coords(navui::canvas,&canvas_area);lv_obj_get_coords(navui::zoom_slider,&slider_area);
  assert(canvas_area.y1==34&&canvas_area.y2==319&&canvas_area.x2==40+width-1);
  assert(slider_area.x1>=canvas_area.x1&&slider_area.x2<=canvas_area.x2&&slider_area.y2<=canvas_area.y2);
  navui::invalidate();lv_obj_clean(body);navui::after_clean();
  assert(navui::zoom_open);navui::create(width);assert(navui::zoom_overlay);navui::toggle_zoom();assert(!navui::zoom_overlay);
  navui::invalidate();lv_obj_clean(body);navui::after_clean();
 }
 info_close_now(nullptr);context_actions=panel_at(lv_scr_act(),355,34,125,286,panel2);lv_obj_add_flag(context_actions,LV_OBJ_FLAG_SCROLLABLE);
 const int expected[]={A_MAP_HOME,A_MAP_ZOOM,A_MAP_CENTER,A_RECORD,A_TRACKS,A_POIS,A_MAP_DIAGNOSTICS,A_GNSS,A_MAP_OPTIONS};
 for(bool compact:{false,true}){right_compact=compact;rebuild_context();advance();
  for(unsigned i=0;i<9;++i){auto *button=lv_obj_get_child(context_actions,i);assert(lv_obj_get_style_border_width(button,0)==0);lv_event_send(button,LV_EVENT_CLICKED,nullptr);assert(clicked==expected[i]);}
 }
 mcxmap::clear_tiles();
 puts("PASS: HTTPS cache transactions, corrupt cache repair, 503 backoff, no-SD gating, real PNG SD tiles, Mercator raster crop, stale job cancellation, bad PNG/URL, real LVGL map drag, GPS settings and source editor geometry");
}
