#pragma once
#include "meshcrossway_map_projection.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sdcard_port.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#define LODEPNG_NO_COMPILE_CPP
extern "C" {
#include "src/extra/libs/png/lodepng.h"
}
#include <atomic>
#include <new>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
namespace mcxmap {
static constexpr size_t MAX_PNG=256*1024;
static const char *SD_ROOT="/sdcard";
struct Config {uint32_t version=1;uint8_t online=1,zoom=14;uint8_t reserved[2]={};char url[192]="https://tile.openstreetmap.org";char attribution[128]="© OpenStreetMap contributors";};
static bool valid_config(const Config &c){
  if(c.version!=1||c.online>1||c.zoom<3||c.zoom>19||!memchr(c.url,0,sizeof(c.url))||!memchr(c.attribution,0,sizeof(c.attribution))||!c.attribution[0]||strncmp(c.url,"https://",8)||!c.url[8])return false;
  for(const char *p=c.url;*p;++p)if(uint8_t(*p)<=32||strchr("?#@{}",*p))return false;
  return true;
}
struct View {double latitude=0,longitude=0;uint32_t generation=0;unsigned zoom=14,width=400,height=155;};
struct Frame {lv_img_dsc_t image={};View view;unsigned loaded=0,missing=0;char status[96]={};};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Config config;
static View requested;
static Frame *ready=nullptr;
static bool queued=false,loaded_config=false;
static uint32_t revision=0,changed_ms=0;
static TaskHandle_t task=nullptr;
static std::atomic<uint32_t> wanted{0};
static std::atomic<int> last_error{0};
static std::atomic<bool> settings_saved{true};
static uint32_t backoff_started=0,last_request=0;
static bool backing_off=false;
static bool sntp_started=false,waiting_clock=false,cache_full=false;
struct Miss {unsigned z=0,x=0,y=0;uint32_t at=0;bool used=false;};
static Miss misses[12];static unsigned miss_slot=0;
static uint32_t milliseconds(){return uint32_t(esp_timer_get_time()/1000);}
static void release(Frame *f){if(f){heap_caps_free((void *)f->image.data);heap_caps_free(f);}}
static void load_config(){
  if(loaded_config)return;
  nvs_handle_t h;if(nvs_open("mcx_map",NVS_READONLY,&h)==ESP_OK){Config c;size_t n=sizeof(c);
    if(nvs_get_blob(h,"config",&c,&n)==ESP_OK&&n==sizeof(c)&&valid_config(c))config=c;
    nvs_close(h);
  }loaded_config=true;
}
static uint32_t configuration_revision(){portENTER_CRITICAL(&mux);const auto r=revision;portEXIT_CRITICAL(&mux);return r;}
static Config configuration(){portENTER_CRITICAL(&mux);auto c=config;portEXIT_CRITICAL(&mux);return c;}
static void configure(const Config &c){if(!valid_config(c))return;
  portENTER_CRITICAL(&mux);config=c;++revision;changed_ms=milliseconds();settings_saved=false;portEXIT_CRITICAL(&mux);
}
static uint64_t source_hash(const Config &c){uint64_t hash=14695981039346656037ULL;for(const char *p=c.url;*p;++p)hash=(hash^uint8_t(*p))*1099511628211ULL;return hash;}
static void paths(char *file,size_t cap,const Config &c,unsigned z,unsigned x,unsigned y){snprintf(file,cap,"%s/mcx-map-cache/%016llx/%u/%u/%u.png",SD_ROOT,(unsigned long long)source_hash(c),z,x,y);}
static bool directories(const Config &c,unsigned z,unsigned x){
  char path[256],root[160];snprintf(root,sizeof(root),"%s/mcx-map-cache",SD_ROOT);
  if(mkdir(root,0755)&&errno!=EEXIST)return false;
  snprintf(path,sizeof(path),"%s/%016llx",root,(unsigned long long)source_hash(c));if(mkdir(path,0755)&&errno!=EEXIST)return false;
  snprintf(path,sizeof(path),"%s/%016llx/%u",root,(unsigned long long)source_hash(c),z);if(mkdir(path,0755)&&errno!=EEXIST)return false;
  snprintf(path,sizeof(path),"%s/%016llx/%u/%u",root,(unsigned long long)source_hash(c),z,x);return !mkdir(path,0755)||errno==EEXIST;
}
static unsigned char *read_png(const char *path,size_t &size){
  FILE *f=fopen(path,"rb");if(!f)return nullptr;
  struct stat st={};if(fstat(fileno(f),&st)||st.st_size<33||st.st_size>int(MAX_PNG)){fclose(f);return nullptr;}
  size=size_t(st.st_size);auto *p=(unsigned char *)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  const bool ok=p&&fread(p,1,size,f)==size&&!ferror(f);const int close=fclose(f);
  if(!ok||close){heap_caps_free(p);return nullptr;}return p;
}
static bool decode(unsigned char *png,size_t size,unsigned char **rgba){
  const unsigned char signature[]={137,80,78,71,13,10,26,10};
  if(size<33||memcmp(png,signature,8)||memcmp(png+12,"IHDR",4))return false;
  const unsigned char dimension[]={0,0,1,0};if(memcmp(png+16,dimension,4)||memcmp(png+20,dimension,4))return false;
  LodePNGState state;lodepng_state_init(&state);state.decoder.zlibsettings.max_output_size=300000;
  state.decoder.read_text_chunks=0;state.decoder.max_text_size=1024;state.decoder.max_icc_size=1024;
  unsigned width=0,height=0;const unsigned err=lodepng_decode(rgba,&width,&height,&state,png,size);lodepng_state_cleanup(&state);
  if(err||width!=256||height!=256){lv_mem_free(*rgba);*rgba=nullptr;return false;}return true;
}
static bool (*wifi_ready)()=nullptr;
static bool network_allowed(){return wifi_ready&&wifi_ready();}
static unsigned char *checked_png(const char *file,size_t &size);
static unsigned char *obtain(const Config &c,unsigned z,unsigned x,unsigned y,size_t &size){
  if(esp_sdcard_port_get_size()==0)return nullptr;
  char file[192];snprintf(file,sizeof(file),"%s/tiles/%u/%u/%u.png",SD_ROOT,z,x,y);
  if(auto *p=checked_png(file,size))return p;
  paths(file,sizeof(file),c,z,x,y);if(auto *p=checked_png(file,size))return p;
  const uint32_t now=milliseconds();if(!c.online||!network_allowed())return nullptr;
  if(backing_off){if(uint32_t(now-backoff_started)<120000)return nullptr;backing_off=false;}
  if(time(nullptr)<1700000000){
    waiting_clock=true;
    if(!sntp_started){const esp_sntp_config_t ntp=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");const auto rc=esp_netif_sntp_init(&ntp);sntp_started=rc==ESP_OK||rc==ESP_ERR_INVALID_STATE;}
    return nullptr;
  }
  waiting_clock=false;
  for(const auto &m:misses)if(m.used&&m.z==z&&m.x==x&&m.y==y&&uint32_t(now-m.at)<600000)return nullptr;
  uint64_t total=0,free_space=0;
  if(esp_vfs_fat_info(SD_ROOT,&total,&free_space)!=ESP_OK||free_space<8ULL*1024*1024+MAX_PNG){cache_full=true;return nullptr;}
  if(!directories(c,z,x))return nullptr;
  if(uint32_t(now-last_request)<1000)vTaskDelay(pdMS_TO_TICKS(1000-uint32_t(now-last_request)));
  last_request=milliseconds();
  char temporary[200];snprintf(temporary,sizeof(temporary),"%s.tmp",file);FILE *out=fopen(temporary,"wb");if(!out)return nullptr;
  char url[256];snprintf(url,sizeof(url),"%s%s%u/%u/%u.png",c.url,c.url[strlen(c.url)-1]=='/'?"":"/",z,x,y);
  esp_http_client_config_t http={};http.url=url;http.timeout_ms=2500;http.buffer_size=1024;http.buffer_size_tx=512;
  http.user_agent="MeshCrossway/0.8.1 (+https://github.com/Renbrant/MeshCrossway)";http.crt_bundle_attach=esp_crt_bundle_attach;http.disable_auto_redirect=true;
  auto client=esp_http_client_init(&http);bool ok=false;unsigned char *png=nullptr;size=0;
  if(client&&esp_http_client_open(client,0)==ESP_OK){
    const auto length=esp_http_client_fetch_headers(client);const int status=esp_http_client_get_status_code(client);
    if(status==429||status>=500){backoff_started=now;backing_off=true;}
    if(status==200&&length<=int64_t(MAX_PNG)){
      png=(unsigned char *)heap_caps_malloc(MAX_PNG,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
      if(png){int n=0;ok=true;
        while(size<MAX_PNG){n=esp_http_client_read(client,(char *)png+size,int(fmin(size_t(4096),MAX_PNG-size)));if(n<0){ok=false;break;}if(!n)break;size+=n;}
        ok=ok&&size>32&&esp_http_client_is_complete_data_received(client);
      }
    }
  }
  if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}
  unsigned char *test=nullptr;if(ok)ok=decode(png,size,&test);lv_mem_free(test);
  if(ok)ok=fwrite(png,1,size,out)==size&&fflush(out)==0&&fsync(fileno(out))==0;
  if(fclose(out))ok=false;
  if(ok&&remove(file)&&errno!=ENOENT)ok=false;
  if(ok)ok=rename(temporary,file)==0;
  if(!ok){remove(temporary);heap_caps_free(png);png=nullptr;misses[miss_slot]={z,x,y,now,true};miss_slot=(miss_slot+1)%12;}
  // Bound demand from an interactive viewport; never prefetch adjacent regions.

  return png;
}
static unsigned char *checked_png(const char *file,size_t &size){
  auto *png=read_png(file,size);unsigned char *rgba=nullptr;
  if(!png)return nullptr;
  const bool ok=decode(png,size,&rgba);lv_mem_free(rgba);
  if(!ok){heap_caps_free(png);return nullptr;}return png;
}
static Frame *render(const View &v,const Config &c){
  waiting_clock=cache_full=false;
  auto *f=(Frame *)heap_caps_malloc(sizeof(Frame),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!f)return nullptr;new(f) Frame();f->view=v;
  const size_t count=v.width*v.height;auto *pixels=(lv_color_t *)heap_caps_malloc(count*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){release(f);return nullptr;}
  f->image.header.cf=LV_IMG_CF_TRUE_COLOR;f->image.header.w=v.width;f->image.header.h=v.height;f->image.data=(const uint8_t *)pixels;f->image.data_size=count*sizeof(lv_color_t);
  for(unsigned y=0;y<v.height;++y)for(unsigned x=0;x<v.width;++x)pixels[y*v.width+x]=lv_color_hex(((x/32+y/32)&1)?0x222A31:0x1A2027);
  const auto center=project(v.latitude,v.longitude,v.zoom);const int left=int(floor(center.x-double(v.width)/2)),top=int(floor(center.y-double(v.height)/2));
  const int first_x=int(floor(double(left)/256)),first_y=int(floor(double(top)/256));const int tiles=1<<v.zoom;
  for(int ty=first_y;ty<=int(floor(double(top+int(v.height)-1)/256));++ty){
    for(int tx=first_x;tx<=int(floor(double(left+int(v.width)-1)/256));++tx){
      if(wanted.load()!=v.generation){release(f);return nullptr;}
      if(ty<0||ty>=tiles){++f->missing;continue;}
      size_t size=0;auto *png=obtain(c,v.zoom,unsigned((tx%tiles+tiles)%tiles),unsigned(ty),size);unsigned char *rgba=nullptr;
      const bool ok=png&&decode(png,size,&rgba);heap_caps_free(png);
      if(ok){++f->loaded;
        const int dx=tx*256-left,dy=ty*256-top;
        for(int y=fmax(0,dy);y<int(v.height)&&y<dy+256;++y)for(int x=fmax(0,dx);x<int(v.width)&&x<dx+256;++x){
          const auto *p=rgba+4*((y-dy)*256+x-dx);const unsigned alpha=p[3];
          pixels[y*v.width+x]=lv_color_make((p[0]*alpha+230*(255-alpha))/255,(p[1]*alpha+230*(255-alpha))/255,(p[2]*alpha+230*(255-alpha))/255);
        }
      }else ++f->missing;
      lv_mem_free(rgba);
    }
  }
  snprintf(f->status,sizeof(f->status),"z%u / %u tiles / %u missing%s",v.zoom,f->loaded,f->missing,!esp_sdcard_port_get_size()?" / SD unavailable":waiting_clock?" / clock sync":cache_full?" / cache reserve":"");return f;
}
static void worker(void *){
  uint32_t saved=0,last_save=0;uint64_t previous_source=0;
  for(;;){
    portENTER_CRITICAL(&mux);const Config c=config;const uint32_t rev=revision,at=changed_ms;const bool work=queued&&!ready;View v=requested;if(work)queued=false;portEXIT_CRITICAL(&mux);
    const uint32_t now=milliseconds();
    if(rev!=saved&&uint32_t(now-at)>700&&(!last_save||uint32_t(now-last_save)>5000)){last_save=now;nvs_handle_t h;
      if(nvs_open("mcx_map",NVS_READWRITE,&h)==ESP_OK){if(nvs_set_blob(h,"config",&c,sizeof(c))==ESP_OK&&nvs_commit(h)==ESP_OK){saved=rev;portENTER_CRITICAL(&mux);if(revision==rev)settings_saved=true;portEXIT_CRITICAL(&mux);}
        nvs_close(h);}
    }
    if(work){if(source_hash(c)!=previous_source){for(auto &miss:misses)miss=Miss{};backing_off=false;previous_source=source_hash(c);}
      auto *f=render(v,c);if(f&&wanted.load()==v.generation){portENTER_CRITICAL(&mux);ready=f;portEXIT_CRITICAL(&mux);}else release(f);}
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
static bool start(){
  if(task)return true;
  load_config();if(xTaskCreatePinnedToCore(worker,"mcx_map",6144,nullptr,1,&task,0)!=pdPASS){last_error=ESP_ERR_NO_MEM;return false;}return true;
}
static uint32_t request(View v){
  if(v.zoom<3||v.zoom>19||v.width<64||v.width>480||v.height<64||v.height>220||!isfinite(v.latitude)||!isfinite(v.longitude))return 0;
  if(!start())return 0;
  v.generation=wanted.fetch_add(1)+1;
  portENTER_CRITICAL(&mux);requested=v;queued=true;portEXIT_CRITICAL(&mux);return v.generation;
}
static Frame *take(){portENTER_CRITICAL(&mux);auto *f=ready;ready=nullptr;portEXIT_CRITICAL(&mux);return f;}
static void stop(){wanted.fetch_add(1);portENTER_CRITICAL(&mux);queued=false;portEXIT_CRITICAL(&mux);}
}
