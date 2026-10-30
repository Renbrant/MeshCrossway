#pragma once
// Included inside meshcrossway; all widgets are owned by the LVGL task.
namespace navui {
static lv_obj_t *canvas=nullptr,*picture=nullptr,*marker=nullptr,*map_status=nullptr,*attribution=nullptr;
static mcxmap::Frame *front=nullptr;
static lv_obj_t *zoom_overlay=nullptr,*zoom_slider=nullptr,*zoom_value=nullptr;
static bool zoom_open=false,zoom_adjusting=false;
static unsigned pending_zoom=14;
static char status_text[96]="Waiting for GPS";
static void recording();
static void library(bool);
static void draw_overlays(lv_event_t *);
static mcxnav::Snapshot *nav_state=nullptr;
static lv_obj_t *record_badge=nullptr;
static void diagnostics();
static void credits();
static void set_status(const char *s){snprintf(status_text,sizeof(status_text),"%s",s);}
static void preview_zoom(unsigned level){
  if(!picture||!front)return;
  const int difference=int(level)-int(front->view.zoom);
  // Bound the preview transform; it is only feedback while the real view loads.
  unsigned scale=256;
  if(difference>0)scale=256u<<unsigned(difference>2?2:difference);
  else if(difference<0)scale=256u>>unsigned(-difference>2?2:-difference);
  lv_img_set_pivot(picture,front->view.width/2,front->view.height/2);
  lv_img_set_antialias(picture,false);lv_img_set_zoom(picture,uint16_t(scale));
  if(marker)lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
}
static void toggle_zoom();
static bool adjusting(){return zoom_adjusting;}
static bool following=true,inflight=false,dragging=false,centered=false;
static double latitude=0,longitude=0;
static unsigned zoom=14;
static uint32_t generation=0,requested_at=0,completed_at=0,seen_config=0;
static lv_point_t drag_start={};
static int drag_x=0,drag_y=0;
static void invalidate(){
  canvas=picture=marker=map_status=attribution=nullptr;zoom_overlay=zoom_slider=zoom_value=record_badge=nullptr;zoom_adjusting=false;inflight=false;dragging=false;mcxmap::stop();
}
static void after_clean(){if(front){lv_img_cache_invalidate_src(&front->image);mcxmap::release(front);front=nullptr;}}
static void request(){
  if(!canvas||!centered)return;
  mcxmap::View v;v.latitude=latitude;v.longitude=longitude;v.zoom=zoom;v.width=unsigned(lv_obj_get_width(canvas));v.height=unsigned(lv_obj_get_height(canvas));
  seen_config=mcxmap::configuration_revision();
  generation=mcxmap::request(v);inflight=generation!=0;requested_at=uint32_t(esp_timer_get_time()/1000);
  set_status(generation?"Loading map...":"Map unavailable / memory");
}
static void pointer(lv_event_t *event){
  if(!centered||zoom_adjusting||inflight)return;
  auto *input=lv_indev_get_act();if(!input)return;
  lv_point_t p={};lv_indev_get_point(input,&p);const auto code=lv_event_get_code(event);
  if(code==LV_EVENT_PRESSED){drag_start=p;drag_x=drag_y=0;dragging=true;}
  else if(code==LV_EVENT_PRESSING&&dragging){drag_x=p.x-drag_start.x;drag_y=p.y-drag_start.y;
    if(picture)lv_obj_set_pos(picture,drag_x,drag_y);
  }else if((code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)&&dragging){dragging=false;
    if(abs(drag_x)+abs(drag_y)>8){following=false;auto at=mcxmap::project(latitude,longitude,zoom);auto geo=mcxmap::unproject(at.x-drag_x,at.y-drag_y,zoom);latitude=geo.x;longitude=geo.y;request();}
    if(picture)lv_obj_set_pos(picture,0,0);
    drag_x=drag_y=0;
  }
}
static void zoom_changed(lv_event_t *event){
  const auto code=lv_event_get_code(event);
  if(code==LV_EVENT_PRESSED){zoom_adjusting=true;mcxmap::stop();inflight=false;}
  if(code==LV_EVENT_VALUE_CHANGED||code==LV_EVENT_PRESSED){
    pending_zoom=unsigned(lv_slider_get_value(zoom_slider));char value[24];snprintf(value,sizeof(value),"Zoom %u",pending_zoom);
    chatview::label(zoom_value,value);preview_zoom(pending_zoom);
    // Keyboard adjustments have no pointer release: commit on key event below.
  }
  if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST||code==LV_EVENT_KEY){
    zoom_adjusting=false;
    if(pending_zoom!=zoom){zoom=pending_zoom;auto c=mcxmap::configuration();c.zoom=uint8_t(zoom);mcxmap::configure(c);request();}
    else if(centered)request();
  }
}
static void show_zoom(){
  if(!canvas||zoom_overlay)return;
  const int width=lv_obj_get_width(canvas);
  zoom_overlay=panel_at(canvas,8,8,width-16,70,panel2,6);
  lv_obj_add_flag(zoom_overlay,LV_OBJ_FLAG_CLICKABLE);
  pending_zoom=zoom;char value[24];snprintf(value,sizeof(value),"Zoom %u",zoom);
  zoom_value=single_label(zoom_overlay,value,8,4,width-32,white);
  zoom_slider=lv_slider_create(zoom_overlay);lv_obj_set_size(zoom_slider,width-56,14);lv_obj_set_pos(zoom_slider,20,43);
  lv_slider_set_range(zoom_slider,3,int(mcxmap::max_zoom(mcxmap::configuration())));lv_slider_set_value(zoom_slider,int(zoom),LV_ANIM_OFF);
  lv_obj_set_style_bg_color(zoom_slider,orange,LV_PART_INDICATOR);lv_obj_set_style_bg_color(zoom_slider,orange,LV_PART_KNOB);
  lv_obj_add_event_cb(zoom_slider,zoom_changed,LV_EVENT_ALL,nullptr);
}
static void toggle_zoom(){
  zoom_open=!zoom_open;
  if(zoom_open)show_zoom();
  else if(zoom_overlay){lv_obj_del(zoom_overlay);zoom_overlay=zoom_slider=zoom_value=nullptr;zoom_adjusting=false;}
}
static void recenter(lv_event_t *){following=true;const auto f=mcxgnss::snapshot();if(f.valid){latitude=f.latitude;longitude=f.longitude;centered=true;request();}else set_status("Waiting for a fresh GPS fix");}
static void options(lv_event_t *);
static void create(int w){
  mcxmap::load_config();zoom=mcxmap::configuration().zoom;
  const auto f=mcxgnss::snapshot();if(f.valid){latitude=f.latitude;longitude=f.longitude;centered=true;}
  lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  canvas=panel_at(body,0,0,w,lv_obj_get_height(body),lv_color_hex(0x1A2027),0);
  lv_obj_add_flag(canvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_add_event_cb(canvas,pointer,LV_EVENT_ALL,nullptr);
  picture=lv_img_create(canvas);lv_obj_clear_flag(picture,LV_OBJ_FLAG_CLICKABLE);
  if(front){lv_img_cache_invalidate_src(&front->image);mcxmap::release(front);front=nullptr;}
  marker=panel_at(canvas,0,0,12,12,orange,6);lv_obj_set_style_border_width(marker,2,0);lv_obj_set_style_border_color(marker,white,0);lv_obj_clear_flag(marker,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(picture,draw_overlays,LV_EVENT_DRAW_POST,nullptr);
  record_badge=single_label(canvas,"",5,5,60,red);lv_obj_set_style_text_font(record_badge,&lv_font_montserrat_10,0);lv_obj_add_flag(record_badge,LV_OBJ_FLAG_HIDDEN);
  attribution=single_label(canvas,mcxmap::configuration().attribution,3,0,w-6,lv_color_hex(0x202020));
  lv_obj_set_style_text_font(attribution,&lv_font_montserrat_10,0);lv_label_set_long_mode(attribution,LV_LABEL_LONG_WRAP);
  // Tight text-only halo: no opaque strip across the map.
  lv_obj_set_width(attribution,LV_SIZE_CONTENT);lv_obj_set_style_max_width(attribution,w-6,0);
  lv_obj_set_style_bg_opa(attribution,LV_OPA_TRANSP,0);
  lv_obj_add_event_cb(attribution,[](lv_event_t *e){
    auto *label=lv_event_get_target(e);auto *ctx=lv_event_get_draw_ctx(e);
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);lv_obj_init_draw_label_dsc(label,LV_PART_MAIN,&d);d.color=white;
    lv_area_t a;lv_obj_get_coords(label,&a);
    for(int axis=0;axis<4;++axis){auto edge=a;const int dx=axis<2?(axis?1:-1):0,dy=axis>=2?(axis==2?-1:1):0;
      edge.x1+=dx;edge.x2+=dx;edge.y1+=dy;edge.y2+=dy;lv_draw_label(ctx,&d,&edge,lv_label_get_text(label),nullptr);}
  },LV_EVENT_DRAW_MAIN_BEGIN,nullptr);
  lv_obj_update_layout(attribution);lv_obj_set_y(attribution,lv_obj_get_height(canvas)-lv_obj_get_height(attribution)-3);
  lv_obj_add_flag(attribution,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(attribution,[](lv_event_t *){credits();},LV_EVENT_CLICKED,nullptr);
  set_status("Waiting for GPS");if(zoom_open)show_zoom();
  mcxmap::start();if(centered)request();
}
static void poll(){
  if(!nav_state){auto *p=heap_caps_malloc(sizeof(mcxnav::Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(p)nav_state=new(p) mcxnav::Snapshot();}
  if(nav_state)mcxnav::snapshot(*nav_state);
  if(canvas&&nav_state){lv_obj_invalidate(canvas);if(record_badge){const bool active=nav_state->mode==mcxnav::RECORDING||nav_state->mode==mcxnav::PAUSED||nav_state->mode==mcxnav::STORAGE_ERROR;
    if(active){chatview::label(record_badge,nav_state->mode==mcxnav::RECORDING?"REC":nav_state->mode==mcxnav::PAUSED?"PAUSED":"SD ERR");lv_obj_clear_flag(record_badge,LV_OBJ_FLAG_HIDDEN);}else lv_obj_add_flag(record_badge,LV_OBJ_FLAG_HIDDEN);}}
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(auto *next=mcxmap::take()){
    if(next->view.generation==generation){inflight=false;completed_at=now;}
    if(canvas&&next->view.generation==generation&&!zoom_adjusting){
      auto *old=front;front=next;lv_img_set_zoom(picture,256);lv_img_set_src(picture,&front->image);lv_obj_clear_flag(picture,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(picture,dragging?drag_x:0,dragging?drag_y:0);
      if(old){lv_img_cache_invalidate_src(&old->image);mcxmap::release(old);}
      set_status(front->status);
    }else mcxmap::release(next);
  }
  if(!canvas||info_dialog||zoom_adjusting)return;
  if(seen_config!=mcxmap::configuration_revision()){
    seen_config=mcxmap::configuration_revision();const auto c=mcxmap::configuration();zoom=c.zoom;
    if(zoom_slider){lv_slider_set_range(zoom_slider,3,int(mcxmap::max_zoom(c)));lv_slider_set_value(zoom_slider,int(zoom),LV_ANIM_OFF);pending_zoom=zoom;}
    if(front&&front->source!=mcxmap::source_hash(c))lv_obj_add_flag(picture,LV_OBJ_FLAG_HIDDEN);
    chatview::label(attribution,c.attribution);lv_obj_update_layout(attribution);lv_obj_set_y(attribution,lv_obj_get_height(canvas)-lv_obj_get_height(attribution)-3);
    if(centered)request();
  }
  if(inflight&&uint32_t(now-requested_at)>30000){inflight=false;set_status("Map timeout / check SD or Wi-Fi");}
  const auto f=mcxgnss::snapshot();
  if(f.valid&&following&&!dragging&&!inflight){
    const auto old=mcxmap::project(latitude,longitude,zoom),pos=mcxmap::project(f.latitude,f.longitude,zoom);
    if(!centered||fabs(mcxmap::delta(pos.x,old.x,zoom))+fabs(pos.y-old.y)>16){latitude=f.latitude;longitude=f.longitude;centered=true;request();}
  }
  if(!inflight&&centered&&(!front||front->missing)&&uint32_t(now-completed_at)>4000){completed_at=now;request();}
  if(f.valid&&front&&!dragging&&front->view.zoom==zoom){
    const auto origin=mcxmap::project(front->view.latitude,front->view.longitude,front->view.zoom),pos=mcxmap::project(f.latitude,f.longitude,front->view.zoom);
    const int x=int(lround(mcxmap::delta(pos.x,origin.x,front->view.zoom)+double(front->view.width)/2));const int y=int(lround(pos.y-origin.y+double(front->view.height)/2));
    if(x>=0&&x<int(front->view.width)&&y>=0&&y<int(front->view.height)){lv_obj_clear_flag(marker,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(marker,x-6,y-6);}
    else lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
  }else lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
}
static void center_position(const mcxnav::Sample &p){following=false;latitude=p.latitude;longitude=p.longitude;centered=true;request();}
static void draw_overlays(lv_event_t *e){
  if(!nav_state||!front||dragging||zoom_adjusting||front->view.zoom!=zoom)return;
  auto *ctx=lv_event_get_draw_ctx(e);lv_area_t area;lv_obj_get_coords(canvas,&area);
  const auto origin=mcxmap::project(front->view.latitude,front->view.longitude,front->view.zoom);
  auto point=[&](const mcxnav::Sample &p){const auto v=mcxmap::project(p.latitude,p.longitude,front->view.zoom);return lv_point_t{lv_coord_t(lround(mcxmap::delta(v.x,origin.x,front->view.zoom)+front->view.width/2.0)+area.x1),lv_coord_t(lround(v.y-origin.y+front->view.height/2.0)+area.y1)};};
  lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.color=orange;line.width=3;line.round_start=line.round_end=1;
  auto draw_track=[&](const mcxnav::Sample *points,unsigned count,uint32_t color){line.color=lv_color_hex(color);
  for(unsigned i=1;i<count;++i){const auto &a=points[i-1],&b=points[i];if(a.segment!=b.segment)continue;
    // Skip far-away vertices before converting to LVGL's narrow screen coordinates.
    const auto pa=mcxmap::project(a.latitude,a.longitude,front->view.zoom),pb=mcxmap::project(b.latitude,b.longitude,front->view.zoom);
    if(fabs(mcxmap::delta(pa.x,origin.x,front->view.zoom))>10000||fabs(pa.y-origin.y)>10000||fabs(mcxmap::delta(pb.x,origin.x,front->view.zoom))>10000||fabs(pb.y-origin.y)>10000)continue;
    auto aa=point(a),bb=point(b);lv_draw_line(ctx,&line,&aa,&bb);
  }
  };
  for(unsigned i=0;i<nav_state->overlay_count;++i){const auto &o=nav_state->overlays[i];draw_track(o.points,o.count,o.color);}
  for(const auto &style:nav_state->preferences.styles)if(style.id==nav_state->preview_id&&style.visible){
    bool saved=false;for(unsigned i=0;i<nav_state->overlay_count;++i)saved|=nav_state->overlays[i].id==style.id;
    if(!saved)draw_track(nav_state->preview,nav_state->preview_count,style.color);
  }
  for(unsigned i=0;i<nav_state->poi_count;++i){const auto &p=nav_state->pois[i];const auto at=mcxmap::project(p.point.latitude,p.point.longitude,front->view.zoom);if(fabs(mcxmap::delta(at.x,origin.x,front->view.zoom))>double(front->view.width)/2+10||fabs(at.y-origin.y)>double(front->view.height)/2+10)continue;
    const auto q=point(p.point);lv_area_t a={lv_coord_t(q.x-4),lv_coord_t(q.y-4),lv_coord_t(q.x+4),lv_coord_t(q.y+4)};lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=cyan;d.border_color=white;d.border_width=1;d.radius=LV_RADIUS_CIRCLE;lv_draw_rect(ctx,&d,&a);
  }
}

}
namespace gpsui {
static lv_obj_t *dialog=nullptr,*status_label=nullptr,*metrics_label=nullptr,*enabled_button=nullptr,*baud_button=nullptr,*timeout_button=nullptr,*units_button=nullptr;
static lv_obj_t *map_save_label=nullptr,*clock_status=nullptr;
static lv_obj_t *edit_value=nullptr,*edit_keyboard=nullptr,*edit_notice=nullptr;
static int editing=0;
static lv_obj_t *nav_detail=nullptr;static unsigned nav_dialog=0;
static void nav_refresh();
static void open();
static void map_options();
static void map_diagnostics();
static void map_credits();
static void refresh(){
  if(dialog!=info_dialog||!dialog){dialog=nullptr;return;}
  nav_refresh();
  if(map_save_label)chatview::label(map_save_label,mcxmap::settings_saved.load()?"Settings saved / visible tiles only":"Settings pending / save failed");
  if(clock_status){char clock[8],detail[160];mcxclock::label(clock,sizeof(clock));
    snprintf(detail,sizeof(detail),"%s / %s\n%s",clock,mcxclock::zone_name(),mcxclock::state.synced?"GPS UTC synchronized / continues without signal":"Waiting for valid GPS UTC date and time");chatview::label(clock_status,detail);
  }
  if(!status_label)return;
  const auto c=mcxgnss::configuration();const auto d=mcxgnss::status();const auto f=mcxgnss::snapshot();char state[160],label[64];
  snprintf(state,sizeof(state),"UART %u / error %d / %lu bytes\nNMEA %lu / bad %lu / settings %s",d.baud,d.error,(unsigned long)d.bytes,(unsigned long)f.frames,(unsigned long)f.errors,d.saved?"saved":"pending / save failed");chatview::label(status_label,state);
  snprintf(label,sizeof(label),"GPS reading: %s",c.enabled?"ON":"OFF");if(enabled_button)chatview::label(lv_obj_get_child(enabled_button,0),label);
  if(c.baud)snprintf(label,sizeof(label),"UART baud: %lu",(unsigned long)c.baud);else snprintf(label,sizeof(label),"UART baud: AUTO");if(baud_button)chatview::label(lv_obj_get_child(baud_button,0),label);
  snprintf(label,sizeof(label),"Fix expiry: %lu seconds",(unsigned long)(c.timeout_ms/1000));if(timeout_button)chatview::label(lv_obj_get_child(timeout_button,0),label);
  if(metrics_label){char detail[100],altitude[24]="--",speed[24]="--";
    if(f.valid&&f.have_altitude)snprintf(altitude,sizeof(altitude),"%.1f %s",f.altitude*(c.imperial?3.28084:1),c.imperial?"ft":"m");
    if(f.valid&&f.have_speed)snprintf(speed,sizeof(speed),"%.1f %s",f.speed_kmh*(c.imperial?0.621371:1),c.imperial?"mph":"km/h");
    snprintf(detail,sizeof(detail),"Altitude %s / Speed %s",altitude,speed);chatview::label(metrics_label,detail);
  }
  if(units_button)chatview::label(lv_obj_get_child(units_button,0),c.imperial?"Units: ft / mph":"Units: m / km/h");
}
static void base(const char *title){
  info_close_now(nullptr);
  dialog=info_dialog=panel_at(body,0,0,lv_obj_get_width(body),286,bg,0);
  navigation_panel=true;
  lv_obj_add_flag(dialog,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(dialog,LV_DIR_VER);
  lv_obj_set_flex_flow(dialog,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(dialog,8,0);
  lv_obj_set_style_pad_row(dialog,10,0);
  single_label(dialog,title,0,0,lv_obj_get_width(body)-16,orange);
  // Apply after the page builder has populated its controls. Flex layout keeps
  // every control inside the centre column while both navigation rails remain live.
  lv_async_call([](void *object){
    if(object!=info_dialog||!navigation_panel)return;
    auto *parent=static_cast<lv_obj_t *>(object);
    const int width=lv_obj_get_width(body)-16;
    for(unsigned i=0;i<lv_obj_get_child_cnt(parent);++i){
      auto *child=lv_obj_get_child(parent,i);lv_obj_set_width(child,width);
      if(lv_obj_check_type(child,&lv_label_class)){
        lv_label_set_long_mode(child,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(child,LV_SIZE_CONTENT);
      }
    }
  },dialog);
  map_save_label=clock_status=nav_detail=nullptr;nav_dialog=0;
  metrics_label=status_label=enabled_button=baud_button=timeout_button=units_button=nullptr;
  edit_value=edit_keyboard=edit_notice=nullptr;
}
static void change(lv_event_t *event){auto c=mcxgnss::configuration();const int id=int(intptr_t(lv_event_get_user_data(event)));
  if(id==0)c.enabled=!c.enabled;
  if(id==1)c.baud=c.baud==0?9600:c.baud==9600?38400:c.baud==38400?115200:0;
  if(id==2)c.timeout_ms=c.timeout_ms==3000?5000:c.timeout_ms==5000?10000:3000;
  if(id==3)c.imperial=!c.imperial;
  mcxgnss::configure(c);refresh();
}
static void clock_settings(lv_event_t *){
  base("CLOCK / TIME ZONE");
  single_label(dialog,"Time: automatic GPS UTC / 24-hour display",8,44,440,white);
  auto *zones=lv_dropdown_create(dialog);lv_obj_set_pos(zones,8,77);lv_obj_set_size(zones,440,38);
  lv_dropdown_clear_options(zones);lv_dropdown_add_option(zones,"Automatic / GPS location",LV_DROPDOWN_POS_LAST);
  for(unsigned i=0;i<mcxtz::count;++i)lv_dropdown_add_option(zones,mcxtz::zones[i].name,LV_DROPDOWN_POS_LAST);
  const unsigned selected=mcxclock::selection(mcxgnss::configuration());
  lv_dropdown_set_selected(zones,selected<=mcxtz::count?selected:0);
  lv_obj_add_event_cb(zones,[](lv_event_t *event){auto c=mcxgnss::configuration();mcxclock::select(c,lv_dropdown_get_selected(lv_event_get_target(event)));mcxgnss::configure(c);mcxclock::poll(mcxgnss::snapshot(),c);refresh();},LV_EVENT_VALUE_CHANGED,nullptr);
  clock_status=single_label(dialog,"",8,126,440,cyan);lv_label_set_long_mode(clock_status,LV_LABEL_LONG_WRAP);
  single_label(dialog,"DST follows the selected zone automatically.",8,197,440,white);
  single_label(dialog,"GPS zone estimates can differ near borders.",8,225,440,muted);
  single_label(dialog,"Select a zone manually when needed. No internet required.",8,251,440,muted);refresh();
}
static void open(){
  base("LOCAL GPS / SETTINGS");
  enabled_button=action_button(dialog,"",8,42,216,34,panel2,change,0);baud_button=action_button(dialog,"",232,42,216,34,panel2,change,1);
  timeout_button=action_button(dialog,"",8,82,216,34,panel2,change,2);units_button=action_button(dialog,"",232,82,216,34,panel2,change,3);
  status_label=single_label(dialog,"",8,126,440,white);lv_label_set_long_mode(status_label,LV_LABEL_LONG_WRAP);
  metrics_label=single_label(dialog,"",8,177,440,cyan);
  single_label(dialog,"RX44 / TX43 fixed. Reading OFF does not power off GPS.",8,216,440,muted);
  action_button(dialog,"CLOCK / TIME ZONE",8,241,440,34,panel2,clock_settings,0);refresh();
}
static void nav_refresh(){
  if(!nav_detail||!navui::nav_state||!nav_dialog)return;
  const auto &s=*navui::nav_state;char text[224];
  if(nav_dialog==2){chatview::label(nav_detail,s.status);return;}
  snprintf(text,sizeof(text),"%s\nTrack %08lx / %lu points / %.2f km\n%u saved tracks / %u POIs",s.status,(unsigned long)s.current,(unsigned long)s.points,s.metres/1000,s.track_count,s.poi_count);chatview::label(nav_detail,text);
}
static void nav_action(lv_event_t *e){const auto a=mcxnav::Action(intptr_t(lv_event_get_user_data(e)));if(!mcxnav::submit(a)&&nav_detail)chatview::label(nav_detail,"Storage worker unavailable or busy; try again.");}
static void preset_list();
static void recording(){
  base("RECORD TRACK");mcxnav::start();navui::poll();
  nav_dialog=1;nav_detail=single_label(dialog,"",8,45,440,white);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);nav_refresh();
  action_button(dialog,"START",8,148,104,40,orange,nav_action,mcxnav::START);
  action_button(dialog,"PAUSE",120,148,104,40,panel2,nav_action,mcxnav::PAUSE);
  action_button(dialog,"RESUME",232,148,104,40,panel2,nav_action,mcxnav::RESUME);
  action_button(dialog,"FINISH",344,148,104,40,panel2,nav_action,mcxnav::STOP);
  if(navui::nav_state){auto *picker=lv_dropdown_create(dialog);char options[128]="";for(unsigned i=0;i<4;++i){if(i)strcat(options,"\n");strcat(options,navui::nav_state->preferences.presets[i].name);}lv_dropdown_set_options(picker,options);lv_dropdown_set_selected(picker,navui::nav_state->preferences.selected);lv_obj_add_event_cb(picker,[](lv_event_t *e){mcxnav::submit(mcxnav::SELECT_PRESET,lv_dropdown_get_selected(lv_event_get_target(e)));},LV_EVENT_VALUE_CHANGED,nullptr);}
  action_button(dialog,"EDIT RECORDING PRESETS",8,195,300,40,panel2,[](lv_event_t *){lv_async_call([](void *){preset_list();},nullptr);},0);
  auto *help=single_label(dialog,"Valid GPS positions save to microSD.\nPause or lost fix creates a separate track segment.\nFinish writes a GPX file; no radio transmission.",8,207,440,muted);lv_label_set_long_mode(help,LV_LABEL_LONG_WRAP);
}
// Device settings use the same worker commands and SD preferences as HTTP.
static unsigned preset_slot=0;
static mcxnav::Preset preset_draft;
static lv_obj_t *setting_value=nullptr,*settings_owner=nullptr;
static unsigned setting_field=0;
static uint32_t setting_track=0;
static void preset_form();
static void preset_list();
static void setting_editor(unsigned field);
static bool settings_send(const mcxnav::Command &command){
  const bool queued=mcxnav::enqueue(command);
  if(nav_detail)chatview::label(nav_detail,queued?"Queued / waiting for SD confirmation":"Storage busy / retry SAVE");
  return queued;
}
static void setting_apply(){
  if(!setting_value||settings_owner!=info_dialog)return;
  const char *text=lv_textarea_get_text(setting_value);
  if(setting_field==0){
    if(!*text||strlen(text)>23){chatview::label(nav_detail,"Enter a name (1-23 characters).");return;}
    snprintf(preset_draft.name,sizeof(preset_draft.name),"%s",text);
  }else{
    const unsigned length=unsigned(strlen(text));
    bool valid=length>0&&length<=6;
    for(const char *p=text;*p;++p)valid=valid&&((*p>='0'&&*p<='9')||(setting_field==3&&((*p>='a'&&*p<='f')||(*p>='A'&&*p<='F'))));
    const unsigned long value=valid?strtoul(text,nullptr,setting_field==3?16:10):0;
    if(setting_field==3){
      if(!valid||length!=6){chatview::label(nav_detail,"Enter six hex digits, for example FF8A24.");return;}
      mcxnav::Command command;command.action=mcxnav::SET_COLOR;command.id=setting_track;command.value=uint32_t(value);
      if(settings_send(command)){setting_value=nullptr;base("TRACK COLOR / SAVING");nav_dialog=2;nav_detail=single_label(dialog,"Waiting for SD confirmation",8,40,300,muted);}
      return;
    }
    const unsigned max=setting_field==1?1000:300,min=setting_field==1?3:1;
    if(!valid||value<min||value>max){chatview::label(nav_detail,setting_field==1?"Distance must be 3-1000 metres.":"Interval must be 1-300 seconds.");return;}
    if(setting_field==1)preset_draft.distance_m=uint32_t(value);else preset_draft.interval_s=uint32_t(value);
  }
  setting_value=nullptr;preset_form();
}
static void setting_editor(unsigned field){
  setting_field=field;
  base(field==0?"PRESET NAME":field==1?"DISTANCE / METRES":field==2?"INTERVAL / SECONDS":"TRACK COLOR / HEX");
  settings_owner=dialog;lv_obj_set_style_pad_row(dialog,4,0);
  char value[32];
  if(field==0)snprintf(value,sizeof(value),"%s",preset_draft.name);
  else if(field==3){uint32_t rgb=0xFF8A24;if(navui::nav_state)for(const auto &s:navui::nav_state->preferences.styles)if(s.id==setting_track)rgb=s.color;snprintf(value,sizeof(value),"%06lX",(unsigned long)rgb);}
  else snprintf(value,sizeof(value),"%lu",(unsigned long)(field==1?preset_draft.distance_m:preset_draft.interval_s));
  setting_value=lv_textarea_create(dialog);lv_textarea_set_one_line(setting_value,true);lv_obj_set_height(setting_value,40);lv_textarea_set_max_length(setting_value,field==0?23:field==3?6:4);
  lv_textarea_set_accepted_chars(setting_value,field==0?"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-":field==3?"0123456789abcdefABCDEF":"0123456789");
  lv_textarea_set_text(setting_value,value);
  action_button(dialog,"APPLY",8,80,300,38,orange,[](lv_event_t *){lv_async_call([](void *){setting_apply();},nullptr);},0);
  nav_detail=single_label(dialog,field==3?"Six hex digits; APPLY saves to SD.":field==1?"Distance: 3-1000 metres.":field==2?"Interval: 1-300 seconds.":"Draft: APPLY, then SAVE AND USE.",8,120,300,muted);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);lv_obj_set_height(nav_detail,LV_SIZE_CONTENT);lv_obj_set_style_text_font(nav_detail,&lv_font_montserrat_10,0);
  auto *keyboard=lv_keyboard_create(dialog);lv_obj_set_height(keyboard,130);
  lv_keyboard_set_mode(keyboard,field==1||field==2?LV_KEYBOARD_MODE_NUMBER:LV_KEYBOARD_MODE_TEXT_UPPER);lv_keyboard_set_textarea(keyboard,setting_value);
  lv_obj_add_event_cb(keyboard,[](lv_event_t *){lv_async_call([](void *){setting_apply();},nullptr);},LV_EVENT_READY,nullptr);
  lv_async_call([](void *owner){if(owner==dialog&&setting_value)lv_obj_scroll_to_view(setting_value,LV_ANIM_OFF);},dialog);
}
static void preset_form(){
  base("EDIT RECORDING PRESET");
  char text[96];
  snprintf(text,sizeof(text),"Name: %s",preset_draft.name);
  action_button(dialog,text,8,40,300,40,panel2,[](lv_event_t *){lv_async_call([](void *){setting_editor(0);},nullptr);},0);
  snprintf(text,sizeof(text),"Distance: %lu metres",(unsigned long)preset_draft.distance_m);
  action_button(dialog,text,8,90,300,40,panel2,[](lv_event_t *){lv_async_call([](void *){setting_editor(1);},nullptr);},0);
  snprintf(text,sizeof(text),"Interval: %lu seconds",(unsigned long)preset_draft.interval_s);
  action_button(dialog,text,8,140,300,40,panel2,[](lv_event_t *){lv_async_call([](void *){setting_editor(2);},nullptr);},0);
  action_button(dialog,"SAVE AND USE PRESET",8,190,300,40,orange,[](lv_event_t *){
    mcxnav::Command command;command.action=mcxnav::SAVE_PRESET;command.id=preset_slot;command.value=1;command.distance_m=preset_draft.distance_m;command.interval_s=preset_draft.interval_s;snprintf(command.name,sizeof(command.name),"%s",preset_draft.name);settings_send(command);
  },0);
  nav_dialog=2;nav_detail=single_label(dialog,"Edit fields, then SAVE AND USE. Changes apply to recording immediately.",8,235,300,muted);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);lv_obj_set_height(nav_detail,LV_SIZE_CONTENT);
}
static void preset_list(){
  base("RECORDING PRESETS");navui::poll();if(!navui::nav_state)return;
  for(unsigned i=0;i<4;++i){const auto &p=navui::nav_state->preferences.presets[i];char text[96];snprintf(text,sizeof(text),"%s%s / %lu m / %lu s",i==navui::nav_state->preferences.selected?"* ":"",p.name,(unsigned long)p.distance_m,(unsigned long)p.interval_s);
    action_button(dialog,text,8,40+int(i)*46,300,40,panel2,[](lv_event_t *event){lv_async_call([](void *slot){preset_slot=unsigned(uintptr_t(slot));if(navui::nav_state){preset_draft=navui::nav_state->preferences.presets[preset_slot];preset_form();}},lv_event_get_user_data(event));},i);
  }
  auto *help=single_label(dialog,"Choose a slot to edit or create your own preset. All settings are stored on the device SD card.",8,240,300,muted);lv_label_set_long_mode(help,LV_LABEL_LONG_WRAP);lv_obj_set_height(help,LV_SIZE_CONTENT);
}
static lv_obj_t *poi_name=nullptr;static uint32_t chosen_poi=0;
static void save_poi(lv_event_t *){
  const auto *name=lv_textarea_get_text(poi_name);
  if(!mcxnav::submit(mcxnav::SAVE_POI,0,name)){if(nav_detail)chatview::label(nav_detail,"Fresh GPS fix required; name at most 39 UTF-8 bytes.");return;}
  base("SAVING POI");nav_dialog=2;nav_detail=single_label(dialog,"Saving to microSD...",8,55,440,white);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);
}
static void new_poi(){
  base("SAVE CURRENT GPS AS POI");
  poi_name=lv_textarea_create(dialog);lv_obj_set_pos(poi_name,8,42);lv_obj_set_size(poi_name,330,38);lv_textarea_set_one_line(poi_name,true);lv_textarea_set_max_length(poi_name,39);
  lv_textarea_set_text(poi_name,"POI");action_button(dialog,"SAVE",346,42,102,38,orange,[](lv_event_t *){lv_async_call([](void *){save_poi(nullptr);},nullptr);},0);
  nav_detail=single_label(dialog,"Name this location. A fresh GPS fix is required.",8,84,440,muted);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);
  auto *keyboard=lv_keyboard_create(dialog);lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(keyboard,6,127);lv_obj_set_size(keyboard,448,151);lv_keyboard_set_textarea(keyboard,poi_name);
}
static void show_location(const mcxnav::Sample &p){info_close_now(nullptr);page=4;map_view=0;draw();navui::center_position(p);}
static void poi_details(uint32_t id){
  if(!navui::nav_state)return;
  for(unsigned i=0;i<navui::nav_state->poi_count;++i)if(navui::nav_state->pois[i].id==id){const auto p=navui::nav_state->pois[i];chosen_poi=id;base("POI");
    auto *l=single_label(dialog,p.name,8,48,440,white);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);char text[128];snprintf(text,sizeof(text),"%.7f, %.7f\nSaved on microSD",p.point.latitude,p.point.longitude);l=single_label(dialog,text,8,95,440,muted);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);
    action_button(dialog,"SHOW ON MAP",8,169,216,40,orange,[](lv_event_t *){lv_async_call([](void *){if(navui::nav_state)for(unsigned j=0;j<navui::nav_state->poi_count;++j)if(navui::nav_state->pois[j].id==chosen_poi){const auto p=navui::nav_state->pois[j].point;show_location(p);break;}},nullptr);},0);
    action_button(dialog,"DELETE...",232,169,216,40,panel2,[](lv_event_t *){lv_async_call([](void *){base("DELETE THIS POI?");single_label(dialog,"This removes the saved POI from the SD card.",8,64,440,white);action_button(dialog,"CONFIRM DELETE",8,130,440,42,orange,[](lv_event_t *){mcxnav::submit(mcxnav::DELETE_POI,chosen_poi);info_close(nullptr);},0);},nullptr);},0);return;
  }
}
static void track_details(uint32_t id){
  base("TRACK / DISPLAY");if(!navui::nav_state)return;nav_dialog=2;nav_detail=single_label(dialog,navui::nav_state->status,8,30,300,muted);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);char title[64];snprintf(title,sizeof(title),"Track %08lx",(unsigned long)id);single_label(dialog,title,8,40,300,white);
  const mcxnav::Style *style=nullptr;for(const auto &v:navui::nav_state->preferences.styles)if(v.id==id)style=&v;
  auto *toggle=lv_switch_create(dialog);if(style&&style->visible)lv_obj_add_state(toggle,LV_STATE_CHECKED);
  lv_obj_add_event_cb(toggle,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::SET_VISIBLE;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));c.value=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED);mcxnav::enqueue(c);},LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)id);
  single_label(dialog,"Visible on map",8,80,300,muted);
  auto *colors=lv_dropdown_create(dialog);lv_dropdown_set_options(colors,"Orange\nBlue\nGreen\nRed\nPurple\nYellow");if(style)for(unsigned i=0;i<6;++i)if(style->color==mcxnav::TRACK_COLORS[i])lv_dropdown_set_selected(colors,i);
  lv_obj_add_event_cb(colors,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::SET_COLOR;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));c.value=mcxnav::TRACK_COLORS[lv_dropdown_get_selected(lv_event_get_target(e))%6];mcxnav::enqueue(c);},LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)id);
  action_button(dialog,"CUSTOM RGB COLOR",8,140,300,40,panel2,[](lv_event_t *e){setting_track=uint32_t(uintptr_t(lv_event_get_user_data(e)));lv_async_call([](void *){setting_editor(3);},nullptr);},id);
  action_button(dialog,"EXPORT GPX TO SD",8,190,300,40,panel2,[](lv_event_t *e){mcxnav::Command command;command.action=mcxnav::EXPORT_TRACK;command.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));settings_send(command);},id);
  action_button(dialog,"CENTER ON TRACK",8,160,300,40,panel2,[](lv_event_t *e){const auto id=uint32_t(uintptr_t(lv_event_get_user_data(e)));if(navui::nav_state)for(unsigned i=0;i<navui::nav_state->track_count;++i)if(navui::nav_state->tracks[i].id==id&&navui::nav_state->tracks[i].points){show_location(navui::nav_state->tracks[i].first);break;}},id);
}
static void library(bool pois){
  base(pois?"SAVED POIs":"SAVED TRACKS");mcxnav::start();navui::poll();
  if(pois)action_button(dialog,"SAVE CURRENT GPS AS POI",8,40,440,36,orange,[](lv_event_t *){lv_async_call([](void *){new_poi();},nullptr);},0);
  auto *list=panel_at(dialog,8,pois?82:44,440,pois?170:208,bg,0);lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(list,LV_DIR_VER);
  const auto *s=navui::nav_state;const unsigned n=s?(pois?s->poi_count:s->track_count):0;
  if(!n)single_label(list,"No saved items yet / SD worker may be loading",4,8,425,muted);
  for(unsigned i=0;i<n;++i){char title[100];uint32_t id;
    if(pois){id=s->pois[i].id;snprintf(title,sizeof(title),"%s",s->pois[i].name);}
    else {const auto &t=s->tracks[i];id=t.id;snprintf(title,sizeof(title),"Track %08lx / %.2f km / %lu pts%s",(unsigned long)id,t.metres/1000,(unsigned long)t.points,t.recovered?" / recovered":"");}
    action_button(list,title,0,int(i)*42,425,38,panel2,pois?+[](lv_event_t *e){lv_async_call([](void *p){poi_details(uint32_t(uintptr_t(p)));},lv_event_get_user_data(e));}:+[](lv_event_t *e){lv_async_call([](void *p){const uint32_t id=uint32_t(uintptr_t(p));track_details(id);},lv_event_get_user_data(e));},id);
  }
  auto *help=single_label(dialog,pois?"POIs export automatically to navigation/pois.gpx.":"Finished/recovered tracks have a GPX beside the journal.",8,259,440,muted);lv_obj_set_style_text_font(help,&lv_font_montserrat_10,0);
}
static lv_obj_t *provider_picker=nullptr,*online_switch=nullptr;
static void provider_changed(lv_event_t *){auto c=mcxmap::configuration();mcxmap::select_provider(c,lv_dropdown_get_selected(provider_picker));mcxmap::configure(c);}
static void online_changed(lv_event_t *){auto c=mcxmap::configuration();c.online=lv_obj_has_state(online_switch,LV_STATE_CHECKED);mcxmap::configure(c);}
static void map_diagnostics(){
  base("MAP / DIAGNOSTICS");
  auto *detail=single_label(dialog,navui::status_text,8,50,440,white);lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
  const auto c=mcxmap::configuration();char text[320];snprintf(text,sizeof(text),"Zoom %u / %s\nSD %s / worker error %d\nSettings %s\nRAM tile hits %lu / PNG decodes %lu\nLast composition %lu ms",navui::zoom,c.online?"online + offline":"offline only",esp_sdcard_port_get_size()?"available":"unavailable",int(mcxmap::last_error.load()),mcxmap::settings_saved.load()?"saved":"pending",(unsigned long)mcxmap::cache_hits.load(),(unsigned long)mcxmap::decode_count.load(),(unsigned long)mcxmap::last_render_ms.load());
  detail=single_label(dialog,text,8,115,440,muted);lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
}
static void map_credits(){
  base("MAP / CREDITS");const auto c=mcxmap::configuration();
  auto *label=single_label(dialog,c.attribution,8,48,440,white);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
  label=single_label(dialog,c.url,8,100,440,muted);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
  label=single_label(dialog,mcxmap::provider(c)==mcxmap::TOPO?"Data: OpenStreetMap (ODbL), SRTM.\nStyle: OpenTopoMap (CC-BY-SA).\nhttps://opentopomap.org/about":"OpenStreetMap data: Open Database License (ODbL).\nhttps://www.openstreetmap.org/copyright",8,163,440,white);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
}
static void map_options(){
  base("MAP OPTIONS");mcxmap::start();const auto c=mcxmap::configuration();
  single_label(dialog,"Map style",8,48,140,white);
  provider_picker=lv_dropdown_create(dialog);lv_obj_set_pos(provider_picker,8,73);lv_obj_set_size(provider_picker,440,40);
  lv_dropdown_set_options(provider_picker,"Street / OpenStreetMap\nTopographic / OpenTopoMap");lv_dropdown_set_selected(provider_picker,unsigned(mcxmap::provider(c)));
  lv_obj_add_event_cb(provider_picker,provider_changed,LV_EVENT_VALUE_CHANGED,nullptr);
  single_label(dialog,"Download visible map tiles",8,137,330,white);
  online_switch=lv_switch_create(dialog);lv_obj_set_pos(online_switch,385,132);lv_obj_set_size(online_switch,58,30);if(c.online)lv_obj_add_state(online_switch,LV_STATE_CHECKED);
  lv_obj_add_event_cb(online_switch,online_changed,LV_EVENT_VALUE_CHANGED,nullptr);
  auto *help=single_label(dialog,"OFF uses SD maps and cached tiles only.\nProvider credits are applied automatically.\nNo area downloads or mesh position sharing.",8,184,440,muted);lv_label_set_long_mode(help,LV_LABEL_LONG_WRAP);
  map_save_label=single_label(dialog,"",8,258,440,muted);refresh();
}
}
namespace navui {
static void options(lv_event_t *){lv_async_call([](void *){gpsui::map_options();},nullptr);}
static void diagnostics(){gpsui::map_diagnostics();}
static void recording(){lv_async_call([](void *){gpsui::recording();},nullptr);}
static void library(bool pois){lv_async_call([](void *v){gpsui::library(v!=nullptr);},pois?(void *)1:nullptr);}
static void credits(){lv_async_call([](void *){gpsui::map_credits();},nullptr);}
}
static void gps_settings_open(){gpsui::open();}
