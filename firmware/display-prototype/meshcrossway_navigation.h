#pragma once
#include "meshcrossway_navigation_store.h"
#include "meshcrossway_gnss.h"
#include "esp_heap_caps.h"
#include "esp_sdcard_port.h"
#include "esp_vfs_fat.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <new>
namespace mcxnav {

static Store *store=nullptr;static Snapshot *published=nullptr;static SemaphoreHandle_t guard=nullptr;static TaskHandle_t worker_task=nullptr;
static Command commands[4];static unsigned head=0,count=0;
static bool snapshot(Snapshot &out){if(!guard||xSemaphoreTake(guard,0)!=pdTRUE)return false;out=*published;xSemaphoreGive(guard);return true;}
static Sample position(const mcxgnss::Fix &f){Sample p;p.latitude=f.latitude;p.longitude=f.longitude;p.altitude=f.altitude;p.flags=f.have_altitude?1:0;p.epoch=f.have_utc?f.utc_epoch:(time(nullptr)>1577836800?int64_t(time(nullptr)):0);return p;}
static void publish(){xSemaphoreTake(guard,portMAX_DELAY);auto &o=*published;auto &s=*store;o.mode=s.mode;o.current=s.current;o.points=s.point_count;o.metres=s.metres;o.revision=s.revision;o.poi_count=s.poi_count;o.track_count=s.track_count;o.preview_count=s.preview_count;o.preview_id=s.preview_id;snprintf(o.status,sizeof(o.status),"%s",s.status);memcpy(o.pois,s.pois,sizeof(o.pois));memcpy(o.tracks,s.tracks,sizeof(o.tracks));memcpy(o.preview,s.preview,sizeof(o.preview));xSemaphoreGive(guard);}
static bool submit(Action action,uint32_t id=0,const char *name=nullptr){if(!guard||xSemaphoreTake(guard,0)!=pdTRUE)return false;if(count==4){xSemaphoreGive(guard);return false;}Command c;c.action=action;c.id=id;if(name&&strlen(name)>=sizeof(c.name)){xSemaphoreGive(guard);return false;}if(name)snprintf(c.name,sizeof(c.name),"%s",name);if(action==SAVE_POI){auto fix=mcxgnss::snapshot();if(!fix.valid){xSemaphoreGive(guard);return false;}c.point=position(fix);}commands[(head+count)%4]=c;++count;xSemaphoreGive(guard);return true;}
static void worker(void *){
 for(;;){
  if(!store->loaded&&esp_sdcard_port_get_size()){if(mkdir("/sdcard/meshcrossway",0755)==0||errno==EEXIST)store->load("/sdcard/meshcrossway/navigation");}
  Command c;xSemaphoreTake(guard,portMAX_DELAY);if(count){c=commands[head];head=(head+1)%4;--count;}xSemaphoreGive(guard);
  uint64_t total=0,free=0;const bool space=esp_sdcard_port_get_size()&&esp_vfs_fat_info("/sdcard",&total,&free)==ESP_OK&&free>8ULL*1024*1024;
  switch(c.action){
   case START:if(space)store->start();else {snprintf(store->status,sizeof(store->status),"Recording needs writable SD / 8 MiB free");++store->revision;}break;
   case PAUSE:store->pause();break;case RESUME:store->resume();break;case STOP:store->stop();break;
   case SAVE_POI:if(space)store->save_poi(c.point,c.name);else {snprintf(store->status,sizeof(store->status),"POI needs writable SD / 8 MiB free");++store->revision;}break;
   case DELETE_POI:store->delete_poi(c.id);break;
   case SHOW_TRACK:if(store->mode==RECORDING||store->mode==PAUSED){snprintf(store->status,sizeof(store->status),"Finish recording before viewing another track");++store->revision;}else {Track t;if(store->scan_track(c.id,t,true)){snprintf(store->status,sizeof(store->status),"Track %08lx / %lu points",(unsigned long)c.id,(unsigned long)t.points);++store->revision;}}break;
   default:break;
  }
  if(store->mode==RECORDING){const auto f=mcxgnss::snapshot();if(!space){store->pause();snprintf(store->status,sizeof(store->status),"Recording paused / SD unavailable or full");++store->revision;}else store->sample(position(f),f.valid,uint32_t(esp_timer_get_time()/1000));}
  publish();vTaskDelay(pdMS_TO_TICKS(500));
 }
}
static bool start(){if(worker_task)return true;if(!store){auto *p=heap_caps_malloc(sizeof(Store),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);auto *q=heap_caps_malloc(sizeof(Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p||!q){heap_caps_free(p);heap_caps_free(q);return false;}store=new(p) Store();published=new(q) Snapshot();guard=xSemaphoreCreateMutex();if(!guard){store->~Store();published->~Snapshot();heap_caps_free(store);heap_caps_free(published);store=nullptr;published=nullptr;return false;}}
 return xTaskCreatePinnedToCore(worker,"mcx_nav",4096,nullptr,1,&worker_task,0)==pdPASS;
}
}
