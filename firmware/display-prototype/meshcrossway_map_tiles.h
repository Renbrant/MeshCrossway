#pragma once
#include "meshcrossway_memory_log.h"
#include "meshcrossway_map_projection.h"
#include "meshcrossway_radio_priority.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sdcard_port.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
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
static constexpr uint32_t MAP_WORKER_STACK_BYTES=6144;
// Worker-owned bounded LRU: twelve RGB565 tiles, 1.5 MiB of PSRAM at most.
struct Tile {uint64_t source=0;unsigned z=0,x=0,y=0;uint32_t used=0;lv_color_t *pixels=nullptr;};
static Tile tile_cache[12];static uint32_t tile_clock=0;
static std::atomic<uint32_t> cache_hits{0},decode_count{0},last_render_ms{0};
static void clear_tiles(){for(auto &t:tile_cache){heap_caps_free(t.pixels);t=Tile{};}}
static lv_color_t *cached_tile(uint64_t source,unsigned z,unsigned x,unsigned y){
  for(auto &t:tile_cache)if(t.pixels&&t.source==source&&t.z==z&&t.x==x&&t.y==y){t.used=++tile_clock;++cache_hits;return t.pixels;}
  return nullptr;
}
static lv_color_t *keep_tile(uint64_t source,unsigned z,unsigned x,unsigned y,const unsigned char *rgba){
  auto *slot=&tile_cache[0];for(auto &t:tile_cache){if(!t.pixels){slot=&t;break;}if(t.used<slot->used)slot=&t;}
  if(!slot->pixels)slot->pixels=(lv_color_t *)heap_caps_malloc(256*256*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!slot->pixels)return nullptr;
  for(unsigned i=0;i<256*256;++i){const auto *p=rgba+4*i;const unsigned a=p[3];slot->pixels[i]=lv_color_make((p[0]*a+230*(255-a))/255,(p[1]*a+230*(255-a))/255,(p[2]*a+230*(255-a))/255);}
  slot->source=source;slot->z=z;slot->x=x;slot->y=y;slot->used=++tile_clock;return slot->pixels;
}
static const char *SD_ROOT="/sdcard";
// Schema 5 appends independent MeshCore size and local night presentation.
// The leading schema-1..4 blob layout remains readable for migration.
struct Config {uint32_t version=5;uint8_t online=1,zoom=14,hide_nodes=0,show_node_names=0;char url[192]="https://tile.openstreetmap.org";char attribution[128]="© OpenStreetMap contributors";uint8_t meshcore_size=0,night=0;};
static_assert(offsetof(Config,meshcore_size)==328,"Legacy map configuration prefix changed");
enum NodeIcon {NODE_CIRCLE=0,NODE_SQUARE=1,NODE_CROSS=2,NODE_NAME=3};
enum NodeColor {NODE_COLOR_AUTO=0,NODE_COLOR_ORANGE=1,NODE_COLOR_CYAN=2,NODE_COLOR_GREEN=3,NODE_COLOR_RED=4,NODE_COLOR_PURPLE=5,NODE_COLOR_YELLOW=6,NODE_COLOR_WHITE=7};
static constexpr unsigned NODE_SIZES[]={8,12,18,22,26,30};
static constexpr uint32_t NODE_COLORS[]={0,0xFF8A22,0x59C9F5,0x61D69B,0xFF6262,0xB99AFF,0xFFD45A,0xFFFFFF};
static bool nodes_hidden(const Config &c){return c.hide_nodes&1;}
static bool nodes_grouped(const Config &c){return !(c.show_node_names&0x80u);}
static void set_nodes_grouped(Config &c,bool enabled){c.show_node_names=uint8_t((c.show_node_names&0x7fu)|(enabled?0u:0x80u));}
static NodeIcon node_icon(const Config &c,bool meshcore){return NodeIcon((c.hide_nodes>>(meshcore?6:4))&3);}
static unsigned node_size_index(const Config &c,bool meshcore=false){return meshcore?c.meshcore_size:(c.hide_nodes>>1)&7;}
static unsigned node_size(const Config &c,bool meshcore=false){return NODE_SIZES[node_size_index(c,meshcore)];}
static NodeColor node_color(const Config &c,bool meshcore){return NodeColor((c.show_node_names>>(meshcore?4:1))&7);}
static uint32_t node_color_hex(const Config &c,bool meshcore){const auto selected=node_color(c,meshcore);return selected==NODE_COLOR_AUTO?(meshcore?0x9C88FF:0x72D8A5):NODE_COLORS[selected];}
static void set_nodes_hidden(Config &c,bool hidden){c.hide_nodes=uint8_t((c.hide_nodes&~1u)|(hidden?1u:0u));}
static void set_node_icon(Config &c,bool meshcore,unsigned icon){const unsigned shift=meshcore?6:4;const uint8_t mask=uint8_t(~(3u<<shift));c.hide_nodes=uint8_t((c.hide_nodes&mask)|((icon%4u)<<shift));}
static void set_node_size(Config &c,unsigned size,bool meshcore=false){if(meshcore)c.meshcore_size=uint8_t(size%6u);else c.hide_nodes=uint8_t((c.hide_nodes&0xF1u)|((size%6u)<<1));}
static void set_node_color(Config &c,bool meshcore,unsigned color){const unsigned shift=meshcore?4:1;const uint8_t mask=uint8_t(~(7u<<shift));c.show_node_names=uint8_t((c.show_node_names&mask)|((color&7u)<<shift));}
static void set_node_names(Config &c,bool enabled){c.show_node_names=uint8_t((c.show_node_names&~1u)|(enabled?1u:0u));}
static bool valid_config(const Config &c){
  if(c.version!=5||c.meshcore_size>=6||c.night>1||c.online>1||((c.hide_nodes>>1)&7)>=6||((c.hide_nodes>>4)&3)>NODE_NAME||((c.hide_nodes>>6)&3)>NODE_NAME||c.zoom<3||c.zoom>19||!memchr(c.url,0,sizeof(c.url))||!memchr(c.attribution,0,sizeof(c.attribution))||!c.attribution[0]||strncmp(c.url,"https://",8)||!c.url[8])return false;
  for(const char *p=c.url;*p;++p)if(uint8_t(*p)<=32||strchr("?#@{}",*p))return false;
  return true;
}
static bool migrate_config(Config &c){
  if(c.version==5)return valid_config(c);
  if(c.version==1){
    if(c.online>1||((c.hide_nodes>>1)&3)>NODE_CROSS||c.show_node_names>1)return false;
    const uint8_t old_nodes=c.hide_nodes,old_names=c.show_node_names;
    const unsigned icon=(old_nodes>>1)&3,size=(old_nodes>>3)&3,color=(old_nodes>>5)&7;
    c.hide_nodes=uint8_t((old_nodes&1u)|(size<<1)|(icon<<3)|(icon<<5));
    c.show_node_names=uint8_t((old_names&1u)|(color<<1)|(color<<4));c.version=2;
  }
  if(c.version==2){
    if(c.online>1||(c.hide_nodes&0x80)||((c.hide_nodes>>3)&3)>NODE_CROSS||((c.hide_nodes>>5)&3)>NODE_CROSS||(c.show_node_names&0x80))return false;
    const uint8_t old=c.hide_nodes;const unsigned size=(old>>1)&3,mt=(old>>3)&3,mc=(old>>5)&3;
    c.hide_nodes=uint8_t((old&1u)|(size<<1)|(mt<<4)|(mc<<6));c.version=3;
  }
  if(c.version==3){if(c.show_node_names&0x80)return false;c.version=4;}
  if(c.version==4){c.meshcore_size=uint8_t((c.hide_nodes>>1)&7);c.night=0;c.version=5;}
  return valid_config(c);
}
enum Provider {STREET=0,TOPO=1,STREET_NIGHT=2};
static Provider provider(const Config &c){return !strcmp(c.url,"https://a.tile.opentopomap.org")?TOPO:c.night?STREET_NIGHT:STREET;}
static unsigned max_zoom(const Config &c){return provider(c)==TOPO?17:19;}
static const char *provider_name(const Config &c){return provider(c)==TOPO?"Topographic / OpenTopoMap":provider(c)==STREET_NIGHT?"Street / OpenStreetMap night":"Street / OpenStreetMap";}
static void select_provider(Config &c,unsigned id){
  c.night=id==STREET_NIGHT;
  snprintf(c.url,sizeof(c.url),"%s",id==TOPO?"https://a.tile.opentopomap.org":"https://tile.openstreetmap.org");
  snprintf(c.attribution,sizeof(c.attribution),"%s",id==TOPO?"© OpenStreetMap contributors, SRTM | © OpenTopoMap (CC-BY-SA)":"© OpenStreetMap contributors");
  if(c.zoom>max_zoom(c))c.zoom=uint8_t(max_zoom(c));
}
struct View {double latitude=0,longitude=0;uint32_t generation=0;unsigned zoom=14,width=400,height=155;};
struct Frame {lv_img_dsc_t image={};View view;uint64_t source=0;unsigned loaded=0,missing=0;char status[96]={};};
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
static uint32_t backoff_started=0,backoff_ms=0,last_request=0;
static bool backing_off=false;
static bool sntp_started=false,waiting_clock=false,cache_full=false;
struct Miss {unsigned z=0,x=0,y=0;uint32_t at=0;bool used=false;};
static Miss misses[12];static unsigned miss_slot=0;
static uint32_t milliseconds(){return uint32_t(esp_timer_get_time()/1000);}
static void release(Frame *f){if(f){heap_caps_free((void *)f->image.data);heap_caps_free(f);}}
static void load_config(){
  if(loaded_config)return;
  nvs_handle_t h;if(nvs_open("mcx_map",NVS_READONLY,&h)==ESP_OK){Config c;size_t n=sizeof(c);
    if(nvs_get_blob(h,"config",&c,&n)==ESP_OK&&((c.version==5&&n==sizeof(c))||(c.version>=1&&c.version<=4&&n==offsetof(Config,meshcore_size)))){const bool legacy=c.version<5;if(migrate_config(c)){config=c;if(legacy){revision=1;changed_ms=milliseconds();settings_saved=false;}}}
    select_provider(config,provider(config));
    nvs_close(h);
  }loaded_config=true;
}
static void persist_config(){
  static uint32_t last_attempt=0;
  Config c;uint32_t rev,at;
  portENTER_CRITICAL(&mux);c=config;rev=revision;at=changed_ms;const bool pending=!settings_saved.load();portEXIT_CRITICAL(&mux);
  const uint32_t now=milliseconds();
  if(!pending||uint32_t(now-at)<700||(last_attempt&&uint32_t(now-last_attempt)<5000))return;
  last_attempt=now;
  nvs_handle_t h;
  if(nvs_open("mcx_map",NVS_READWRITE,&h)==ESP_OK){
    const bool ok=nvs_set_blob(h,"config",&c,sizeof(c))==ESP_OK&&nvs_commit(h)==ESP_OK;
    nvs_close(h);
    if(ok){portENTER_CRITICAL(&mux);if(revision==rev)settings_saved=true;portEXIT_CRITICAL(&mux);}
  }
}
static uint32_t configuration_revision(){portENTER_CRITICAL(&mux);const auto r=revision;portEXIT_CRITICAL(&mux);return r;}
static Config configuration(){portENTER_CRITICAL(&mux);auto c=config;portEXIT_CRITICAL(&mux);return c;}
static void configure(const Config &c){if(!valid_config(c))return;
  Config chosen=c;select_provider(chosen,provider(chosen));
  portENTER_CRITICAL(&mux);config=chosen;++revision;changed_ms=milliseconds();settings_saved=false;wanted.fetch_add(1);queued=false;portEXIT_CRITICAL(&mux);
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
  ++decode_count;LodePNGState state;lodepng_state_init(&state);state.decoder.zlibsettings.max_output_size=300000;
  state.decoder.read_text_chunks=0;state.decoder.max_text_size=1024;state.decoder.max_icc_size=1024;
  unsigned width=0,height=0;const unsigned err=lodepng_decode(rgba,&width,&height,&state,png,size);lodepng_state_cleanup(&state);
  if(err||width!=256||height!=256){lv_mem_free(*rgba);*rgba=nullptr;return false;}return true;
}
static bool (*wifi_ready)()=nullptr;
static bool network_allowed(){return wifi_ready&&wifi_ready();}
static unsigned char *checked_png(const char *file,size_t &size,unsigned char **decoded=nullptr);
static unsigned char *obtain(const Config &c,unsigned z,unsigned x,unsigned y,size_t &size,unsigned char **decoded=nullptr,uint32_t generation=0){
  if(decoded)*decoded=nullptr;
  const auto cancelled=[generation](){return mcxpriority::ble_initializing.load()||(generation&&wanted.load()!=generation);};
  if(cancelled())return nullptr;
  if(esp_sdcard_port_get_size()==0)return nullptr;
  char file[192];snprintf(file,sizeof(file),"%s/tiles/%s%u/%u/%u.png",SD_ROOT,provider(c)==TOPO?"opentopo/":"",z,x,y);
  if(auto *p=checked_png(file,size,decoded))return p;
  paths(file,sizeof(file),c,z,x,y);if(auto *p=checked_png(file,size,decoded))return p;
  const uint32_t now=milliseconds();if(!c.online||!network_allowed())return nullptr;
  if(backing_off){if(uint32_t(now-backoff_started)<backoff_ms)return nullptr;backing_off=false;}
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
  if(cancelled())return nullptr;
  last_request=milliseconds();
  char temporary[200];snprintf(temporary,sizeof(temporary),"%s.tmp",file);FILE *out=fopen(temporary,"wb");if(!out)return nullptr;
  char url[256];snprintf(url,sizeof(url),"%s%s%u/%u/%u.png",c.url,c.url[strlen(c.url)-1]=='/'?"":"/",z,x,y);
  esp_http_client_config_t http={};http.url=url;http.timeout_ms=2500;http.buffer_size=1024;http.buffer_size_tx=512;
  http.user_agent="MeshCrossway/0.8.4 (+https://github.com/Renbrant/MeshCrossway)";http.crt_bundle_attach=esp_crt_bundle_attach;http.disable_auto_redirect=true;
  auto client=esp_http_client_init(&http);bool ok=false,valid_payload=false,decode_attempted=false;int response=0;unsigned char *png=nullptr;size=0;
  if(client&&esp_http_client_open(client,0)==ESP_OK){
    const auto length=esp_http_client_fetch_headers(client);const int status=esp_http_client_get_status_code(client);response=status;
    if(status==429||status>=500){backoff_started=now;backoff_ms=120000;backing_off=true;}
    if(status==200&&length<=int64_t(MAX_PNG)){
      png=(unsigned char *)heap_caps_malloc(MAX_PNG,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
      if(png){int n=0;ok=true;
        while(size<MAX_PNG){if(cancelled()){ok=false;break;}n=esp_http_client_read(client,(char *)png+size,int(fmin(size_t(4096),MAX_PNG-size)));if(n<0){ok=false;break;}if(!n)break;size+=n;}
        ok=ok&&size>32&&esp_http_client_is_complete_data_received(client);
      }
    }
  }
  if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}
  unsigned char *test=nullptr;if(ok&&!cancelled()){decode_attempted=true;valid_payload=decode(png,size,&test);ok=valid_payload;}else ok=false;
  if(ok)ok=fwrite(png,1,size,out)==size&&fflush(out)==0&&fsync(fileno(out))==0;
  if(fclose(out))ok=false;
  if(ok&&remove(file)&&errno!=ENOENT)ok=false;
  if(ok)ok=rename(temporary,file)==0;
  if(!ok){
    lv_mem_free(test);test=nullptr;
    const bool missing=response==404||response==410||(response==200&&decode_attempted&&!valid_payload);
    remove(temporary);heap_caps_free(png);png=nullptr;
    if(missing){misses[miss_slot]={z,x,y,now,true};miss_slot=(miss_slot+1)%12;}
    else if(!cancelled()&&!backing_off){backoff_started=now;backoff_ms=10000;backing_off=true;}
  }
  // Bound demand from an interactive viewport; never prefetch adjacent regions.

  if(decoded)*decoded=test;else lv_mem_free(test);
  return png;
}
static unsigned char *checked_png(const char *file,size_t &size,unsigned char **decoded){
  auto *png=read_png(file,size);unsigned char *rgba=nullptr;
  if(!png)return nullptr;
  const bool ok=decode(png,size,&rgba);
  if(!ok){heap_caps_free(png);return nullptr;}
  if(decoded)*decoded=rgba;else lv_mem_free(rgba);return png;
}
// Invert lightness while retaining hue, then tint to a dark blue-gray palette.
// Apply only to the composed frame: shared SD/RAM tiles stay in their original colors.
static lv_color_t night_pixel(lv_color_t color){
  const uint32_t rgb=lv_color_to32(color);const int r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
  int hi=r>g?r:g;if(b>hi)hi=b;int lo=r<g?r:g;if(b<lo)lo=b;
  const int shift=255-hi-lo;
  return lv_color_make(uint8_t(8+(r+shift)*200/255),uint8_t(12+(g+shift)*200/255),uint8_t(18+(b+shift)*210/255));
}
static Frame *render(const View &v,const Config &c){
  const auto begun=milliseconds();waiting_clock=cache_full=false;
  if(!esp_sdcard_port_get_size())clear_tiles();
  auto *f=(Frame *)heap_caps_malloc(sizeof(Frame),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!f)return nullptr;new(f) Frame();f->view=v;f->source=source_hash(c);
  const bool night=provider(c)==STREET_NIGHT;
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
      const unsigned tile_x=unsigned((tx%tiles+tiles)%tiles),tile_y=unsigned(ty);
      auto *tile=esp_sdcard_port_get_size()?cached_tile(f->source,v.zoom,tile_x,tile_y):nullptr;
      unsigned char *rgba=nullptr;
      if(!tile){size_t size=0;auto *png=obtain(c,v.zoom,tile_x,tile_y,size,&rgba,v.generation);heap_caps_free(png);
        if(rgba)tile=keep_tile(f->source,v.zoom,tile_x,tile_y,rgba);
      }
      if(wanted.load()!=v.generation){lv_mem_free(rgba);release(f);return nullptr;}
      if(tile||rgba){++f->loaded;
        const int dx=tx*256-left,dy=ty*256-top;
        for(int y=fmax(0,dy);y<int(v.height)&&y<dy+256;++y)for(int x=fmax(0,dx);x<int(v.width)&&x<dx+256;++x){
          const unsigned i=(y-dy)*256+x-dx;
          if(tile)pixels[y*v.width+x]=tile[i];
          else {const auto *p=rgba+4*i;const unsigned a=p[3];pixels[y*v.width+x]=lv_color_make((p[0]*a+230*(255-a))/255,(p[1]*a+230*(255-a))/255,(p[2]*a+230*(255-a))/255);}
          if(night)pixels[y*v.width+x]=night_pixel(pixels[y*v.width+x]);
        }
      }else ++f->missing;
      lv_mem_free(rgba);
    }
  }
  snprintf(f->status,sizeof(f->status),"z%u / %u tiles / %u missing%s",v.zoom,f->loaded,f->missing,!esp_sdcard_port_get_size()?" / SD unavailable":waiting_clock?" / clock sync":cache_full?" / cache reserve":"");last_render_ms=milliseconds()-begun;return f;
}
static void worker(void *){
  uint64_t previous_source=0;
  for(;;){
#if MCX_MEMORY_DIAGNOSTICS
    mcxmem::observe(mcxmem::MAP);
#endif
    portENTER_CRITICAL(&mux);const Config c=config;const bool work=queued&&!ready&&!mcxpriority::ble_initializing.load();View v=requested;if(work)queued=false;portEXIT_CRITICAL(&mux);
    if(work){mcxpriority::map_busy=true;
      if(mcxpriority::ble_initializing.load()){mcxpriority::map_busy=false;continue;}
      if(source_hash(c)!=previous_source){clear_tiles();for(auto &miss:misses)miss=Miss{};backing_off=false;previous_source=source_hash(c);}
      mcxmem::capture(10);auto *f=render(v,c);mcxmem::observe(mcxmem::MAP);mcxmem::capture(11);if(f&&wanted.load()==v.generation){portENTER_CRITICAL(&mux);ready=f;portEXIT_CRITICAL(&mux);}else release(f);mcxpriority::map_busy=false;}
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
static bool start(){
  if(task)return true;
  load_config();
  const auto result=xTaskCreatePinnedToCoreWithCaps(worker,"mcx_map",MAP_WORKER_STACK_BYTES,nullptr,1,&task,0,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(result!=pdPASS){
    task=nullptr;last_error=ESP_ERR_NO_MEM;
    printf("[MCX-MAP] worker create failed rc=%ld stack=%lu internal_free=%lu internal_largest=%lu psram_free=%lu psram_largest=%lu\\n",(long)result,(unsigned long)MAP_WORKER_STACK_BYTES,(unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    return false;
  }
  last_error=ESP_OK;
  printf("[MCX-MAP] worker started stack=PSRAM/%lu TCB=internal internal_free=%lu psram_free=%lu\\n",(unsigned long)MAP_WORKER_STACK_BYTES,(unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  return true;
}
static uint32_t request(View v){
  if(v.zoom<3||v.zoom>19||v.width<64||v.width>480||v.height<64||v.height>286||!isfinite(v.latitude)||!isfinite(v.longitude))return 0;
  if(!start())return 0;
  v.generation=wanted.fetch_add(1)+1;
  portENTER_CRITICAL(&mux);requested=v;queued=true;portEXIT_CRITICAL(&mux);return v.generation;
}
static Frame *take(){portENTER_CRITICAL(&mux);auto *f=ready;ready=nullptr;portEXIT_CRITICAL(&mux);return f;}
static void stop(){wanted.fetch_add(1);portENTER_CRITICAL(&mux);queued=false;portEXIT_CRITICAL(&mux);}
}
