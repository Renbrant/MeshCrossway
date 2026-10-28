#pragma once
// Explicitly unencrypted, removable Wi-Fi backup. Never print credential contents.
#include <stdint.h>
#include <errno.h>
#include "esp_sdcard_port.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace meshbackup {
struct credentials_t {char ssid[33];char password[64];};
static const char *FILE_PATH="/sdcard/meshcrossway-wifi.json";
static const char *TEMP_PATH="/sdcard/meshcrossway-wifi.tmp";
static const char *OLD_PATH="/sdcard/meshcrossway-wifi.bak";
enum Status {UNAVAILABLE,READY,SAVED,RESTORED,INVALID_FILE,WRITE_FAILED};
static std::atomic<int> state{UNAVAILABLE};
// Expose filesystem failure stages without exposing credentials.
enum FailureStep {NONE,INPUT,ENCODE,OPEN_TEMP,WRITE_TEMP,FLUSH_TEMP,SYNC_TEMP,CLOSE_TEMP,
                  REMOVE_BACKUP,ROTATE_BACKUP,REMOVE_INVALID,COMMIT_TEMP};
static std::atomic<int> failure_step{NONE};
static std::atomic<int> failure_errno{0};
static const char *step_name(int step){
  switch(step){
    case INPUT:return "input";case ENCODE:return "JSON";case OPEN_TEMP:return "open";
    case WRITE_TEMP:return "write";case FLUSH_TEMP:return "flush";case SYNC_TEMP:return "sync";
    case CLOSE_TEMP:return "close";case REMOVE_BACKUP:return "remove backup";
    case ROTATE_BACKUP:return "rotate";case REMOVE_INVALID:return "remove invalid";
    case COMMIT_TEMP:return "rename";default:return "unknown";
  }
}
static bool failed(FailureStep step,int err){
  failure_step.store(step);
  failure_errno.store(err);
  printf("[MCX-SD] write failure stage=%s errno=%d\n",step_name(step),err);
  return false;
}
static QueueHandle_t queue=nullptr;
static bool mounted=false;
static const char *status(){
  switch(state.load()){
    case READY:return "ready";case SAVED:return "saved";case RESTORED:return "restored";
    case INVALID_FILE:return "invalid file";case WRITE_FAILED:return "write failed";
    default:return "unavailable";
  }
}
static void describe(char *out,size_t size){
  if(!out||!size)return;
  if(state.load()!=WRITE_FAILED){snprintf(out,size,"%s",status());return;}
  snprintf(out,size,"write failed (%s: %d)",step_name(failure_step.load()),failure_errno.load());
}
static bool valid(const credentials_t &c){
  const size_t n=strnlen(c.ssid,sizeof(c.ssid)),p=strnlen(c.password,sizeof(c.password));
  return n>0&&n<=32&&p>=8&&p<=63;
}
static bool read_file(const char *path,credentials_t &out){
  FILE *f=fopen(path,"rb");if(!f)return false;
  char data[1025]={};const size_t n=fread(data,1,1024,f);
  const int extra=fgetc(f);const bool bad=ferror(f)||extra!=EOF;fclose(f);
  if(bad||!n||memchr(data,0,n))return false;
  // Allow only a flat object, so malformed SD data cannot trigger deep parser recursion.
  bool quoted=false,escaped=false;int depth=0;
  for(size_t i=0;i<n;i++){
    char c=data[i];
    if(quoted){
      if(escaped){if(c=='u'&&i+4<n&&!memcmp(data+i+1,"0000",4))return false;escaped=false;}
      else if(c=='\\')escaped=true;else if(c=='"')quoted=false;
    }
    else if(c=='"')quoted=true;
    else if(c=='['||c==']')return false;
    else if(c=='{'&&++depth>1)return false;
    else if(c=='}'&&--depth<0)return false;
  }
  if(quoted||depth!=0)return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithOpts(data,&end,true);
  if(!root)return false;
  const cJSON *format=cJSON_GetObjectItemCaseSensitive(root,"format");
  const cJSON *ssid=cJSON_GetObjectItemCaseSensitive(root,"ssid");
  const cJSON *password=cJSON_GetObjectItemCaseSensitive(root,"password");
  bool ok=cJSON_IsObject(root)&&cJSON_GetArraySize(root)==3&&cJSON_IsNumber(format)&&format->valuedouble==1&&cJSON_IsString(ssid)&&cJSON_IsString(password);
  if(ok){
    const size_t a=strlen(ssid->valuestring),b=strlen(password->valuestring);
    ok=a>0&&a<=32&&b>=8&&b<=63;
    if(ok){memset(&out,0,sizeof(out));memcpy(out.ssid,ssid->valuestring,a);memcpy(out.password,password->valuestring,b);}
  }
  cJSON_Delete(root);return ok;
}
static bool save_file(const credentials_t &c){
  failure_step.store(NONE);failure_errno.store(0);
  if(!valid(c))return failed(INPUT,EINVAL);
  credentials_t previous={};const bool old_valid=read_file(FILE_PATH,previous);
  if(old_valid&&!strcmp(previous.ssid,c.ssid)&&!strcmp(previous.password,c.password))return true;
  cJSON *root=cJSON_CreateObject();
  if(!root)return failed(ENCODE,ENOMEM);
  const bool encoded=cJSON_AddNumberToObject(root,"format",1)&&
    cJSON_AddStringToObject(root,"ssid",c.ssid)&&
    cJSON_AddStringToObject(root,"password",c.password);
  char *json=encoded?cJSON_PrintUnformatted(root):nullptr;
  cJSON_Delete(root);
  if(!json)return failed(ENCODE,ENOMEM);
  FILE *f=fopen(TEMP_PATH,"wb");
  if(!f){const int err=errno;cJSON_free(json);return failed(OPEN_TEMP,err);}
  const size_t length=strlen(json);
  bool ok=fwrite(json,1,length,f)==length;
  if(!ok)failed(WRITE_TEMP,errno);
  if(ok){
    ok=fflush(f)==0;
    if(!ok)failed(FLUSH_TEMP,errno);
  }
  if(ok){
    ok=fsync(fileno(f))==0;
    if(!ok)failed(SYNC_TEMP,errno);
  }
  const int close_result=fclose(f);
  if(close_result!=0&&ok){ok=false;failed(CLOSE_TEMP,errno);}
  cJSON_free(json);
  if(!ok){unlink(TEMP_PATH);return false;}
  bool rotated=false;
  if(old_valid){
    if(unlink(OLD_PATH)!=0&&errno!=ENOENT){
      const int err=errno;unlink(TEMP_PATH);return failed(REMOVE_BACKUP,err);
    }
    if(rename(FILE_PATH,OLD_PATH)!=0){
      const int err=errno;unlink(TEMP_PATH);return failed(ROTATE_BACKUP,err);
    }
    rotated=true;
  }else if(access(FILE_PATH,F_OK)==0&&unlink(FILE_PATH)!=0){
    const int err=errno;unlink(TEMP_PATH);return failed(REMOVE_INVALID,err);
  }
  if(rename(TEMP_PATH,FILE_PATH)!=0){
    const int err=errno;
    if(rotated)(void)rename(OLD_PATH,FILE_PATH);
    unlink(TEMP_PATH);
    return failed(COMMIT_TEMP,err);
  }
  return true;
}
static void worker(void *){
  credentials_t c={};
  for(;;){
    if(xQueueReceive(queue,&c,portMAX_DELAY)==pdTRUE){
      state.store(save_file(c)?SAVED:WRITE_FAILED);
      memset(&c,0,sizeof(c));
      printf("[MCX-SD] Wi-Fi backup status=%s\n",status());
    }
  }
}
static void start(){
  // Vendor port mounts FAT on SDMMC CMD=10,D0=9,CLK=11; formatting is disabled.
  esp_sdcard_port_init();
  mounted=esp_sdcard_port_get_size()>0;
  if(!mounted){state.store(UNAVAILABLE);return;}
  queue=xQueueCreate(1,sizeof(credentials_t));
  if(!queue){state.store(UNAVAILABLE);mounted=false;return;}
  if(xTaskCreate(worker,"mcx_sd_backup",6144,nullptr,2,nullptr)!=pdPASS){
    vQueueDelete(queue);queue=nullptr;mounted=false;state.store(UNAVAILABLE);return;
  }
  state.store(READY);
}
static bool restore(wifi_config_t &station){
  if(!mounted||station.sta.ssid[0])return false;
  credentials_t c={};
  if(!read_file(FILE_PATH,c)&&!read_file(OLD_PATH,c)){
    if(access(FILE_PATH,F_OK)==0||access(OLD_PATH,F_OK)==0)state.store(INVALID_FILE);
    return false;
  }
  wifi_config_t restored={};
  memcpy(restored.sta.ssid,c.ssid,strlen(c.ssid));
  memcpy(restored.sta.password,c.password,strlen(c.password));
  memset(&c,0,sizeof(c));
  if(esp_wifi_set_config(WIFI_IF_STA,&restored)!=ESP_OK)return false;
  station=restored;state.store(RESTORED);return true;
}
static void schedule(const wifi_config_t &station){
  if(!mounted||!queue)return;
  credentials_t c={};
  memcpy(c.ssid,station.sta.ssid,32);memcpy(c.password,station.sta.password,63);
  // Do not silently truncate unsupported 64-byte PSKs.
  if(station.sta.password[63]!=0||!valid(c))return;
  (void)xQueueOverwrite(queue,&c);memset(&c,0,sizeof(c));
}
} // namespace meshbackup
