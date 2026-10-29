#pragma once
#include "meshcrossway_touch_queue.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
namespace mcxtouch {
static Queue<> queue;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static esp_lcd_touch_handle_t hardware=nullptr;
static lv_indev_t *device=nullptr;
static TaskHandle_t task=nullptr;
static Sample delivered;
static uint32_t serial=0,errors=0,max_gap_ms=0,last_activity=0;
static void (*on_sample)(bool,int,int,uint32_t,bool)=nullptr;
static void (*old_feedback)(lv_indev_drv_t *,uint8_t)=nullptr;
static uint32_t notified=UINT32_MAX;
static void notify_sample(){
  if(notified==serial||!serial)return;
  notified=serial;lv_point_t point={};lv_indev_get_point(device,&point);
  if(on_sample)on_sample(delivered.down,point.x,point.y,delivered.ms,delivered.cancel);
}
static bool active(){return task!=nullptr;}
static bool busy(){
  portENTER_CRITICAL(&mux);const bool result=queue.busy();const uint32_t at=last_activity;portEXIT_CRITICAL(&mux);
  return result||uint32_t(uint32_t(esp_timer_get_time()/1000)-at)<120;
}
static void feedback(lv_indev_drv_t *driver,uint8_t event){
  // Feedback runs after LVGL applies display rotation, before widget callbacks.
  if(notified!=serial&&(event==LV_EVENT_PRESSED||event==LV_EVENT_PRESSING||event==LV_EVENT_RELEASED)){
    notify_sample();
  }
  if(old_feedback)old_feedback(driver,event);
}
static void read(lv_indev_drv_t *,lv_indev_data_t *data){
  // A scroll can consume RELEASED without a widget release event. At the next
  // read the previous sample has completed pointer processing, so flush it here.
  notify_sample();
  Sample next;
  portENTER_CRITICAL(&mux);
  const bool got=queue.pop(next);data->continue_reading=queue.count!=0;
  portEXIT_CRITICAL(&mux);
  if(got){delivered=next;++serial;
    if(next.cancel){lv_indev_reset(device,nullptr);lv_indev_wait_release(device);
      if(on_sample)on_sample(false,0,0,next.ms,true);notified=serial;
    }
  }
  data->point.x=delivered.x;data->point.y=delivered.y;
  data->state=delivered.down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void sample_task(void *){
  uint32_t previous=0,reported=0;unsigned failures=0;
  for(;;){
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    const uint32_t gap=previous?now-previous:0;previous=now;
    // Only this task accesses the touch controller after attach().
    const esp_err_t result=esp_lcd_touch_read_data(hardware);
    uint16_t x=0,y=0,strength=0;uint8_t points=0;
    const bool down=result==ESP_OK&&esp_lcd_touch_get_coordinates(hardware,&x,&y,&strength,&points,1);
    portENTER_CRITICAL(&mux);
    if(gap>max_gap_ms)max_gap_ms=gap;
    if(result!=ESP_OK){++errors;if(++failures==3){queue.cancel(now);last_activity=now;}}
    else {failures=0;
      if(gap>250&&queue.last.down)queue.cancel(now);
      Sample s;s.x=down?x:queue.last.x;s.y=down?y:queue.last.y;s.ms=now;s.down=down;
      if(down||queue.last.down)last_activity=now;
      queue.push(s);
    }
    portEXIT_CRITICAL(&mux);
    if(uint32_t(now-reported)>=30000){
      reported=now;
      portENTER_CRITICAL(&mux);
      const unsigned err=errors,gap_ms=max_gap_ms,peak=queue.peak,over=queue.overflows;
      portEXIT_CRITICAL(&mux);
      printf("[MCX-TOUCH] errors=%u max_gap_ms=%u queue_peak=%u overflows=%u stack_free=%u\n",
        err,gap_ms,peak,over,unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    }
    // No catch-up loop after a slow I2C transaction.
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
// Called with the LVGL lock, after the SDK registers its one pointer device.
// Preserve SDK user_data and feedback ownership; fall back on allocation failure.
static bool attach(lv_indev_t *input,esp_lcd_touch_handle_t touch){
  if(active())return device==input;
  if(!input||!touch)return false;
  hardware=touch;device=input;
  if(xTaskCreatePinnedToCore(sample_task,"mcx_touch",3072,nullptr,2,&task,0)!=pdPASS){
    task=nullptr;printf("[MCX-TOUCH] sampler unavailable; SDK polling retained\n");return false;
  }
  old_feedback=input->driver->feedback_cb;
  input->driver->read_cb=read;input->driver->feedback_cb=feedback;
  printf("[MCX-TOUCH] buffered 10ms input; internal stack=3072 bytes\n");return true;
}
}
