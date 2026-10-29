#pragma once
// MeshCrossway experimental unencrypted microSD history (MeshCore only).
// Runs on LVGL owner thread; file writes are delegated to a single FreeRTOS worker.
// No credentials or eFuse secrets are accessed. An SD card can be read by anyone.
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

namespace mcxhistory {
static constexpr uint32_t MAGIC=0x3158434du; // MCX1
static constexpr uint32_t SCHEMA=1;
static constexpr const char *DIR="/sdcard/mcx-history";
struct Payload {
  uint8_t radio[32];
  uint16_t contacts=0, channels=0, messages=0;
  uint32_t next_id=1;
  meshmc::Contact contact[meshmc::MAX_CONTACTS];
  meshmc::Channel channel[meshmc::MAX_CHANNELS];
  meshmc::Message message[meshmc::MAX_MESSAGES];
};
struct Image { uint32_t magic=0, schema=0, bytes=0, checksum=0; Payload data; };
static TaskHandle_t worker=nullptr;
static Image *pending=nullptr;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static uint32_t last_generation=0, last_attempt=0;
static bool restored=false, offline_loaded=false;
static uint32_t last_connection_epoch=0;
static uint8_t bound_radio[32]={};
static bool nonzero(const uint8_t *p){for(int i=0;i<32;i++)if(p[i])return true;return false;}
static uint32_t checksum(const Payload &p){
  const uint8_t *bytes=(const uint8_t *)&p;
  uint32_t hash=2166136261u;
  for(size_t i=0;i<sizeof(Payload);i++)hash=(hash^bytes[i])*16777619u;
  return hash; // Accidental corruption detection ONLY; not authentication.
}
static void filename(char *out,size_t n,const uint8_t radio[32],const char *suffix){
  char id[25]={};
  for(int i=0;i<12;i++)snprintf(id+i*2,3,"%02x",radio[i]);
  snprintf(out,n,"%s/mc-%s.%s",DIR,id,suffix);
}
static bool read_path(const char *path,Image *image,const uint8_t *key){
  FILE *f=fopen(path,"rb");if(!f)return false;
  const bool ok=fread(image,1,sizeof(Image),f)==sizeof(Image)
    && fgetc(f)==EOF;
  fclose(f);
  if(!ok||image->magic!=MAGIC||image->schema!=SCHEMA
     ||image->bytes!=sizeof(Payload)||image->data.contacts>meshmc::MAX_CONTACTS
     ||image->data.channels>meshmc::MAX_CHANNELS
     ||image->data.messages>meshmc::MAX_MESSAGES
     ||!nonzero(image->data.radio)||memcmp(image->data.radio,key,32)
     ||image->checksum!=checksum(image->data))return false;
  return true;
}
static bool load(const uint8_t radio[32],meshmc::Client &c,bool history_only){
  if(!nonzero(radio))return false;
  Image *image=(Image *)heap_caps_malloc(sizeof(Image),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!image)return false;
  char path[110];filename(path,sizeof(path),radio,"bin");
  bool ok=read_path(path,image,radio);
  if(!ok){filename(path,sizeof(path),radio,"bak");ok=read_path(path,image,radio);}
  if(ok){
    const Payload &d=image->data;
    if(!history_only){
      c.contact_count=d.contacts;
      c.channel_count=d.channels;
      memcpy(c.contacts,d.contact,sizeof(d.contact));
      memcpy(c.channels,d.channel,sizeof(d.channel));
      memcpy(c.public_key,radio,32);
    }else{
      // Current radio directory is authoritative; only fill absent slots.
      if(!c.contact_count){c.contact_count=d.contacts;memcpy(c.contacts,d.contact,sizeof(d.contact));}
      for(size_t i=0;i<d.channels&&i<c.channel_count;i++)
        if(!c.channels[i].loaded)c.channels[i]=d.channel[i];
    }
    // Restore in chronological ring order. Never replay outgoing messages.
    const uint32_t first=d.next_id>d.messages?d.next_id-d.messages:1;
    for(uint32_t id=first;id<d.next_id;id++){
      const meshmc::Message &m=d.message[(id-1)%meshmc::MAX_MESSAGES];
      if(m.id!=id)continue;
      bool seen=false;
      for(const auto &existing:c.messages)
        if(existing.id && existing.channel==m.channel && existing.outgoing==m.outgoing
          && existing.timestamp==m.timestamp && existing.slot==m.slot
          && !memcmp(existing.key,m.key,sizeof(m.key)) && !strcmp(existing.text,m.text)){
          seen=true;break;
        }
      if(!seen){
        meshmc::Message &dst=c.append();const uint32_t new_id=dst.id;
        dst=m;dst.id=new_id;
        if(dst.outgoing&&(dst.delivery==meshmc::QUEUED||dst.delivery==meshmc::ACCEPTED))
          dst.delivery=meshmc::UNCERTAIN;
      }
    }
    ++c.generation;
  }
  heap_caps_free(image);
  return ok;
}
static void save_image(Image *image){
  if(!image)return;
  struct stat st={};
  if(stat("/sdcard",&st)||mkdir(DIR,0755)&&errno!=EEXIST)return;
  char current[110],temp[110],backup[110];
  filename(current,sizeof(current),image->data.radio,"bin");
  filename(temp,sizeof(temp),image->data.radio,"tmp");
  filename(backup,sizeof(backup),image->data.radio,"bak");
  FILE *f=fopen(temp,"wb");if(!f)return;
  bool ok=fwrite(image,1,sizeof(*image),f)==sizeof(*image);
  if(ok)ok=fflush(f)==0 && fsync(fileno(f))==0;
  if(fclose(f)!=0)ok=false;
  if(!ok){remove(temp);return;}
  // Keep last committed snapshot if power is lost during replacement.
  remove(backup);
  rename(current,backup);
  if(rename(temp,current)!=0){rename(backup,current);remove(temp);return;}
  FILE *last=fopen(DIR "/last-radio.tmp","wb");
  if(last){
    bool wrote=fwrite(image->data.radio,1,32,last)==32;
    if(wrote)wrote=fflush(last)==0 && fsync(fileno(last))==0;
    if(fclose(last)!=0)wrote=false;
    if(wrote)rename(DIR "/last-radio.tmp",DIR "/last-radio");
  }
}
static void task(void *){
  while(true){
    Image *image=nullptr;
    portENTER_CRITICAL(&lock);image=pending;pending=nullptr;portEXIT_CRITICAL(&lock);
    if(image){save_image(image);heap_caps_free(image);}
    else vTaskDelay(pdMS_TO_TICKS(250));
  }
}
static void begin(){
  if(!worker)xTaskCreatePinnedToCore(task,"mcx_sd_history",4096,nullptr,2,&worker,0);
}
static void restore_offline(meshmc::Client &c){
  if(offline_loaded)return;
  offline_loaded=true;
  FILE *f=fopen(DIR "/last-radio","rb");
  if(!f)return;
  uint8_t radio[32]={};
  bool ok=fread(radio,1,32,f)==32 && fgetc(f)==EOF;
  fclose(f);
  if(ok&&nonzero(radio))load(radio,c,false);
}
static void poll(meshmc::Client &c,uint32_t now,uint32_t epoch){
  if(epoch!=last_connection_epoch){last_connection_epoch=epoch;restored=false;}
  if(!offline_loaded)restore_offline(c);
  if(!c.connected||!c.initialized||!nonzero(c.public_key))return;
  if(!restored||memcmp(bound_radio,c.public_key,32)){
    memcpy(bound_radio,c.public_key,32);
    // Identity is known and radio bootstrap is complete. Merge only its archive.
    load(bound_radio,c,true);
    restored=true;
    last_generation=0;
  }
  if((uint32_t)(now-last_attempt)<10000||c.generation==last_generation)return;
  last_attempt=now;
  begin();
  if(!worker)return;
  Image *image=(Image *)heap_caps_calloc(1,sizeof(Image),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!image)return;
  image->magic=MAGIC;image->schema=SCHEMA;image->bytes=sizeof(Payload);
  Payload &d=image->data;
  memcpy(d.radio,c.public_key,32);
  d.contacts=c.contact_count;d.channels=c.channel_count;
  d.messages=c.message_count;d.next_id=c.next_id;
  memcpy(d.contact,c.contacts,sizeof(d.contact));
  memcpy(d.channel,c.channels,sizeof(d.channel));
  memcpy(d.message,c.messages,sizeof(d.message));
  image->checksum=checksum(d);
  portENTER_CRITICAL(&lock);
  if(!pending){pending=image;image=nullptr;last_generation=c.generation;}
  portEXIT_CRITICAL(&lock);
  if(image)heap_caps_free(image);
}
} // namespace mcxhistory
