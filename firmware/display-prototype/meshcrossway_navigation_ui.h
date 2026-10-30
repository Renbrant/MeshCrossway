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
static void set_hidden(lv_obj_t *object,bool hidden){
  if(!object||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN)==hidden)return;
  if(hidden)lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(object,LV_OBJ_FLAG_HIDDEN);
}
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
  if(marker)set_hidden(marker,true);
}
static void toggle_zoom();
static bool adjusting(){return zoom_adjusting;}
static bool following=true,inflight=false,dragging=false,centered=false;
static double latitude=0,longitude=0;
static unsigned zoom=14;
static uint32_t generation=0,requested_at=0,completed_at=0,seen_config=0;
static lv_point_t drag_start={};
static int drag_x=0,drag_y=0,pan_x=0,pan_y=0;static bool gesture_moved=false;
static constexpr unsigned MAX_DRAW_SEGMENTS=mcxnav::MAX_VISIBLE*(mcxnav::OVERLAY_POINTS-1)+mcxnav::MAX_PREVIEW-1;
struct DrawSegment {lv_point_t a,b;uint32_t color;};
struct OverlayCache {DrawSegment lines[MAX_DRAW_SEGMENTS];lv_point_t pois[mcxnav::MAX_POIS];unsigned line_count=0,poi_count=0;uint32_t generation=0,revision=0;bool ready=false;};
static OverlayCache *overlay_cache=nullptr;
static uint32_t overlay_builds=0;
static uint32_t overlay_signature(){
  if(!nav_state)return 0;
  const uint32_t fields[]={nav_state->preview_id,nav_state->preview_count,nav_state->points,nav_state->overlay_count,nav_state->poi_count};
  return mcxnav::checksum(fields,sizeof(fields))^mcxnav::checksum(nav_state->preferences.styles,sizeof(nav_state->preferences.styles))^mcxnav::checksum(nav_state->pois,nav_state->poi_count*sizeof(mcxnav::Poi));
}
static void rebuild_overlay_cache();

static void invalidate(){
  canvas=picture=marker=map_status=attribution=nullptr;zoom_overlay=zoom_slider=zoom_value=record_badge=nullptr;zoom_adjusting=false;inflight=false;dragging=false;pan_x=pan_y=drag_x=drag_y=0;if(overlay_cache)overlay_cache->ready=false;mcxmap::stop();
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
  if(!centered||zoom_adjusting||!front)return;
  auto *input=lv_indev_get_act();if(!input)return;
  lv_point_t p={};lv_indev_get_point(input,&p);const auto code=lv_event_get_code(event);
  if(code==LV_EVENT_PRESSED){drag_start=p;drag_x=pan_x;drag_y=pan_y;dragging=true;gesture_moved=false;}
  else if(code==LV_EVENT_PRESSING&&dragging){drag_x=pan_x+p.x-drag_start.x;drag_y=pan_y+p.y-drag_start.y;
    drag_x=drag_x<-8192?-8192:drag_x>8192?8192:drag_x;drag_y=drag_y<-8192?-8192:drag_y>8192?8192:drag_y;
    if(abs(p.x-drag_start.x)+abs(p.y-drag_start.y)>8)gesture_moved=true;
    if(picture)lv_obj_set_pos(picture,drag_x,drag_y);
  }else if((code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)&&dragging){dragging=false;
    if(gesture_moved){following=false;auto at=mcxmap::project(front->view.latitude,front->view.longitude,zoom);auto geo=mcxmap::unproject(at.x-drag_x,at.y-drag_y,zoom);latitude=geo.x;longitude=geo.y;request();}
    pan_x=drag_x;pan_y=drag_y;
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
  marker=panel_at(canvas,0,0,12,12,orange,6);lv_obj_set_style_border_width(marker,2,0);lv_obj_set_style_border_color(marker,white,0);lv_obj_clear_flag(marker,LV_OBJ_FLAG_CLICKABLE);set_hidden(marker,true);
  lv_obj_add_event_cb(picture,draw_overlays,LV_EVENT_DRAW_POST,nullptr);
  record_badge=single_label(canvas,"",5,5,60,red);lv_obj_set_style_text_font(record_badge,&lv_font_montserrat_10,0);set_hidden(record_badge,true);
  map_status=single_label(canvas,"Loading map...",5,24,w-10,muted);lv_obj_set_style_text_font(map_status,&lv_font_montserrat_10,0);set_hidden(map_status,true);
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
  if(canvas&&nav_state&&!info_dialog){if(record_badge){const bool active=nav_state->mode==mcxnav::RECORDING||nav_state->mode==mcxnav::PAUSED||nav_state->mode==mcxnav::STORAGE_ERROR;
    if(active){chatview::label(record_badge,nav_state->mode==mcxnav::RECORDING?"REC":nav_state->mode==mcxnav::PAUSED?"PAUSED":"SD ERR");set_hidden(record_badge,false);}else set_hidden(record_badge,true);}}
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(!dragging)if(auto *next=mcxmap::take()){
    if(next->view.generation==generation){inflight=false;completed_at=now;}
    if(canvas&&next->view.generation==generation&&!zoom_adjusting&&!dragging){
      auto *old=front;front=next;pan_x=pan_y=drag_x=drag_y=0;lv_img_set_zoom(picture,256);lv_img_set_src(picture,&front->image);set_hidden(picture,false);lv_obj_set_pos(picture,dragging?drag_x:0,dragging?drag_y:0);
      if(old){lv_img_cache_invalidate_src(&old->image);mcxmap::release(old);}
      set_status(front->status);
    }else mcxmap::release(next);
  }
  if(map_status&&!info_dialog){const bool waiting=inflight||(front&&front->missing);if(waiting)chatview::label(map_status,inflight?"Loading map...":mcxmap::configuration().online?"Map tiles unavailable / retrying":"Offline / tiles not saved");set_hidden(map_status,!waiting);}
  if(!canvas||info_dialog||zoom_adjusting)return;
  if(front&&nav_state&&(!overlay_cache||!overlay_cache->ready||overlay_cache->generation!=front->view.generation||overlay_cache->revision!=overlay_signature())){rebuild_overlay_cache();lv_obj_invalidate(picture);}

  if(seen_config!=mcxmap::configuration_revision()){
    seen_config=mcxmap::configuration_revision();const auto c=mcxmap::configuration();zoom=c.zoom;
    if(zoom_slider){lv_slider_set_range(zoom_slider,3,int(mcxmap::max_zoom(c)));lv_slider_set_value(zoom_slider,int(zoom),LV_ANIM_OFF);pending_zoom=zoom;}
    if(front&&front->source!=mcxmap::source_hash(c))set_hidden(picture,true);
    chatview::label(attribution,c.attribution);lv_obj_update_layout(attribution);lv_obj_set_y(attribution,lv_obj_get_height(canvas)-lv_obj_get_height(attribution)-3);
    if(centered)request();
  }
  if(inflight&&uint32_t(now-requested_at)>30000){inflight=false;set_status("Map timeout / check SD or Wi-Fi");}
  const auto f=mcxgnss::snapshot();
  if(f.valid&&following&&!dragging&&!inflight){
    const auto old=mcxmap::project(latitude,longitude,zoom),pos=mcxmap::project(f.latitude,f.longitude,zoom);
    if(!centered||fabs(mcxmap::delta(pos.x,old.x,zoom))+fabs(pos.y-old.y)>16){latitude=f.latitude;longitude=f.longitude;centered=true;request();}
  }
  if(!dragging&&!inflight&&centered&&(!front||front->missing||pan_x||pan_y)&&uint32_t(now-completed_at)>4000){completed_at=now;request();}
  if(f.valid&&front&&!dragging&&!pan_x&&!pan_y&&front->view.zoom==zoom){
    const auto origin=mcxmap::project(front->view.latitude,front->view.longitude,front->view.zoom),pos=mcxmap::project(f.latitude,f.longitude,front->view.zoom);
    const int x=int(lround(mcxmap::delta(pos.x,origin.x,front->view.zoom)+double(front->view.width)/2));const int y=int(lround(pos.y-origin.y+double(front->view.height)/2));
    if(x>=0&&x<int(front->view.width)&&y>=0&&y<int(front->view.height)){set_hidden(marker,false);lv_obj_set_pos(marker,x-6,y-6);}
    else set_hidden(marker,true);
  }else set_hidden(marker,true);
}
static void center_position(const mcxnav::Sample &p){following=false;latitude=p.latitude;longitude=p.longitude;centered=true;request();}
static void rebuild_overlay_cache(){
  if(!front||!nav_state||front->view.zoom!=zoom)return;
  if(!overlay_cache){auto *memory=heap_caps_malloc(sizeof(OverlayCache),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return;overlay_cache=new(memory) OverlayCache();}
  auto &cache=*overlay_cache;
  if(cache.ready&&cache.generation==front->view.generation&&cache.revision==overlay_signature())return;
  cache.line_count=cache.poi_count=0;
  const auto origin=mcxmap::project(front->view.latitude,front->view.longitude,front->view.zoom);
  auto project=[&](const mcxnav::Sample &sample,lv_point_t &point){
    const auto p=mcxmap::project(sample.latitude,sample.longitude,front->view.zoom);
    const double x=mcxmap::delta(p.x,origin.x,front->view.zoom),y=p.y-origin.y;
    if(fabs(x)>10000||fabs(y)>10000)return false;
    point={lv_coord_t(lround(x+front->view.width/2.0)),lv_coord_t(lround(y+front->view.height/2.0))};return true;
  };
  auto track=[&](const mcxnav::Sample *points,unsigned count,uint32_t color){
    lv_point_t previous={};bool previous_valid=false;
    for(unsigned i=0;i<count;++i){lv_point_t point={};const bool valid=project(points[i],point);
      if(i&&valid&&previous_valid&&points[i-1].segment==points[i].segment&&cache.line_count<MAX_DRAW_SEGMENTS)cache.lines[cache.line_count++]={previous,point,color};
      previous=point;previous_valid=valid;
    }
  };
  for(unsigned i=0;i<nav_state->overlay_count;++i){const auto &o=nav_state->overlays[i];track(o.points,o.count,o.color);}
  for(const auto &style:nav_state->preferences.styles)if(style.id==nav_state->preview_id&&style.visible){
    bool saved=false;for(unsigned i=0;i<nav_state->overlay_count;++i)saved|=nav_state->overlays[i].id==style.id;
    if(!saved)track(nav_state->preview,nav_state->preview_count,style.color);
  }
  for(unsigned i=0;i<nav_state->poi_count;++i){lv_point_t point={};if(project(nav_state->pois[i].point,point))cache.pois[cache.poi_count++]=point;}
  cache.generation=front->view.generation;cache.revision=overlay_signature();cache.ready=true;++overlay_builds;
}
static void draw_overlays(lv_event_t *event){
  // Called once per DMA stripe: use only integer coordinates cached outside draw.
  if(!front||!overlay_cache||!overlay_cache->ready||zoom_adjusting||front->view.zoom!=zoom)return;
  auto *ctx=lv_event_get_draw_ctx(event);lv_area_t area;lv_obj_get_coords(picture,&area);
  const auto &clip=*ctx->clip_area;
  lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.width=3;line.round_start=line.round_end=1;
  for(unsigned i=0;i<overlay_cache->line_count;++i){const auto &s=overlay_cache->lines[i];
    lv_point_t a={lv_coord_t(s.a.x+area.x1),lv_coord_t(s.a.y+area.y1)},b={lv_coord_t(s.b.x+area.x1),lv_coord_t(s.b.y+area.y1)};
    if((a.x<clip.x1-3&&b.x<clip.x1-3)||(a.x>clip.x2+3&&b.x>clip.x2+3)||(a.y<clip.y1-3&&b.y<clip.y1-3)||(a.y>clip.y2+3&&b.y>clip.y2+3))continue;
    line.color=lv_color_hex(s.color);lv_draw_line(ctx,&line,&a,&b);
  }
  for(unsigned i=0;i<overlay_cache->poi_count;++i){const auto &p=overlay_cache->pois[i];lv_area_t a={lv_coord_t(p.x+area.x1-4),lv_coord_t(p.y+area.y1-4),lv_coord_t(p.x+area.x1+4),lv_coord_t(p.y+area.y1+4)};
    if(a.x2<clip.x1||a.x1>clip.x2||a.y2<clip.y1||a.y1>clip.y2)continue;
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=cyan;d.border_color=white;d.border_width=1;d.radius=LV_RADIUS_CIRCLE;lv_draw_rect(ctx,&d,&a);
  }
}

}
namespace gpsui {
static lv_obj_t *dialog=nullptr,*status_label=nullptr,*metrics_label=nullptr,*enabled_button=nullptr,*baud_button=nullptr,*timeout_button=nullptr,*units_button=nullptr;
static lv_obj_t *map_save_label=nullptr,*clock_status=nullptr;
static lv_obj_t *edit_value=nullptr,*edit_keyboard=nullptr,*edit_notice=nullptr;
static int editing=0;
static lv_obj_t *nav_detail=nullptr;static unsigned nav_dialog=0;
static lv_obj_t *record_start=nullptr,*record_pause=nullptr,*record_resume=nullptr,*record_finish=nullptr,*record_mode=nullptr;
static lv_obj_t *name_value=nullptr,*name_owner=nullptr,*name_save=nullptr,*name_default=nullptr,*delete_action=nullptr,*delete_cancel=nullptr;
static bool record_pending=false,name_waiting=false,name_finishing=false,delete_waiting=false,start_editing=false;
static mcxnav::Action pending_action=mcxnav::NONE;static uint32_t pending_since=0,name_track=0,start_color=0xFF8A24;static unsigned start_slot=0;
static uint32_t name_revision=0,delete_revision=0;
static char name_expected[40]={};
static void recording();static void preset_list();static void start_setup();static void prepare_start();static void name_form(uint32_t,bool);static void track_details(uint32_t);static void library(bool);
static void compact_base(const char *);static lv_obj_t *control_row(int height=36);static lv_obj_t *control_button(lv_obj_t *,const char *,lv_color_t,lv_event_cb_t,intptr_t);
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
    if(object!=info_dialog||!navigation_panel||lv_obj_has_flag(info_dialog,LV_OBJ_FLAG_USER_1))return;
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
static void compact_base(const char *title){base(title);lv_obj_add_flag(dialog,LV_OBJ_FLAG_USER_1);lv_obj_set_style_pad_row(dialog,6,0);lv_obj_set_width(lv_obj_get_child(dialog,0),LV_PCT(100));}
static lv_obj_t *control_row(int height){auto *row=panel_at(dialog,0,0,lv_obj_get_width(body)-16,height,bg,0);lv_obj_set_width(row,LV_PCT(100));lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_set_style_pad_column(row,8,0);return row;}
static lv_obj_t *control_button(lv_obj_t *parent,const char *title,lv_color_t color,lv_event_cb_t cb,intptr_t data){auto *button=action_button(parent,title,0,0,100,36,color,cb,data);lv_obj_set_width(button,0);lv_obj_set_flex_grow(button,1);auto *label=lv_obj_get_child(button,0);lv_obj_set_x(label,0);lv_obj_set_width(label,LV_PCT(100));lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);lv_obj_set_style_bg_color(button,panel2,LV_STATE_DISABLED);lv_obj_set_style_bg_color(button,lv_color_hex(0xB65E12),LV_STATE_PRESSED);return button;}
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
static uint32_t library_folder=0xffffffffu,folder_filter_ids[mcxnav::MAX_FOLDERS+2]={},folder_move_ids[mcxnav::MAX_FOLDERS+1]={};
static lv_obj_t *folder_value=nullptr,*folder_owner=nullptr,*folder_save=nullptr;
static uint32_t folder_edit_id=0,folder_revision=0;static mcxnav::Action folder_action=mcxnav::NONE;static bool folder_waiting=false;static char folder_expected[32]={};
static void nav_refresh(){
  if(!navui::nav_state||!dialog||dialog!=info_dialog)return;
  const auto &s=*navui::nav_state;
  if(nav_dialog==9&&folder_waiting){bool done=false;
    if(folder_action==mcxnav::DELETE_FOLDER){done=true;for(const auto &f:s.folders)if(f.id==folder_edit_id)done=false;}
    else for(const auto &f:s.folders)if(f.id&&(!folder_edit_id||f.id==folder_edit_id)&&!strcmp(f.name,folder_expected)){done=true;library_folder=f.id;}
    if(done&&s.revision!=folder_revision){folder_waiting=false;lv_async_call([](void *owner){if(info_dialog==owner&&nav_dialog==9)library(false);},dialog);}
    else if(s.revision!=folder_revision&&(strstr(s.status,"failed")||strstr(s.status,"exists")||strstr(s.status,"limit")||strstr(s.status,"unavailable"))){folder_waiting=false;chatview::label(nav_detail,s.status);lv_obj_clear_state(folder_save,LV_STATE_DISABLED);}
    return;
  }
  if(nav_dialog==1){
    const bool active=s.mode==mcxnav::RECORDING||s.mode==mcxnav::PAUSED;
    if(record_pending&&((pending_action==mcxnav::START_CONFIGURED&&active)||(pending_action==mcxnav::PAUSE&&s.mode==mcxnav::PAUSED)||(pending_action==mcxnav::RESUME&&s.mode==mcxnav::RECORDING)||s.mode==mcxnav::STORAGE_ERROR))record_pending=false;
    if(record_pending&&uint32_t(lv_tick_get()-pending_since)>5000){record_pending=false;}
    char summary[128];const char *state=s.mode==mcxnav::RECORDING?"Recording":s.mode==mcxnav::PAUSED?"Paused":s.mode==mcxnav::STORAGE_ERROR?"SD error":"Ready to record";
    if(active)snprintf(summary,sizeof(summary),"%s  /  %lu points  /  %.2f km",state,(unsigned long)s.points,s.metres/1000);else snprintf(summary,sizeof(summary),"%s",state);
    chatview::label(nav_detail,record_pending?"Please wait...":summary);
    if(record_mode){snprintf(summary,sizeof(summary),"Mode: %s",active?s.recording_preset.name:s.preferences.presets[s.preferences.selected].name);chatview::label(record_mode,summary);}
    auto enabled=[](lv_obj_t *button,bool yes){if(!button)return;if(yes)lv_obj_clear_state(button,LV_STATE_DISABLED);else lv_obj_add_state(button,LV_STATE_DISABLED);};
    enabled(record_start,!active&&!record_pending);enabled(record_pause,s.mode==mcxnav::RECORDING&&!record_pending);enabled(record_resume,s.mode==mcxnav::PAUSED&&!record_pending);enabled(record_finish,active&&!record_pending);
    return;
  }
  if(nav_dialog==7&&name_waiting){
    bool done=name_finishing?s.mode==mcxnav::STOPPED:false;
    if(!name_finishing)for(unsigned i=0;i<s.track_count;++i)if(s.tracks[i].id==name_track&&!strcmp(s.tracks[i].name,name_expected))done=true;
    if(done&&s.revision!=name_revision){name_waiting=false;lv_async_call([](void *owner){if(info_dialog!=owner||nav_dialog!=7)return;if(name_finishing)recording();else track_details(name_track);},dialog);return;}
    if(s.revision!=name_revision&&(strstr(s.status,"failed")||s.mode==mcxnav::STORAGE_ERROR)){name_waiting=false;chatview::label(nav_detail,s.status);lv_obj_clear_state(name_save,LV_STATE_DISABLED);lv_obj_clear_state(name_default,LV_STATE_DISABLED);}
    return;
  }
  if(nav_dialog==8&&delete_waiting){bool found=false;for(unsigned i=0;i<s.track_count;++i)found|=s.tracks[i].id==name_track;if(!found){delete_waiting=false;lv_async_call([](void *owner){if(info_dialog==owner&&nav_dialog==8)library(false);},dialog);}else if(s.revision!=delete_revision&&(strstr(s.status,"failed")||strstr(s.status,"before deleting"))){delete_waiting=false;chatview::label(nav_detail,s.status);lv_obj_clear_state(delete_action,LV_STATE_DISABLED);lv_obj_clear_state(delete_cancel,LV_STATE_DISABLED);}return;}
  if(nav_dialog==6){if(nav_detail){const bool error=strstr(s.status,"failed")||strstr(s.status,"unavailable")||strstr(s.status,"limit:");navui::set_hidden(nav_detail,!error);if(error)chatview::label(nav_detail,s.status);}return;}
  if(!nav_detail||!nav_dialog)return;
  if(nav_dialog==2)chatview::label(nav_detail,s.status);
}
static void nav_action(lv_event_t *e){
  const auto action=mcxnav::Action(intptr_t(lv_event_get_user_data(e)));
  if(mcxnav::submit(action)){record_pending=true;pending_action=action;pending_since=lv_tick_get();nav_refresh();}
  else if(nav_detail)chatview::label(nav_detail,"Storage busy / retry");
}
static void recording(){
  compact_base("RECORD TRACK");mcxnav::start();navui::poll();nav_dialog=1;
  nav_detail=single_label(dialog,"Ready to record",0,0,lv_obj_get_width(body)-16,white);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);lv_obj_set_height(nav_detail,LV_SIZE_CONTENT);
  record_mode=single_label(dialog,"",0,0,lv_obj_get_width(body)-16,muted);
  auto *row=control_row();
  record_start=control_button(row,"START",orange,[](lv_event_t *){lv_async_call([](void *){prepare_start();},nullptr);},0);
  record_pause=control_button(row,"PAUSE",panel2,nav_action,mcxnav::PAUSE);
  row=control_row();record_resume=control_button(row,"RESUME",panel2,nav_action,mcxnav::RESUME);
  record_finish=control_button(row,"FINISH",orange,[](lv_event_t *){lv_async_call([](void *){if(!navui::nav_state)return;if(navui::nav_state->mode==mcxnav::RECORDING&&!mcxnav::submit(mcxnav::PAUSE)){chatview::label(nav_detail,"Storage busy / retry");return;}name_form(navui::nav_state->current,true);},nullptr);},0);
  row=control_row();control_button(row,"PRESETS",panel2,[](lv_event_t *){lv_async_call([](void *){preset_list();},nullptr);},0);control_button(row,"TRACKS",panel2,[](lv_event_t *){lv_async_call([](void *){library(false);},nullptr);},0);
  nav_refresh();
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
  if(!queued&&nav_detail&&nav_dialog==6){navui::set_hidden(nav_detail,false);chatview::label(nav_detail,"Storage busy / retry");}
  if(nav_detail&&nav_dialog!=6)chatview::label(nav_detail,queued?"Queued / waiting for SD confirmation":"Storage busy / retry SAVE");
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
      if(settings_send(command)){setting_value=nullptr;track_details(setting_track);}
      return;
    }
    const unsigned max=setting_field==1?1000:300,min=setting_field==1?3:1;
    if(!valid||value<min||value>max){chatview::label(nav_detail,setting_field==1?"Distance must be 3-1000 metres.":"Interval must be 1-300 seconds.");return;}
    if(setting_field==1)preset_draft.distance_m=uint32_t(value);else preset_draft.interval_s=uint32_t(value);
  }
  setting_value=nullptr;if(start_editing)start_setup();else preset_form();
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
  start_editing=false;
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
static void prepare_start(){
  navui::poll();if(!navui::nav_state)return;
  start_slot=navui::nav_state->preferences.selected;start_color=0xFF8A24;
  preset_draft=navui::nav_state->preferences.presets[start_slot];start_setup();
}
static void start_setup(){
  compact_base("NEW TRACK");
  single_label(dialog,"Recording mode",0,0,lv_obj_get_width(body)-16,muted);
  auto *picker=lv_dropdown_create(dialog);lv_obj_set_width(picker,LV_PCT(100));lv_obj_set_height(picker,34);char options[160]="";
  if(navui::nav_state)for(unsigned i=0;i<4;++i){if(i)strcat(options,"\n");strcat(options,navui::nav_state->preferences.presets[i].name);}
  strcat(options,"\nCustom / this track");
  lv_dropdown_set_options(picker,options);lv_dropdown_set_selected(picker,start_slot);
  lv_obj_add_event_cb(picker,[](lv_event_t *e){start_slot=lv_dropdown_get_selected(lv_event_get_target(e));lv_async_call([](void *){start_setup();},nullptr);},LV_EVENT_VALUE_CHANGED,nullptr);
  if(start_slot==4){auto *row=control_row();char text[48];snprintf(text,sizeof(text),"%lu m",(unsigned long)preset_draft.distance_m);control_button(row,text,panel2,[](lv_event_t *){start_editing=true;lv_async_call([](void *){setting_editor(1);},nullptr);},0);snprintf(text,sizeof(text),"%lu s",(unsigned long)preset_draft.interval_s);control_button(row,text,panel2,[](lv_event_t *){start_editing=true;lv_async_call([](void *){setting_editor(2);},nullptr);},0);}
  single_label(dialog,"Track color",0,0,lv_obj_get_width(body)-16,muted);
  auto *colors=lv_dropdown_create(dialog);lv_obj_set_width(colors,LV_PCT(100));lv_obj_set_height(colors,34);lv_dropdown_set_options(colors,"Orange\nBlue\nGreen\nRed\nPurple\nYellow");for(unsigned i=0;i<6;++i)if(start_color==mcxnav::TRACK_COLORS[i])lv_dropdown_set_selected(colors,i);
  lv_obj_add_event_cb(colors,[](lv_event_t *e){start_color=mcxnav::TRACK_COLORS[lv_dropdown_get_selected(lv_event_get_target(e))%6];},LV_EVENT_VALUE_CHANGED,nullptr);
  auto *row=control_row();control_button(row,"RECORD",orange,[](lv_event_t *){mcxnav::Command c;c.action=mcxnav::START_CONFIGURED;c.id=start_slot;c.value=start_color;c.distance_m=preset_draft.distance_m;c.interval_s=preset_draft.interval_s;if(mcxnav::enqueue(c)){record_pending=true;pending_action=c.action;pending_since=lv_tick_get();lv_async_call([](void *){recording();},nullptr);}else if(nav_detail)chatview::label(nav_detail,"Storage busy / retry");},0);
  control_button(row,"CANCEL",panel2,[](lv_event_t *){lv_async_call([](void *){recording();},nullptr);},0);
  nav_detail=single_label(dialog,"",0,0,lv_obj_get_width(body)-16,red);
}
static void commit_name(bool use_default){
  if(name_waiting||!name_value||name_owner!=info_dialog)return;
  char name[40];if(use_default)snprintf(name,sizeof(name),"Track %08lx",(unsigned long)name_track);else snprintf(name,sizeof(name),"%s",lv_textarea_get_text(name_value));
  const auto bytes=strlen(lv_textarea_get_text(name_value));if(!use_default&&(!bytes||bytes>39)){chatview::label(nav_detail,"Name: 1-39 UTF-8 bytes");return;}
  mcxnav::Command command;command.action=name_finishing?mcxnav::STOP:mcxnav::RENAME_TRACK;command.id=name_track;snprintf(command.name,sizeof(command.name),"%s",name);
  if(!mcxnav::enqueue(command)){chatview::label(nav_detail,"Storage busy / retry");return;}
  snprintf(name_expected,sizeof(name_expected),"%s",name);name_waiting=true;name_revision=navui::nav_state?navui::nav_state->revision:0;chatview::label(nav_detail,"Saving...");lv_obj_add_state(name_save,LV_STATE_DISABLED);lv_obj_add_state(name_default,LV_STATE_DISABLED);
}
static void name_form(uint32_t id,bool finish){
  char name[40];snprintf(name,sizeof(name),"Track %08lx",(unsigned long)id);if(navui::nav_state)for(unsigned i=0;i<navui::nav_state->track_count;++i)if(navui::nav_state->tracks[i].id==id&&navui::nav_state->tracks[i].name[0])snprintf(name,sizeof(name),"%s",navui::nav_state->tracks[i].name);
  compact_base(finish?"FINISH / SAVE TRACK":"RENAME TRACK");lv_obj_set_style_pad_row(dialog,4,0);name_track=id;name_finishing=finish;name_waiting=false;nav_dialog=7;name_owner=dialog;
  name_value=lv_textarea_create(dialog);lv_obj_set_width(name_value,LV_PCT(100));lv_obj_set_height(name_value,36);lv_textarea_set_one_line(name_value,true);lv_textarea_set_max_length(name_value,39);lv_textarea_set_text(name_value,name);
  auto *row=control_row();name_save=control_button(row,"SAVE",orange,[](lv_event_t *){lv_async_call([](void *){commit_name(false);},nullptr);},0);name_default=control_button(row,"USE DEFAULT",panel2,[](lv_event_t *){lv_async_call([](void *){commit_name(true);},nullptr);},0);
  nav_detail=single_label(dialog,"Choose a name or keep the default.",0,0,lv_obj_get_width(body)-16,muted);lv_obj_set_style_text_font(nav_detail,&lv_font_montserrat_10,0);
  auto *keyboard=lv_keyboard_create(dialog);lv_obj_set_width(keyboard,LV_PCT(100));lv_obj_set_height(keyboard,124);lv_keyboard_set_textarea(keyboard,name_value);
  lv_obj_add_event_cb(keyboard,[](lv_event_t *){lv_async_call([](void *){commit_name(false);},nullptr);},LV_EVENT_READY,nullptr);
}
static void confirm_delete(uint32_t id){
  compact_base("DELETE TRACK?");name_track=id;delete_waiting=false;nav_dialog=8;
  single_label(dialog,"Delete this track and its GPX?",0,0,lv_obj_get_width(body)-16,white);
  auto *row=control_row();delete_action=control_button(row,"DELETE",red,[](lv_event_t *e){if(delete_waiting)return;mcxnav::Command c;c.action=mcxnav::DELETE_TRACK;c.id=name_track;if(mcxnav::enqueue(c)){delete_waiting=true;delete_revision=navui::nav_state?navui::nav_state->revision:0;lv_obj_add_state(lv_event_get_target(e),LV_STATE_DISABLED);lv_obj_add_state(delete_cancel,LV_STATE_DISABLED);chatview::label(nav_detail,"Deleting...");}else chatview::label(nav_detail,"Storage busy / retry");},0);
  delete_cancel=control_button(row,"CANCEL",panel2,[](lv_event_t *){lv_async_call([](void *){track_details(name_track);},nullptr);},0);nav_detail=single_label(dialog,"This cannot be undone.",0,0,lv_obj_get_width(body)-16,muted);
}
static void folder_form(uint32_t id){
  compact_base(id?"RENAME FOLDER":"NEW FOLDER");nav_dialog=9;folder_edit_id=id;folder_owner=dialog;folder_waiting=false;folder_action=id?mcxnav::RENAME_FOLDER:mcxnav::CREATE_FOLDER;
  folder_value=lv_textarea_create(dialog);lv_obj_set_width(folder_value,LV_PCT(100));lv_obj_set_height(folder_value,36);lv_textarea_set_one_line(folder_value,true);lv_textarea_set_max_length(folder_value,31);
  if(id&&navui::nav_state)for(const auto &f:navui::nav_state->folders)if(f.id==id)lv_textarea_set_text(folder_value,f.name);
  auto save=[](lv_event_t *){if(folder_owner!=info_dialog||folder_waiting)return;const char *name=lv_textarea_get_text(folder_value);if(!mcxnav::Store::folder_name_valid(name)){chatview::label(nav_detail,"Name: 1-31 bytes / no slashes");return;}mcxnav::Command c;c.action=folder_action;c.id=folder_edit_id;snprintf(c.name,sizeof(c.name),"%s",name);if(!mcxnav::enqueue(c)){chatview::label(nav_detail,"Storage busy / retry");return;}snprintf(folder_expected,sizeof(folder_expected),"%s",name);folder_waiting=true;folder_revision=navui::nav_state?navui::nav_state->revision:0;lv_obj_add_state(folder_save,LV_STATE_DISABLED);chatview::label(nav_detail,"Saving folder...");};
  auto *row=control_row();folder_save=control_button(row,"SAVE",orange,save,0);control_button(row,"CANCEL",panel2,[](lv_event_t *){lv_async_call([](void *){library(false);},nullptr);},0);
  nav_detail=single_label(dialog,"Folder names stay on your SD card.",0,0,lv_obj_get_width(body)-16,muted);lv_obj_set_style_text_font(nav_detail,&lv_font_montserrat_10,0);
  auto *keyboard=lv_keyboard_create(dialog);lv_obj_set_width(keyboard,LV_PCT(100));lv_obj_set_height(keyboard,124);lv_keyboard_set_textarea(keyboard,folder_value);lv_obj_add_event_cb(keyboard,save,LV_EVENT_READY,nullptr);
}
static void remove_folder(){
  if(!library_folder||library_folder==0xffffffffu)return;
  compact_base("REMOVE FOLDER?");nav_dialog=9;folder_owner=dialog;folder_edit_id=library_folder;folder_action=mcxnav::DELETE_FOLDER;folder_waiting=false;
  auto *label=single_label(dialog,"Tracks are kept in Unfiled.",0,0,lv_obj_get_width(body)-16,white);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_obj_set_height(label,LV_SIZE_CONTENT);
  auto *row=control_row();folder_save=control_button(row,"REMOVE",red,[](lv_event_t *){if(folder_waiting)return;mcxnav::Command c;c.action=mcxnav::DELETE_FOLDER;c.id=folder_edit_id;if(mcxnav::enqueue(c)){folder_waiting=true;folder_revision=navui::nav_state?navui::nav_state->revision:0;lv_obj_add_state(folder_save,LV_STATE_DISABLED);chatview::label(nav_detail,"Removing folder...");}},0);control_button(row,"CANCEL",panel2,[](lv_event_t *){lv_async_call([](void *){library(false);},nullptr);},0);nav_detail=single_label(dialog,"",0,0,lv_obj_get_width(body)-16,muted);
}
static void track_folders(){
  compact_base("SAVED TRACKS");mcxnav::start();navui::poll();const auto *s=navui::nav_state;
  auto *picker=lv_dropdown_create(dialog);lv_obj_set_width(picker,LV_PCT(100));lv_obj_set_height(picker,32);lv_dropdown_clear_options(picker);lv_dropdown_add_option(picker,"All tracks",LV_DROPDOWN_POS_LAST);lv_dropdown_add_option(picker,"Unfiled",LV_DROPDOWN_POS_LAST);folder_filter_ids[0]=0xffffffffu;folder_filter_ids[1]=0;unsigned options=2,selected=0;
  if(s)for(const auto &f:s->folders)if(f.id){folder_filter_ids[options]=f.id;lv_dropdown_add_option(picker,f.name,LV_DROPDOWN_POS_LAST);if(f.id==library_folder)selected=options;++options;}
  if(!library_folder){selected=1;}
  if(selected==0){library_folder=0xffffffffu;}
  lv_dropdown_set_selected(picker,selected);
  lv_obj_add_event_cb(picker,[](lv_event_t *e){library_folder=folder_filter_ids[lv_dropdown_get_selected(lv_event_get_target(e))];lv_async_call([](void *){library(false);},nullptr);},LV_EVENT_VALUE_CHANGED,nullptr);
  auto *row=control_row();control_button(row,"NEW FOLDER",panel2,[](lv_event_t *){lv_async_call([](void *){folder_form(0);},nullptr);},0);control_button(row,"REFRESH",panel2,[](lv_event_t *){lv_async_call([](void *){library(false);},nullptr);},0);
  if(library_folder&&library_folder!=0xffffffffu){row=control_row();control_button(row,"RENAME",panel2,[](lv_event_t *){lv_async_call([](void *){folder_form(library_folder);},nullptr);},0);control_button(row,"REMOVE",panel2,[](lv_event_t *){lv_async_call([](void *){remove_folder();},nullptr);},0);}
  auto *list=panel_at(dialog,0,0,lv_obj_get_width(body)-16,150,bg,0);lv_obj_set_width(list,LV_PCT(100));lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(list,LV_DIR_VER);unsigned shown=0;
  if(s)for(unsigned i=0;i<s->track_count;++i){const auto &t=s->tracks[i];if(library_folder!=0xffffffffu&&t.folder!=library_folder)continue;char text[80];snprintf(text,sizeof(text),"%s / %.2f km",t.name,t.metres/1000);auto *button=action_button(list,text,0,shown++*42,lv_obj_get_width(body)-16,38,panel2,[](lv_event_t *e){lv_async_call([](void *id){track_details(uint32_t(uintptr_t(id)));},lv_event_get_user_data(e));},t.id);lv_obj_set_width(button,LV_PCT(100));lv_obj_set_width(lv_obj_get_child(button,0),LV_PCT(100));}
  if(!shown)single_label(list,"No tracks in this folder.",0,0,lv_obj_get_width(body)-16,muted);
}

static void track_details(uint32_t id){
  compact_base("TRACK DETAILS");navui::poll();if(!navui::nav_state)return;nav_dialog=6;
  const mcxnav::Track *track=nullptr;for(unsigned i=0;i<navui::nav_state->track_count;++i)if(navui::nav_state->tracks[i].id==id)track=&navui::nav_state->tracks[i];
  char title[64];snprintf(title,sizeof(title),"Track %08lx",(unsigned long)id);auto *label=single_label(dialog,track&&track->name[0]?track->name:title,0,0,lv_obj_get_width(body)-16,white);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_obj_set_height(label,LV_SIZE_CONTENT);
  single_label(dialog,"Folder",0,0,lv_obj_get_width(body)-16,muted);
  auto *folders=lv_dropdown_create(dialog);lv_obj_set_width(folders,LV_PCT(100));lv_obj_set_height(folders,32);lv_dropdown_clear_options(folders);lv_dropdown_add_option(folders,"Unfiled",LV_DROPDOWN_POS_LAST);folder_move_ids[0]=0;unsigned folder_count=1,folder_selected=0;
  for(const auto &f:navui::nav_state->folders)if(f.id){folder_move_ids[folder_count]=f.id;lv_dropdown_add_option(folders,f.name,LV_DROPDOWN_POS_LAST);if(track&&track->folder==f.id)folder_selected=folder_count;++folder_count;}
  lv_dropdown_set_selected(folders,folder_selected);lv_obj_add_event_cb(folders,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::MOVE_TRACK;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));c.value=folder_move_ids[lv_dropdown_get_selected(lv_event_get_target(e))];settings_send(c);},LV_EVENT_VALUE_CHANGED,(void*)(uintptr_t)id);
  const mcxnav::Style *style=nullptr;for(const auto &v:navui::nav_state->preferences.styles)if(v.id==id)style=&v;
  auto *row=control_row(28);label=single_label(row,"Visible on map",0,0,120,muted);lv_obj_set_flex_grow(label,1);auto *toggle=lv_switch_create(row);lv_obj_set_size(toggle,44,24);if(style&&style->visible)lv_obj_add_state(toggle,LV_STATE_CHECKED);
  lv_obj_add_event_cb(toggle,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::SET_VISIBLE;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));c.value=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED);settings_send(c);},LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)id);
  auto *colors=lv_dropdown_create(dialog);lv_obj_set_width(colors,LV_PCT(100));lv_obj_set_height(colors,32);lv_dropdown_set_options(colors,"Orange\nBlue\nGreen\nRed\nPurple\nYellow");if(style)for(unsigned i=0;i<6;++i)if(style->color==mcxnav::TRACK_COLORS[i])lv_dropdown_set_selected(colors,i);
  lv_obj_add_event_cb(colors,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::SET_COLOR;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));c.value=mcxnav::TRACK_COLORS[lv_dropdown_get_selected(lv_event_get_target(e))%6];settings_send(c);},LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)id);
  row=control_row();control_button(row,"RENAME",panel2,[](lv_event_t *e){const auto id=lv_event_get_user_data(e);lv_async_call([](void *id){name_form(uint32_t(uintptr_t(id)),false);},id);},id);
  const bool active=id==navui::nav_state->current&&(navui::nav_state->mode==mcxnav::RECORDING||navui::nav_state->mode==mcxnav::PAUSED);
  auto *remove=control_button(row,"DELETE",panel2,[](lv_event_t *e){lv_async_call([](void *id){confirm_delete(uint32_t(uintptr_t(id)));},lv_event_get_user_data(e));},id);if(active)lv_obj_add_state(remove,LV_STATE_DISABLED);
  row=control_row();control_button(row,"CENTER",panel2,[](lv_event_t *e){const auto id=uint32_t(uintptr_t(lv_event_get_user_data(e)));if(navui::nav_state)for(unsigned i=0;i<navui::nav_state->track_count;++i)if(navui::nav_state->tracks[i].id==id&&navui::nav_state->tracks[i].points){show_location(navui::nav_state->tracks[i].first);break;}},id);
  auto *export_button=control_button(row,"EXPORT GPX",panel2,[](lv_event_t *e){mcxnav::Command c;c.action=mcxnav::EXPORT_TRACK;c.id=uint32_t(uintptr_t(lv_event_get_user_data(e)));settings_send(c);},id);if(active)lv_obj_add_state(export_button,LV_STATE_DISABLED);
  row=control_row();control_button(row,"CUSTOM RGB",panel2,[](lv_event_t *e){start_editing=false;setting_track=uint32_t(uintptr_t(lv_event_get_user_data(e)));lv_async_call([](void *){setting_editor(3);},nullptr);},id);
  nav_detail=single_label(dialog,"",0,0,lv_obj_get_width(body)-16,red);lv_label_set_long_mode(nav_detail,LV_LABEL_LONG_WRAP);lv_obj_set_height(nav_detail,LV_SIZE_CONTENT);lv_obj_add_flag(nav_detail,LV_OBJ_FLAG_HIDDEN);
}
static void library(bool pois){
  if(!pois){track_folders();return;}
  base(pois?"SAVED POIs":"SAVED TRACKS");mcxnav::start();navui::poll();
  if(pois)action_button(dialog,"SAVE CURRENT GPS AS POI",8,40,440,36,orange,[](lv_event_t *){lv_async_call([](void *){new_poi();},nullptr);},0);
  auto *list=panel_at(dialog,8,pois?82:44,440,pois?170:208,bg,0);lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(list,LV_DIR_VER);
  const auto *s=navui::nav_state;const unsigned n=s?(pois?s->poi_count:s->track_count):0;
  if(!n)single_label(list,"No saved items yet / SD worker may be loading",4,8,425,muted);
  for(unsigned i=0;i<n;++i){char title[100];uint32_t id;
    if(pois){id=s->pois[i].id;snprintf(title,sizeof(title),"%s",s->pois[i].name);}
    else {const auto &t=s->tracks[i];id=t.id;snprintf(title,sizeof(title),"%s / %.2f km",t.name[0]?t.name:"Track",t.metres/1000);}
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
