#pragma once
// LVGL owns model snapshots. A single low-priority worker owns ALL filesystem I/O.
#include "meshcrossway_history_store.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
namespace mcxhistory {
static constexpr const char *DIR="/sdcard/mcx-history";
static mcxchat::Local local;
static TaskHandle_t worker=nullptr;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
struct Job {mcxstore::State *image=nullptr;bool load=false,offline=false;uint8_t radio[32]={};uint32_t epoch=0;};
static Job jobs[4];static unsigned head=0,count=0;
static mcxstore::State *loaded=nullptr;
static uint32_t loaded_epoch=0;
static bool loaded_offline=false,load_ok=false,load_unavailable=false;
static std::atomic<bool> complete{false}, saving{false};
static std::atomic<int> storage{0}; // 0 pending, 1 saved, 2 unavailable/write error, 3 recovered, 4 no archive
static std::atomic<uint32_t> commits{0};
static bool offline_requested=false,restore_requested=false,restored=false,restore_unavailable=false;
static uint32_t last_restore_attempt=0;
static uint32_t bound_epoch=0,last_generation=UINT32_MAX,last_revision=UINT32_MAX,last_capture=0;
static uint8_t bound_radio[32]={};
static mcxstore::State *allocate(){
  void *p=heap_caps_malloc(sizeof(mcxstore::State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  return p?new(p) mcxstore::State():nullptr;
}
static bool submit(const Job &job){
  bool ok=false;portENTER_CRITICAL(&lock);
  if(count<4){jobs[(head+count)%4]=job;++count;ok=true;}
  portEXIT_CRITICAL(&lock);return ok;
}
static void worker_main(void *){
  auto *previous=allocate(),*staging=allocate();
  if(!previous||!staging){storage.store(2);vTaskDelete(nullptr);return;}
  Job retry;uint32_t retry_at=0;
  for(;;){
    Job job;bool have=false;
    portENTER_CRITICAL(&lock);
    if(count){job=jobs[head];head=(head+1)%4;--count;have=true;}
    portEXIT_CRITICAL(&lock);
    const uint32_t tick=xTaskGetTickCount();
    if(!have&&retry.image&&(int32_t)(tick-retry_at)>=0){job=retry;retry=Job{};have=true;}
    if(!have){vTaskDelay(pdMS_TO_TICKS(100));continue;}
    if(job.load){
      // Never overwrite a result that the UI has not consumed yet.
      while(complete.load())vTaskDelay(pdMS_TO_TICKS(50));
      bool identity=true;
      if(job.offline){FILE *f=fopen(DIR "/last-radio","rb");identity=f&&fread(job.radio,1,32,f)==32&&fgetc(f)==EOF;if(f)fclose(f);}
      auto *image=allocate();
      mcxstore::Recovery r;
      if(image&&identity)r=mcxstore::load(DIR,job.radio,*image,*staging);
      loaded=image;loaded_epoch=job.epoch;loaded_offline=job.offline;load_ok=r.found;
      if(r.found)storage.store(r.recovered?3:1);
      else {storage.store((mkdir(DIR,0755)==0||errno==EEXIST)?4:2);}
      load_unavailable=storage.load()==2;
      complete.store(true);continue;
    }
    saving.store(true);
    const bool ok=mcxstore::save(DIR,*job.image,*previous,*staging);
    if(ok){
      // last-radio is advisory only. Archives still require the full 32-byte identity.
      FILE *f=fopen(DIR "/last-radio.tmp","wb");
      if(f){bool wrote=fwrite(job.image->meta.radio,1,32,f)==32&&fflush(f)==0&&fsync(fileno(f))==0;
        if(fclose(f))wrote=false;if(wrote)rename(DIR "/last-radio.tmp",DIR "/last-radio");}
      storage.store(1);commits.fetch_add(1);
      if(retry.image&&!memcmp(retry.image->meta.radio,job.image->meta.radio,32)){
        heap_caps_free(retry.image);retry=Job{};
      }
      heap_caps_free(job.image);
    }else{
      storage.store(2);
      // Bound memory under SD failure; preserve newest same-radio snapshot for retry.
      if(retry.image)heap_caps_free(retry.image);
      retry=job;retry_at=tick+pdMS_TO_TICKS(5000);
    }
    saving.store(false);
  }
}
static void begin(){if(!worker)xTaskCreatePinnedToCore(worker_main,"mcx_history",6144,nullptr,1,&worker,0);}
static void capture(meshmc::Client &c,uint32_t now,bool force=false){
  if(!mcxstore::nonzero(c.public_key))return;
  if(!force&&((uint32_t)(now-last_capture)<1000||(c.generation==last_generation&&local.revision==last_revision)))return;
  auto *image=allocate();if(!image){storage.store(2);return;}
  mcxstore::capture(c,local,*image);Job j;j.image=image;
  if(submit(j)){last_generation=c.generation;last_revision=local.revision;last_capture=now;}
  else heap_caps_free(image);
}
static void before_attach(meshmc::Client &c,uint32_t now){
  // Save the previous identity before Client::attach clears its model.
  if(restored)capture(c,now,true);
  new (&local) mcxchat::Local();restored=restore_requested=restore_unavailable=false;
}
static const char *status(){
  if(saving.load()||storage.load()==0)return "SD: saving / pending";
  switch(storage.load()){case 1:return "SD: journal saved";case 2:return "SD unavailable / unsaved RAM";
    case 3:return "SD: recovered last commit";default:return "SD: no saved history";}
}
static void poll(meshmc::Client &c,uint32_t now,uint32_t epoch){
  begin();if(!worker)return;
  if(complete.load()){
    if(load_ok&&loaded){
      if(loaded_offline&&!c.connected){mcxstore::restore(*loaded,c,local,false);memcpy(bound_radio,c.public_key,32);restored=true;}
      else if(!loaded_offline&&epoch==loaded_epoch&&c.initialized&&!memcmp(loaded->meta.radio,c.public_key,32)){
        // Messages received while SD was absent are merged after the older journal.
        auto *recent=(meshmc::Message *)heap_caps_malloc(sizeof(c.messages),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(recent){
          memcpy(recent,c.messages,sizeof(c.messages));const uint32_t next=c.next_id;
          auto *edits=allocate();if(edits)edits->local=local;
          mcxstore::restore(*loaded,c,local,true);
          const uint32_t first=next>meshmc::MAX_MESSAGES?next-meshmc::MAX_MESSAGES:1;
          for(uint32_t id=first;id<next;++id){const auto &m=recent[(id-1)%meshmc::MAX_MESSAGES];if(m.id!=id)continue;
            bool seen=false;for(auto &old:c.messages)if(old.id&&old.channel==m.channel&&old.outgoing==m.outgoing&&
              old.timestamp==m.timestamp&&(m.channel?(old.slot==m.slot&&!memcmp(old.channel_identity,m.channel_identity,16)):!memcmp(old.key,m.key,6))&&!strcmp(old.text,m.text)){
                old.read=old.read||m.read;seen=true;break;}
            if(!seen){auto &dst=c.append();uint32_t saved=dst.id;dst=m;dst.id=saved;}
          }
          if(edits&&edits->local.revision){
            for(const auto &p:edits->local.threads)if(p.used){auto *dst=local.get(p.key,true);if(dst)*dst=p;}
            ++local.revision;
          }
          if(edits)heap_caps_free(edits);heap_caps_free(recent);restored=true;
        }else {storage.store(2);load_unavailable=true;}

      }
    }
    if(!loaded_offline&&epoch==loaded_epoch){restored=true;restore_unavailable=load_unavailable;last_generation=last_revision=UINT32_MAX;}
    if(loaded_offline&&load_unavailable){offline_requested=false;last_restore_attempt=now;}
    if(loaded)heap_caps_free(loaded);loaded=nullptr;complete.store(false);
  }
  if(c.connected&&c.initialized&&mcxstore::nonzero(c.public_key)){
    if(bound_epoch!=epoch||memcmp(bound_radio,c.public_key,32)){
      bound_epoch=epoch;memcpy(bound_radio,c.public_key,32);restore_requested=restored=false;
    }
    if(restore_unavailable&&(uint32_t)(now-last_restore_attempt)>=5000){restore_requested=false;last_restore_attempt=now;}
    if(!restore_requested){Job j;j.load=true;j.epoch=epoch;memcpy(j.radio,c.public_key,32);restore_requested=submit(j);}
  }else if(!c.connected&&!offline_requested&&!restored&&(uint32_t)(now-last_restore_attempt)>=5000){
    Job j;j.load=j.offline=true;offline_requested=submit(j);
  }
  if(restored&&!restore_unavailable)capture(c,now);
}
static bool ready_for(const meshmc::Client &c){return !c.initialized||restored;}
} // namespace mcxhistory
