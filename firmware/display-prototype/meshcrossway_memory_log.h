#pragma once
// Diagnostic build only. No extra task, no full heap walks, no internal fallback.
#ifndef MCX_MEMORY_DIAGNOSTICS
#define MCX_MEMORY_DIAGNOSTICS 0
#endif
namespace mcxmem {
enum Task { UI, HISTORY_TASK, NAV, GNSS, BLE_COORD, BLE_HOST, MAP, HTTP, TOUCH, TASK_COUNT };
}
#if MCX_MEMORY_DIAGNOSTICS
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <stdio.h>
#ifndef MCX_MEMLOG_WRITE
#define MCX_MEMLOG_WRITE(data, size) fwrite(data, 1, size, stdout)
#endif
namespace mcxmem {
static constexpr unsigned CAPACITY=32;
static constexpr uint32_t SAMPLE_MS=30000, FLUSH_MS=1000;
struct Row {uint32_t boot,seq,ms,event,detail,internal,dma,psram,failures,failed_bytes,failed_caps,sample_us,dropped,output_errors,stack[TASK_COUNT];};
static std::atomic<Row *> rows{nullptr};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static unsigned head=0,count=0;
static uint32_t boot=0,sequence=0,dropped=0;
static std::atomic<uint32_t> stacks[TASK_COUNT],next_stack[TASK_COUNT];
static std::atomic<uint32_t> failures{0},failed_bytes{0},failed_caps{0},write_errors{0};
static uint32_t last_sample=0,last_flush=0;
// Callback-safe reserve: never allocate, log, walk heaps/stacks or retain task pointers.
struct AllocationFailure {uint32_t seq,ms,bytes,caps,core,isr;char task[16],function[32];};
static constexpr unsigned FAILURE_CAPACITY=4;
static DRAM_ATTR AllocationFailure failure_rows[FAILURE_CAPACITY];
static DRAM_ATTR portMUX_TYPE failure_mux=portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR unsigned failure_head=0,failure_count=0,failure_dropped=0;
static_assert(sizeof(failure_rows)==288,"Keep failure reserve small and fixed");
static void failure_name(char *out,unsigned size,const char *in){
  unsigned i=0;if(!in)in="unknown";
  for(;i+1<size&&in[i];++i){const unsigned char c=in[i];out[i]=(c>=33&&c<=126&&c!=',')?char(c):'_';}
  out[i]=0;
}
// Event codes: 0 periodic, 1 logger ready, 2 controller before, 3 controller after,
// 4 Wi-Fi before, 5 Wi-Fi after, 6 HTTP after, 7 NimBLE before, 8 NimBLE after,
// 9 page changed (detail=page), 10 map render before, 11 map render after.
static void capture(unsigned event=0,unsigned detail=0){
  auto *p=rows.load();if(!p)return;
  const int64_t started=esp_timer_get_time();Row r={};
  r.boot=boot;r.ms=uint32_t(started/1000);r.event=event;r.detail=detail;
  r.internal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  r.dma=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT);
  r.psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  r.failures=failures.load();r.failed_bytes=failed_bytes.load();r.failed_caps=failed_caps.load();
  for(unsigned i=0;i<TASK_COUNT;++i)r.stack[i]=stacks[i].load();
  r.sample_us=uint32_t(esp_timer_get_time()-started);
  portENTER_CRITICAL(&mux);
  if(count==CAPACITY){head=(head+1)%CAPACITY;--count;++dropped;}
  r.seq=++sequence;r.dropped=dropped;r.output_errors=write_errors.load();p[(head+count)%CAPACITY]=r;++count;
  portEXIT_CRITICAL(&mux);
}
static void begin(){
  if(rows.load())return;
  auto *p=(Row *)heap_caps_calloc(CAPACITY,sizeof(Row),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!p){printf("[MCX-MEMLOG] disabled: PSRAM allocation unavailable\n");return;}
  for(unsigned i=0;i<TASK_COUNT;++i)stacks[i].store(UINT32_MAX);
  boot=esp_random();rows.store(p);capture(1,esp_reset_reason());
  printf("[MCX-MEMLOG] v0.10.4 ring=%u bytes PSRAM; sample=30s output=console\n",unsigned(CAPACITY*sizeof(Row)));
}
static void allocation_failed(size_t bytes,uint32_t caps,const char *function="unknown"){
  AllocationFailure r={};r.seq=failures.fetch_add(1)+1;
  failed_bytes.store(bytes);failed_caps.store(caps);
  r.ms=uint32_t(esp_timer_get_time()/1000);r.bytes=uint32_t(bytes);r.caps=caps;
  r.core=uint32_t(xPortGetCoreID());r.isr=bool(xPortInIsrContext());
  const char *task="ISR";
  if(!r.isr){const auto handle=xTaskGetCurrentTaskHandle();task=handle?pcTaskGetName(handle):"startup";}
  failure_name(r.task,sizeof(r.task),task);failure_name(r.function,sizeof(r.function),function);
  portENTER_CRITICAL_SAFE(&failure_mux);
  if(failure_count==FAILURE_CAPACITY){failure_head=(failure_head+1)%FAILURE_CAPACITY;--failure_count;++failure_dropped;}
  failure_rows[(failure_head+failure_count)%FAILURE_CAPACITY]=r;++failure_count;
  portEXIT_CRITICAL_SAFE(&failure_mux);
}
// Existing coordinator only. Console I/O happens after releasing the callback lock.
static __attribute__((noinline)) void flush_failures(){
  for(unsigned n=0;n<2;++n){
    AllocationFailure r={};unsigned lost;
    portENTER_CRITICAL_SAFE(&failure_mux);
    const bool found=failure_count!=0;if(found)r=failure_rows[failure_head];lost=failure_dropped;
    portEXIT_CRITICAL_SAFE(&failure_mux);if(!found)return;
    char line[320];
    const int used=snprintf(line,sizeof(line),"[MCX-ALLOC] v0.10.4,boot=%u,seq=%u,uptime_ms=%u,bytes=%u,caps=0x%x,core=%u,isr=%u,task=%s,function=%s,dropped=%u\n",unsigned(boot),unsigned(r.seq),unsigned(r.ms),unsigned(r.bytes),unsigned(r.caps),unsigned(r.core),unsigned(r.isr),r.task,r.function,lost);
    if(used<0||unsigned(used)>=sizeof(line)||MCX_MEMLOG_WRITE(line,unsigned(used))!=unsigned(used)){++write_errors;return;}
    portENTER_CRITICAL_SAFE(&failure_mux);
    if(failure_count&&failure_rows[failure_head].seq==r.seq){failure_head=(failure_head+1)%FAILURE_CAPACITY;--failure_count;}
    portEXIT_CRITICAL_SAFE(&failure_mux);
  }
}
static void stack(Task id,uint32_t bytes){
  uint32_t before=stacks[id].load();while(bytes<before&&!stacks[id].compare_exchange_weak(before,bytes)){}
}
static void observe(Task id){
  if(!rows.load())return;
  const uint32_t now=uint32_t(esp_timer_get_time()/1000),next=next_stack[id].load();
  if(int32_t(now-next)<0)return;
  next_stack[id].store(now+SAMPLE_MS);stack(id,uxTaskGetStackHighWaterMark(nullptr));
}
// Coordinator task only. One bounded write per row; no console I/O in callbacks.
static __attribute__((noinline)) void flush(){
  auto *p=rows.load();if(!p)return;
  for(unsigned n=0;n<4;++n){
    Row r={};
    portENTER_CRITICAL(&mux);
    const bool found=count!=0;if(found)r=p[head];
    portEXIT_CRITICAL(&mux);if(!found)return;
    char line[384];
    int used=snprintf(line,sizeof(line),"[MCX-MEMLOG] v0.10.4,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",unsigned(r.boot),unsigned(r.seq),unsigned(r.ms),unsigned(r.event),unsigned(r.detail),unsigned(r.internal),unsigned(r.dma),unsigned(r.psram),unsigned(r.failures),unsigned(r.failed_bytes),unsigned(r.failed_caps),unsigned(r.sample_us),unsigned(r.dropped),unsigned(r.output_errors));
    if(used<0||unsigned(used)>=sizeof(line)){++write_errors;return;}
    for(unsigned i=0;i<TASK_COUNT;++i){
      const int added=snprintf(line+used,sizeof(line)-unsigned(used),",%u",unsigned(r.stack[i]));
      if(added<0||unsigned(added)>=sizeof(line)-unsigned(used)){++write_errors;return;}
      used+=added;
    }
    if(unsigned(used)+1>=sizeof(line)){++write_errors;return;}
    line[used++]='\n';
    if(MCX_MEMLOG_WRITE(line,unsigned(used))!=unsigned(used)){++write_errors;return;}
    portENTER_CRITICAL(&mux);
    // Producers may have overwritten this record while the console was blocked.
    if(count&&p[head].seq==r.seq){head=(head+1)%CAPACITY;--count;}
    portEXIT_CRITICAL(&mux);
  }
}
static void poll(){
  if(!rows.load())return;
  observe(BLE_COORD);
  const uint32_t now=uint32_t(esp_timer_get_time()/1000);
  if(uint32_t(now-last_sample)>=SAMPLE_MS){last_sample=now;capture();}
  if(uint32_t(now-last_flush)>=FLUSH_MS){last_flush=now;flush_failures();flush();}
}
static void page(unsigned value){static unsigned previous=UINT32_MAX;if(value!=previous){previous=value;capture(9,value);}}
}
#else
#include <stddef.h>
#include <stdint.h>
namespace mcxmem {
static inline void begin(){}
static inline void capture(unsigned=0,unsigned=0){}
static inline void allocation_failed(size_t,uint32_t,const char * = "unknown"){}
static inline void stack(Task,uint32_t){}
static inline void observe(Task){}
static inline void poll(){}
static inline void page(unsigned){}
}
#endif
