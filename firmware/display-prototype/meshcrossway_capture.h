#pragma once
// v0.5.10 experiment C: single-shot, read-only LVGL capture. No HTTP-thread LVGL calls.
// Never register a second touch input or enable the legacy remote server.
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if LVGL_VERSION_MAJOR != 8 || !LV_USE_SNAPSHOT || LV_COLOR_DEPTH != 16
#error "MeshCrossway manual capture needs LVGL 8 snapshot with RGB565"
#endif

namespace meshcapture {
static constexpr size_t FRAME_BYTES=480u*320u*2u;
static constexpr int64_t MIN_INTERVAL_US=5000000;
enum State { IDLE=0, QUEUED=1, RENDERING=2, READY=3, FAILED=4, CANCELLED=5, SENDING=6 };
static std::atomic<int> phase{IDLE};
static uint8_t *frame=nullptr;
static SemaphoreHandle_t completion=nullptr;
static int64_t earliest_capture=0;  // HTTP server task only.
static std::atomic<uint32_t> captures{0},failures{0},last_capture_ms{0};
static std::atomic<bool> circuit_open{false};
static constexpr uint32_t MAX_CAPTURE_MS=2000; // Disable future captures if redraw stalls excessively.

static bool available(){return frame!=nullptr&&completion!=nullptr&&!circuit_open.load();}
static void capture_timer(lv_timer_t *){
  int expected=QUEUED;
  if(!phase.compare_exchange_strong(expected,RENDERING))return;
  const int64_t started=esp_timer_get_time();
  lv_obj_t *screen=lv_scr_act();
  lv_img_dsc_t descriptor={};
  // Fail closed if the rendered screen unexpectedly exceeds one RGB565 frame.
  const uint32_t needed=lv_snapshot_buf_size_needed(screen,LV_IMG_CF_TRUE_COLOR);
  const bool ok=available()&&needed==FRAME_BYTES&&
    lv_snapshot_take_to_buf(screen,LV_IMG_CF_TRUE_COLOR,&descriptor,frame,FRAME_BYTES)==LV_RES_OK&&
    descriptor.header.w==480&&descriptor.header.h==320;
  const uint32_t elapsed_ms=(uint32_t)((esp_timer_get_time()-started)/1000);
  last_capture_ms.store(elapsed_ms);
  if(elapsed_ms>MAX_CAPTURE_MS){
    circuit_open.store(true);
    printf("[MCX-CAPTURE] disabled: render took %lu ms\n",(unsigned long)elapsed_ms);
  }
  if(ok)captures.fetch_add(1);else failures.fetch_add(1);
  if(phase.load()==CANCELLED){phase.store(IDLE);return;}
  phase.store((ok&&!circuit_open.load())?READY:FAILED);
  xSemaphoreGive(completion);
}
static void init(){
  completion=xSemaphoreCreateBinary();
  frame=(uint8_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!available()){
    if(frame){heap_caps_free(frame);frame=nullptr;}
    if(completion){vSemaphoreDelete(completion);completion=nullptr;}
    printf("[MCX-CAPTURE] disabled; PSRAM or semaphore unavailable\n");
    return;
  }
  // Called from meshcrossway::init() while the Waveshare LVGL lock is held.
  lv_timer_create(capture_timer,100,nullptr);
  printf("[MCX-CAPTURE] manual-only ready; frame=%u bytes touch=OFF\n",(unsigned)FRAME_BYTES);
}
static esp_err_t reject(httpd_req_t *req,const char *status,const char *message){
  httpd_resp_set_status(req,status);
  httpd_resp_set_type(req,"text/plain; charset=utf-8");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  return httpd_resp_send(req,message,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t screenshot(httpd_req_t *req){
  if(!available())return reject(req,"503 Service Unavailable","Capture disabled or allocation unavailable");
  const int64_t now=esp_timer_get_time();
  if(now<earliest_capture)return reject(req,"429 Too Many Requests","Manual capture: wait 5 seconds");
  int idle=IDLE;
  (void)xSemaphoreTake(completion,0); // Drain before queueing a new request.
  if(!phase.compare_exchange_strong(idle,QUEUED))
    return reject(req,"503 Service Unavailable","Capture still busy");
  earliest_capture=now+MIN_INTERVAL_US;
  if(xSemaphoreTake(completion,pdMS_TO_TICKS(4000))!=pdTRUE){
    int queued=QUEUED;
    if(!phase.compare_exchange_strong(queued,IDLE)){
      int rendering=RENDERING;
      if(!phase.compare_exchange_strong(rendering,CANCELLED)){
        int ready=READY;
        if(!phase.compare_exchange_strong(ready,IDLE)){
          int failed=FAILED;
          (void)phase.compare_exchange_strong(failed,IDLE);
        }
      }
    }
    return reject(req,"503 Service Unavailable","Capture timed out");
  }
  int ready=READY;
  if(!phase.compare_exchange_strong(ready,SENDING)){
    phase.store(IDLE);
    return reject(req,"503 Service Unavailable","Capture failed");
  }
  httpd_resp_set_type(req,"application/octet-stream");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
#if LV_COLOR_16_SWAP
  httpd_resp_set_hdr(req,"X-MCX-RGB565-Swap","1");
#else
  httpd_resp_set_hdr(req,"X-MCX-RGB565-Swap","0");
#endif
  const esp_err_t sent=httpd_resp_send(req,(const char*)frame,FRAME_BYTES);
  phase.store(IDLE);
  return sent;
}
} // namespace meshcapture
