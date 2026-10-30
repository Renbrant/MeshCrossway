static bool navigation_panel=false;
// Compiled against real LVGL 8.4 and the production input adapter. Only the
// hardware/RTOS boundary is mocked; samples model physical acquisition times.
#include "lvgl.h"
#include "meshcrossway_touch_input.h"
#include "meshcrossway_edge_swipe.h"
#include <cassert>
#include <cstdio>
static bool left_compact=true,right_compact=true,suppress_left_nav_click=false;
static lv_obj_t *wifi_dialog=nullptr,*wifi_editor=nullptr,*ble_dialog=nullptr,*info_dialog=nullptr,*shade=nullptr;
static int page=3,clicks=0,feedbacks=0,draws=0;
static lv_obj_t *left=nullptr,*right=nullptr;
static bool has_left_rail(){return true;}
static bool has_context_rail(){return true;}
static void draw(){
  assert(mcxtouch::device->proc.state!=LV_INDEV_STATE_PRESSED);++draws;
  lv_obj_set_width(left,left_compact?34:73);
  lv_obj_set_x(right,right_compact?445:360);lv_obj_set_width(right,right_compact?31:115);
}
// PRODUCTION_EDGE_CODE
static bool touch_modal(){return info_dialog!=nullptr;}
static lv_color_t pixels[480*10];
static void flush(lv_disp_drv_t *d,const lv_area_t *,lv_color_t *){lv_disp_flush_ready(d);}
static void idle_read(lv_indev_drv_t *,lv_indev_data_t *d){d->state=LV_INDEV_STATE_RELEASED;}
static void previous_feedback(lv_indev_drv_t *,uint8_t){++feedbacks;}
static void advance(unsigned ms=200){for(unsigned i=0;i<ms;i+=5){lv_tick_inc(5);lv_timer_handler();}}
static void click(lv_event_t *){if(!suppress_left_nav_click&&!edge_swipe_click_in_progress())++clicks;}
static void captured(bool down,int x,int y,unsigned ms){
  mcxtouch::Sample s;s.down=down;s.x=x;s.y=y;s.ms=ms;mcxtouch::queue.push(s);
}
static void swipe(int x1,int y,int x2){
  const unsigned t=lv_tick_get();captured(true,x1,y,t);
  captured(true,(x1+x2)/2,y,t+30);captured(true,x2,y,t+50);captured(false,x2,y,t+60);
  // The owner task was blocked while all four samples were collected.
  lv_tick_inc(100);advance();
}
int main(){
  lv_init();lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,pixels,nullptr,480*10);
  lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=480;display.ver_res=320;
  display.flush_cb=flush;display.draw_buf=&buffer;lv_disp_drv_register(&display);
  lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;
  driver.read_cb=idle_read;driver.feedback_cb=previous_feedback;driver.user_data=(void *)0x1234;
  auto *input=lv_indev_drv_register(&driver);
  fake_task_failure=true;assert(!mcxtouch::attach(input,(void *)1));
  assert(input->driver->read_cb==idle_read&&input->driver->user_data==(void *)0x1234);
  fake_task_failure=false;assert(mcxtouch::attach(input,(void *)1));
  assert(input->driver->user_data==(void *)0x1234);
  mcxtouch::on_sample=buffered_swipe;
  auto make=[](int x,int y,int w){auto *b=lv_btn_create(lv_scr_act());lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,38);
    lv_obj_add_flag(b,LV_OBJ_FLAG_PRESS_LOCK);lv_obj_add_event_cb(b,click,LV_EVENT_CLICKED,nullptr);return b;};
  left=make(3,41,34);right=make(445,60,31);
  lv_timer_create(edge_swipe_poll,20,nullptr);advance();
  captured(true,20,60,lv_tick_get());captured(false,20,60,lv_tick_get()+60);
  lv_tick_inc(100);advance();assert(clicks==1&&feedbacks>0);advance();assert(clicks==1);
  clicks=0;swipe(20,60,100);assert(!left_compact&&clicks==0);
  swipe(65,60,10);assert(left_compact&&clicks==0);
  swipe(465,80,390);assert(!right_compact&&clicks==0);
  swipe(370,80,445);assert(right_compact&&clicks==0&&draws==4);
  // Cancel a delivered press as an I2C error/overflow would: no ghost click.
  captured(true,20,60,lv_tick_get());advance(40);
  mcxtouch::queue.cancel(lv_tick_get());captured(false,20,60,lv_tick_get()+10);
  advance();assert(clicks==0);
  info_dialog=left;swipe(20,60,100);assert(left_compact);info_dialog=nullptr;clicks=0;
  // Scrolling can consume UP without a widget RELEASED event. It must not
  // leave the classifier armed when the next rail gesture begins.
  auto *scroller=lv_obj_create(lv_scr_act());lv_obj_set_pos(scroller,120,40);lv_obj_set_size(scroller,220,260);
  auto *long_content=lv_obj_create(scroller);lv_obj_set_size(long_content,100,1200);advance();
  captured(true,210,220,lv_tick_get());captured(true,210,120,lv_tick_get()+30);
  captured(false,210,120,lv_tick_get()+60);lv_tick_inc(100);advance(500);
  assert(!swipe_state.down);swipe(20,60,100);assert(!left_compact&&clicks==0);

  puts("PASS: real LVGL retains 60ms tap across 100ms stall; all buffered rail directions, click suppression, cancel, modal gating, SDK ownership and task-allocation fallback");
}
