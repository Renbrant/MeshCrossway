#pragma once
// LVGL owns model snapshots. A single low-priority worker owns ALL filesystem I/O.
#include "meshcrossway_history_store.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
namespace mcxhistory {
static const char *DIR="/sdcard/mcx-history";
static std::atomic<mcxchat::Workspace *> workspace_memory{nullptr};
static mcxchat::Workspace *workspace(){return workspace_memory.load();}
static bool init_workspace(){
  if(workspace())return true;
  void *memory=heap_caps_malloc(sizeof(mcxchat::Workspace),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!memory)return false; // No fallback into the RAM required by Bluetooth/DMA.
  auto *created=new(memory) mcxchat::Workspace();
  mcxchat::Workspace *expected=nullptr;
  if(!workspace_memory.compare_exchange_strong(expected,created))heap_caps_free(created);
  return true;
}
static mcxchat::Local &preferences(){return workspace()->local;}
static TaskHandle_t worker=nullptr;
static std::atomic<bool> ble_startup{false};
// Optional second protocol work, executed by the SAME SD owner. No filesystem
// access or waiting is allowed in LVGL/NimBLE callbacks.
static std::atomic<void (*)()> extra_work{nullptr};
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
struct Job {mcxstore::State *image=nullptr;bool load=false,offline=false;uint8_t radio[32]={};uint32_t epoch=0,ticket=0;};
static Job jobs[4];static unsigned head=0,count=0;
static mcxstore::State *loaded=nullptr;
static uint32_t loaded_epoch=0;
static bool loaded_offline=false,load_ok=false,load_unavailable=false;
static std::atomic<bool> complete{false}, saving{false};
static std::atomic<int> storage{0}; // 0 pending, 1 saved, 2 unavailable/write error, 3 recovered, 4 no archive
static std::atomic<uint32_t> commits{0},submitted{0},committed{0};
static std::atomic<bool> dirty{false},worker_failed{false};
static std::atomic<int> error_step{0},error_number{0};
static std::atomic<uint32_t> failures{0},operation_ms{0},max_operation_ms{0};
static void record_duration(uint32_t started){
  const uint32_t ms=(xTaskGetTickCount()-started)*portTICK_PERIOD_MS;
  operation_ms.store(ms);if(ms>max_operation_ms.load())max_operation_ms.store(ms);
}
static void report_error(int step,int number){
  error_number.store(number);error_step.store(step);storage.store(2);failures.fetch_add(1);
  printf("[MCX-HISTORY] failure step=%s errno=%d (message contents omitted)\n",mcxstore::step_name(step),number);
}
static bool offline_requested=false,restore_requested=false,restored=false,restore_unavailable=false;
static uint32_t last_restore_attempt=0;
static uint32_t bound_epoch=0,last_generation=UINT32_MAX,last_revision=UINT32_MAX,last_capture=0;
static uint8_t bound_radio[32]={};
static mcxstore::State *allocate(){
  void *p=heap_caps_malloc(sizeof(mcxstore::State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  return p?new(p) mcxstore::State():nullptr;
}
static bool submit(const Job &job){
  bool ok=false;mcxstore::State *superseded=nullptr;portENTER_CRITICAL(&lock);
  // Coalesce pending captures for a radio. A load is an ordering fence.
  if(job.image&&count){auto &tail=jobs[(head+count-1)%4];
    if(tail.image&&!memcmp(tail.image->meta.radio,job.image->meta.radio,32)){
      superseded=tail.image;tail=job;tail.ticket=submitted.fetch_add(1)+1;ok=true;
    }
  }
  if(!ok&&count<4){jobs[(head+count)%4]=job;if(job.image)jobs[(head+count)%4].ticket=submitted.fetch_add(1)+1;++count;ok=true;}
  portEXIT_CRITICAL(&lock);if(superseded)heap_caps_free(superseded);return ok;
}
static bool read_identity(uint8_t *radio){
  char path[240];
  const char *names[]={"last-radio","last-radio.bak"};
  for(const char *name:names){
    snprintf(path,sizeof(path),"%s/%s",DIR,name);FILE *f=fopen(path,"rb");
    bool ok=f&&fread(radio,1,32,f)==32&&fgetc(f)==EOF;if(f)fclose(f);
    if(ok)return true;
  }
  return false;
}
static void remember_identity(const uint8_t *radio){
  uint8_t current[32];if(read_identity(current)&&!memcmp(current,radio,32))return;
  char file[240],temp[240],backup[240];
  snprintf(file,sizeof(file),"%s/last-radio",DIR);snprintf(temp,sizeof(temp),"%s/last-radio.tmp",DIR);
  snprintf(backup,sizeof(backup),"%s/last-radio.bak",DIR);
  FILE *f=fopen(temp,"wb");
  if(f){bool wrote=fwrite(radio,1,32,f)==32&&fflush(f)==0&&fsync(fileno(f))==0;
    if(fclose(f))wrote=false;
    if(wrote){remove(backup);rename(file,backup);rename(temp,file);}}
}
static void worker_main(void *){
  auto *previous=allocate(),*staging=allocate();
  if(!previous||!staging){if(previous)heap_caps_free(previous);if(staging)heap_caps_free(staging);report_error(mcxstore::MEMORY,ENOMEM);worker_failed.store(true);vTaskDelete(nullptr);return;}
  Job retry;uint32_t retry_at=0;
  mcxstore::Cursor cursor;
  for(;;){
    if(!ble_startup.load())if(auto work=extra_work.load())work();
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
      const uint32_t started=xTaskGetTickCount();
      bool identity=true;
      if(job.offline)identity=read_identity(job.radio);
      auto *image=allocate();
      mcxstore::Recovery r;
      // A failed write must not discard newer RAM messages on BLE reconnect.
      const bool unsaved=image&&retry.image&&identity&&!memcmp(retry.image->meta.radio,job.radio,32);
      if(image&&identity&&!unsaved)r=mcxstore::load(DIR,job.radio,*image,*staging);
      if(unsaved){*image=*retry.image;r.found=true;r.error=0;}
      loaded=image;loaded_epoch=job.epoch;loaded_offline=job.offline;load_ok=r.found;
      if(!image)report_error(mcxstore::MEMORY,ENOMEM);
      else if(r.error)report_error(mcxstore::READ,r.error);
      else if(unsaved)storage.store(2);
      else if(r.found)storage.store(r.recovered?3:1);
      else if(r.recovered)report_error(mcxstore::CORRUPT,EINVAL);
      else if(mkdir(DIR,0755)==0||errno==EEXIST)storage.store(4);
      else report_error(mcxstore::DIRECTORY,errno);
      load_unavailable=!unsaved&&storage.load()==2;
      record_duration(started);
      complete.store(true);continue;
    }
    if(retry.image&&!memcmp(retry.image->meta.radio,job.image->meta.radio,32)){
      // New traffic updates the retry image, not the retry rate.
      if(job.ticket<retry.ticket){heap_caps_free(job.image);continue;}
      heap_caps_free(retry.image);retry=Job{};
      if((int32_t)(tick-retry_at)<0){retry=job;continue;}
    }
    saving.store(true);
    const uint32_t started=xTaskGetTickCount();
    const bool ok=mcxstore::save(DIR,*job.image,*previous,*staging,&cursor);
    if(ok){
      // last-radio is advisory only. Archives still require the full 32-byte identity.
      remember_identity(job.image->meta.radio);
      error_step.store(0);error_number.store(0);storage.store(1);commits.fetch_add(1);
      if(job.ticket>committed.load())committed.store(job.ticket);
      if(retry.image&&!memcmp(retry.image->meta.radio,job.image->meta.radio,32)){
        heap_caps_free(retry.image);retry=Job{};
      }
      heap_caps_free(job.image);
    }else{
      report_error(mcxstore::failure.load(),mcxstore::failure_errno.load());
      // Bound memory under SD failure; preserve newest same-radio snapshot for retry.
      if(retry.image)heap_caps_free(retry.image);
      retry=job;retry_at=xTaskGetTickCount()+pdMS_TO_TICKS(5000);
    }
    record_duration(started);
    saving.store(false);
  }
}
static void begin(){if(!worker)xTaskCreatePinnedToCore(worker_main,"mcx_history",6144,nullptr,1,&worker,0);}
static bool capture(meshmc::Client &c,uint32_t now,bool force=false){
  if(!mcxstore::nonzero(c.public_key))return true;
  if(!force&&((uint32_t)(now-last_capture)<1000||(c.generation==last_generation&&preferences().revision==last_revision)))return true;
  auto *image=allocate();if(!image){report_error(mcxstore::MEMORY,ENOMEM);return false;}
  mcxstore::capture(c,preferences(),*image);Job j;j.image=image;
  if(submit(j)){last_generation=c.generation;last_revision=preferences().revision;last_capture=now;}
  else {heap_caps_free(image);return false;}
  return true;
}
static bool before_attach(meshmc::Client &c,uint32_t now){
  // Save the previous identity before Client::attach clears its model.
  if(restored&&!capture(c,now,true))return false;
  new (&preferences()) mcxchat::Local();restored=restore_requested=restore_unavailable=offline_requested=false;
  last_restore_attempt=now;
  return true;
}
static const char *status(){
  if(storage.load()==2)return "SD: error / unsaved RAM";
  if(dirty.load()||submitted.load()!=committed.load()||saving.load()||storage.load()==0)return "SD: saving / pending";
  switch(storage.load()){case 1:return "SD: journal saved";case 2:return "SD unavailable / unsaved RAM";
    case 3:return "SD: recovered last commit";default:return "SD: no saved history";}
}
static void describe(char *out,size_t cap){
  if(storage.load()==2&&error_step.load())snprintf(out,cap,"SD: %s error (%d) / RAM",mcxstore::step_name(error_step.load()),error_number.load());
  else snprintf(out,cap,"%s",status());
}
static void poll(meshmc::Client &c,uint32_t now,uint32_t epoch){
  if(!init_workspace()){restored=true;c.history_ready=true;storage.store(2);return;}
  if(ble_startup.load()&&!worker)return; // Let controller/host claim internal stacks first.
  begin();if(!worker||worker_failed.load()){restored=true;c.history_ready=true;storage.store(2);return;}
  if(complete.load()){
    if(!loaded_offline&&epoch==loaded_epoch&&c.connected&&!c.initialized&&!c.fault)return;
    bool transmitting=c.busy&&c.active.message>=0;
    for(const auto &m:c.messages)if(m.id&&m.outgoing&&m.delivery==meshmc::QUEUED)transmitting=true;
    if(!loaded_offline&&transmitting)return; // Preserve live queue-to-message IDs during a late SD restore.
    if(load_ok&&loaded){
      if(!c.connected&&(loaded_offline||epoch==loaded_epoch)){mcxstore::restore(*loaded,c,preferences(),false);memcpy(bound_radio,c.public_key,32);restored=true;}
      else if(!loaded_offline&&epoch==loaded_epoch&&c.initialized&&!memcmp(loaded->meta.radio,c.public_key,32)){
        // Messages received while SD was absent are merged after the older journal.
        auto *recent=(meshmc::Message *)heap_caps_malloc(sizeof(c.messages),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(recent){
          memcpy(recent,c.messages,sizeof(c.messages));const uint32_t next=c.next_id;
          auto *edits=allocate();if(edits)edits->local=preferences();
          mcxstore::restore(*loaded,c,preferences(),true);
          const uint32_t first=next>meshmc::MAX_MESSAGES?next-meshmc::MAX_MESSAGES:1;
          for(uint32_t id=first;id<next;++id){const auto &m=recent[(id-1)%meshmc::MAX_MESSAGES];if(m.id!=id)continue;
            bool seen=false;for(auto &old:c.messages)if(old.id&&old.channel==m.channel&&old.outgoing==m.outgoing&&
              old.timestamp==m.timestamp&&(m.channel?(old.slot==m.slot&&!memcmp(old.channel_identity,m.channel_identity,16)):!memcmp(old.key,m.key,6))&&!strcmp(old.text,m.text)){
                old.read=old.read||m.read;seen=true;break;}
            if(!seen){auto &dst=c.append();uint32_t saved=dst.id;dst=m;dst.id=saved;}
          }
          if(edits&&edits->local.revision){
            for(const auto &p:edits->local.threads)if(p.used){auto *dst=preferences().get(p.key,true);if(dst)*dst=p;}
            ++preferences().revision;
          }
          if(edits)heap_caps_free(edits);
          heap_caps_free(recent);restored=true;
        }else {storage.store(2);load_unavailable=true;}

      }
    }
    if(!loaded_offline&&epoch==loaded_epoch&&!c.fault){restored=true;restore_unavailable=load_unavailable;last_generation=last_revision=UINT32_MAX;}
    if(loaded_offline&&load_unavailable){offline_requested=false;last_restore_attempt=now;}
    if(loaded)heap_caps_free(loaded);
    loaded=nullptr;complete.store(false);
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
  c.history_ready=restored;
  dirty.store(c.generation!=last_generation||preferences().revision!=last_revision);
  if(restored&&!restore_unavailable)capture(c,now);
}
static bool ready_for(const meshmc::Client &c){return !c.initialized||restored;}
} // namespace mcxhistory

