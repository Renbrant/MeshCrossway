#pragma once
// Included inside meshcrossway; all widgets are owned by the LVGL task.
namespace navui {
static lv_obj_t *canvas=nullptr,*picture=nullptr,*marker=nullptr,*map_status=nullptr,*attribution=nullptr;
static mcxmap::Frame *front=nullptr;
static bool following=true,inflight=false,dragging=false,centered=false;
static double latitude=0,longitude=0;
static unsigned zoom=14;
static uint32_t generation=0,requested_at=0,completed_at=0,seen_config=0;
static lv_point_t drag_start={};
static int drag_x=0,drag_y=0;
static void invalidate(){
  canvas=picture=marker=map_status=attribution=nullptr;inflight=false;dragging=false;mcxmap::stop();
}
static void after_clean(){if(front){lv_img_cache_invalidate_src(&front->image);mcxmap::release(front);front=nullptr;}}
static void request(){
  if(!canvas||!centered)return;
  mcxmap::View v;v.latitude=latitude;v.longitude=longitude;v.zoom=zoom;v.width=unsigned(lv_obj_get_width(canvas));v.height=unsigned(lv_obj_get_height(canvas));
  seen_config=mcxmap::configuration_revision();
  generation=mcxmap::request(v);inflight=generation!=0;requested_at=uint32_t(esp_timer_get_time()/1000);
  if(map_status)chatview::label(map_status,generation?"Loading map...":"Map unavailable / memory");
}
static void pointer(lv_event_t *event){
  if(!centered)return;
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
static void zoom_event(lv_event_t *event){
  const int delta=int(intptr_t(lv_event_get_user_data(event)));const int next=int(zoom)+delta;if(next<3||next>19)return;
  zoom=unsigned(next);auto c=mcxmap::configuration();c.zoom=uint8_t(zoom);mcxmap::configure(c);request();
}
static void recenter(lv_event_t *){following=true;const auto f=mcxgnss::snapshot();if(f.valid){latitude=f.latitude;longitude=f.longitude;centered=true;request();}else if(map_status)chatview::label(map_status,"Waiting for a fresh GPS fix");}
static void options(lv_event_t *);
static void create(int w){
  mcxmap::load_config();zoom=mcxmap::configuration().zoom;
  const auto f=mcxgnss::snapshot();if(f.valid){latitude=f.latitude;longitude=f.longitude;centered=true;}
  lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  canvas=panel_at(body,8,56,w-16,155,lv_color_hex(0x1A2027),0);
  lv_obj_add_flag(canvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_add_event_cb(canvas,pointer,LV_EVENT_ALL,nullptr);
  picture=lv_img_create(canvas);lv_obj_clear_flag(picture,LV_OBJ_FLAG_CLICKABLE);
  if(front){lv_img_cache_invalidate_src(&front->image);mcxmap::release(front);front=nullptr;}
  marker=panel_at(canvas,0,0,12,12,orange,6);lv_obj_set_style_border_width(marker,2,0);lv_obj_set_style_border_color(marker,white,0);lv_obj_clear_flag(marker,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
  attribution=single_label(canvas,mcxmap::configuration().attribution,3,0,w-22,white);
  lv_obj_set_style_text_font(attribution,&lv_font_montserrat_10,0);lv_label_set_long_mode(attribution,LV_LABEL_LONG_WRAP);
  lv_obj_set_style_bg_color(attribution,bg,0);lv_obj_set_style_bg_opa(attribution,LV_OPA_90,0);
  lv_obj_update_layout(attribution);lv_obj_set_y(attribution,155-lv_obj_get_height(attribution)-2);lv_obj_clear_flag(attribution,LV_OBJ_FLAG_CLICKABLE);
  map_status=single_label(body,"Waiting for GPS / no invented position",8,214,w-16,muted);lv_obj_set_style_text_font(map_status,&lv_font_montserrat_10,0);
  action_button(body,"-",8,251,32,29,panel2,zoom_event,-1);action_button(body,"+",44,251,32,29,panel2,zoom_event,1);
  action_button(body,"GPS",80,251,66,29,panel2,recenter,0);action_button(body,"MAP OPTIONS",152,251,w-160,29,panel2,options,0);
  mcxmap::start();if(centered)request();
}
static void poll(){
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(auto *next=mcxmap::take()){
    inflight=false;completed_at=now;
    if(canvas&&next->view.generation==generation){
      auto *old=front;front=next;lv_img_set_src(picture,&front->image);lv_obj_clear_flag(picture,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(picture,dragging?drag_x:0,dragging?drag_y:0);
      if(old){lv_img_cache_invalidate_src(&old->image);mcxmap::release(old);}
      chatview::label(map_status,front->status);
    }else mcxmap::release(next);
  }
  if(!canvas||info_dialog)return;
  if(seen_config!=mcxmap::configuration_revision()){
    seen_config=mcxmap::configuration_revision();const auto c=mcxmap::configuration();zoom=c.zoom;
    if(front&&front->source!=mcxmap::source_hash(c))lv_obj_add_flag(picture,LV_OBJ_FLAG_HIDDEN);
    chatview::label(attribution,c.attribution);lv_obj_update_layout(attribution);lv_obj_set_y(attribution,155-lv_obj_get_height(attribution)-2);
    if(centered)request();
  }
  if(inflight&&uint32_t(now-requested_at)>30000){inflight=false;chatview::label(map_status,"Map timeout / check SD or Wi-Fi");}
  const auto f=mcxgnss::snapshot();
  if(f.valid&&following&&!dragging&&!inflight){
    const auto old=mcxmap::project(latitude,longitude,zoom),pos=mcxmap::project(f.latitude,f.longitude,zoom);
    if(!centered||fabs(mcxmap::delta(pos.x,old.x,zoom))+fabs(pos.y-old.y)>16){latitude=f.latitude;longitude=f.longitude;centered=true;request();}
  }
  if(!inflight&&centered&&(!front||front->missing)&&uint32_t(now-completed_at)>4000){completed_at=now;request();}
  if(f.valid&&front&&!dragging){
    const auto origin=mcxmap::project(front->view.latitude,front->view.longitude,front->view.zoom),pos=mcxmap::project(f.latitude,f.longitude,front->view.zoom);
    const int x=int(lround(mcxmap::delta(pos.x,origin.x,front->view.zoom)+double(front->view.width)/2));const int y=int(lround(pos.y-origin.y+double(front->view.height)/2));
    if(x>=0&&x<int(front->view.width)&&y>=0&&y<int(front->view.height)){lv_obj_clear_flag(marker,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(marker,x-6,y-6);}
    else lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
  }else lv_obj_add_flag(marker,LV_OBJ_FLAG_HIDDEN);
}
}
namespace gpsui {
static lv_obj_t *dialog=nullptr,*status_label=nullptr,*metrics_label=nullptr,*enabled_button=nullptr,*baud_button=nullptr,*timeout_button=nullptr,*units_button=nullptr;
static lv_obj_t *map_save_label=nullptr;
static lv_obj_t *edit_value=nullptr,*edit_keyboard=nullptr,*edit_notice=nullptr;
static int editing=0;
static void open();
static void map_options();
static void refresh(){
  if(dialog!=info_dialog||!dialog){dialog=nullptr;return;}
  if(map_save_label)chatview::label(map_save_label,mcxmap::settings_saved.load()?"Settings saved / visible tiles only":"Settings pending / save failed");
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
  info_close_now(nullptr);dialog=info_dialog=panel_at(lv_layer_top(),10,26,460,286,bg,0);
  single_label(dialog,title,8,8,330,orange);action_button(dialog,"CLOSE",358,2,96,32,panel2,info_close,0);
  map_save_label=nullptr;
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
static void open(){
  base("LOCAL GPS / SETTINGS");
  enabled_button=action_button(dialog,"",8,42,216,34,panel2,change,0);baud_button=action_button(dialog,"",232,42,216,34,panel2,change,1);
  timeout_button=action_button(dialog,"",8,82,216,34,panel2,change,2);units_button=action_button(dialog,"",232,82,216,34,panel2,change,3);
  status_label=single_label(dialog,"",8,126,440,white);lv_label_set_long_mode(status_label,LV_LABEL_LONG_WRAP);
  metrics_label=single_label(dialog,"",8,177,440,cyan);
  single_label(dialog,"RX44 / TX43 fixed. Reading OFF does not power off GPS.",8,216,440,muted);
  single_label(dialog,"Host UART only; receiver configuration unchanged.",8,244,440,muted);refresh();
}
static void online(lv_event_t *){auto c=mcxmap::configuration();c.online=!c.online;mcxmap::configure(c);map_options();}
static void save_editor(lv_event_t *){
  auto c=mcxmap::configuration();const char *value=lv_textarea_get_text(edit_value);
  const size_t cap=editing==1?sizeof(c.url):sizeof(c.attribution);
  if(strlen(value)>=cap){lv_label_set_text(edit_notice,"Text is too long in UTF-8 bytes. Shorten it before saving.");return;}
  if(editing==1)snprintf(c.url,sizeof(c.url),"%s",value);else snprintf(c.attribution,sizeof(c.attribution),"%s",value);
  if(!mcxmap::valid_config(c)){lv_label_set_text(edit_notice,"Use an HTTPS tile root without query or credentials, and attribution.");return;}
  mcxmap::configure(c);map_options();
}
static void edit(int id){editing=id;const auto c=mcxmap::configuration();
  base(editing==1?"XYZ TILE ROOT URL":"MAP ATTRIBUTION");
  edit_value=lv_textarea_create(dialog);lv_obj_set_pos(edit_value,6,40);lv_obj_set_size(edit_value,448,42);lv_textarea_set_one_line(edit_value,true);
  lv_textarea_set_max_length(edit_value,editing==1?191:127);lv_textarea_set_text(edit_value,editing==1?c.url:c.attribution);
  edit_keyboard=lv_keyboard_create(dialog);lv_obj_set_size(edit_keyboard,448,145);lv_obj_set_pos(edit_keyboard,6,120);lv_obj_set_align(edit_keyboard,LV_ALIGN_TOP_LEFT);lv_keyboard_set_textarea(edit_keyboard,edit_value);
  action_button(dialog,"SAVE",350,84,98,32,orange,[](lv_event_t *){lv_async_call([](void *){save_editor(nullptr);},nullptr);},0);
  edit_notice=single_label(dialog,"",8,88,330,red);lv_label_set_long_mode(edit_notice,LV_LABEL_LONG_WRAP);
}
static void map_options(){
  base("MAP / OPTIONS");mcxmap::start();const auto c=mcxmap::configuration();
  action_button(dialog,c.online?"Online tiles: ON":"Online tiles: OFF",8,42,440,34,panel2,[](lv_event_t *){lv_async_call([](void *){online(nullptr);},nullptr);},0);
  action_button(dialog,"CHANGE HTTPS TILE SOURCE",8,82,440,34,panel2,[](lv_event_t *e){const auto value=lv_event_get_user_data(e);lv_async_call([](void *v){edit(int(intptr_t(v)));},value);},1);
  action_button(dialog,"CHANGE ATTRIBUTION",8,122,440,34,panel2,[](lv_event_t *e){const auto value=lv_event_get_user_data(e);lv_async_call([](void *v){edit(int(intptr_t(v)));},value);},2);
  auto *source=single_label(dialog,c.url,8,163,440,white);lv_obj_set_height(source,34);lv_label_set_long_mode(source,LV_LABEL_LONG_DOT);
  single_label(dialog,"Offline PNG XYZ: /tiles/z/x/y.png on microSD.",8,206,440,muted);
  single_label(dialog,"Online mode sends the viewed area to the tile provider.",8,234,440,muted);
  map_save_label=single_label(dialog,"",8,258,440,muted);refresh();
}
}
namespace navui {static void options(lv_event_t *){lv_async_call([](void *){gpsui::map_options();},nullptr);}}
static void gps_settings_open(){gpsui::open();}
