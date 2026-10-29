#pragma once
// Lightweight crash breadcrumbs, heap allocation failures and UI latency.
// No user content is retained. RTC data survives warm resets without flash writes.
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
namespace mcxruntime {
enum Stage : uint32_t { IDLE, PROTOCOL, HISTORY, DRAW, CAPTURE };
struct BootRecord {uint32_t magic,boots,stage;};
static RTC_NOINIT_ATTR BootRecord retained;
static std::atomic<uint32_t> boots{1},previous_stage{0},ui_max_ms{0},ui_stack_min{UINT32_MAX};
static std::atomic<uint32_t> allocation_failures{0},last_allocation_bytes{0},last_allocation_caps{0};
static const char *stage_name(uint32_t stage){
  switch(stage){case PROTOCOL:return "protocol";case HISTORY:return "history";
    case DRAW:return "draw";case CAPTURE:return "capture";default:return "idle";}
}
static void allocation_failed(size_t bytes,uint32_t caps,const char *){
  allocation_failures.fetch_add(1);last_allocation_bytes.store(bytes);last_allocation_caps.store(caps);
}
static void begin(){
  const bool warm=esp_reset_reason()!=ESP_RST_POWERON&&retained.magic==0x4d435852;
  boots.store(warm?retained.boots+1:1);previous_stage.store(warm?retained.stage:0);
  retained.magic=0x4d435852;retained.boots=boots.load();retained.stage=IDLE;
  (void)heap_caps_register_failed_alloc_callback(allocation_failed);
}
static void sample_ui_stack(){
  const uint32_t bytes=uxTaskGetStackHighWaterMark(nullptr);
  if(bytes<ui_stack_min.load())ui_stack_min.store(bytes);
}
struct Scope {
  uint32_t previous;int64_t started;
  explicit Scope(Stage stage):previous(retained.stage),started(esp_timer_get_time()){retained.stage=stage;}
  ~Scope(){
    const uint32_t ms=(esp_timer_get_time()-started)/1000;
    if(ms>ui_max_ms.load())ui_max_ms.store(ms);
    retained.stage=previous;
  }
};
}
