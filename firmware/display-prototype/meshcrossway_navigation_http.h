#pragma once
#include "meshcrossway_navigation.h"
#include <string>
#include <cmath>
namespace mcxnavhttp {
  static std::string quote(const char *s){
    std::string o="\"";
    for(;*s;++s){
      unsigned char c=*s;
      if(c=='"'||c=='\\'){
        o+='\\';
        o+=char(c);
      }
      else if(c>=32)o+=char(c);
    }
    return o+'"';
  }
  static esp_err_t fail(httpd_req_t *r,const char *code,const char *text){
    httpd_resp_set_status(r,code);
    return httpd_resp_send(r,text,HTTPD_RESP_USE_STRLEN);
  }
  // Bounded PSRAM JSON; avoid chained string temporaries and libstdc++'s
  // floating to_string() alloca + newlib float formatter on the HTTP stack.
  class Json {
    char *buffer;size_t capacity,used=0;bool valid=true;
  public:
    explicit Json(size_t size):buffer((char*)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),capacity(size){if(!buffer)valid=false;}
    ~Json(){heap_caps_free(buffer);}
    Json(const Json&)=delete;Json& operator=(const Json&)=delete;
    void add(const char *text){const size_t length=strlen(text);if(!valid||length>=capacity-used){valid=false;return;}memcpy(buffer+used,text,length);used+=length;buffer[used]=0;}
    void integer(uint64_t value){char text[21];unsigned length=0;do{text[length++]=char('0'+value%10);value/=10;}while(value);while(length){char digit[2]={text[--length],0};add(digit);}}
    void fixed(double value,unsigned decimals){
      if(!std::isfinite(value)||std::fabs(value)>1e12||decimals>7){add("null");return;}
      uint64_t scale=1;for(unsigned i=0;i<decimals;++i)scale*=10;
      const uint64_t rounded=uint64_t(std::fabs(value)*double(scale)+0.5);
      if(value<0&&rounded)add("-");
      integer(rounded/scale);
      if(decimals){add(".");uint64_t fraction=rounded%scale;for(uint64_t place=scale/10;place;place/=10){char digit[2]={char('0'+fraction/place),0};add(digit);fraction%=place;}}
    }
    void text(const char *text){add("\"");for(const unsigned char *p=(const unsigned char*)text;*p;++p){char escaped[7]={};if(*p=='"'||*p=='\\'){escaped[0]='\\';escaped[1]=char(*p);}else if(*p<32){static const char hex[]="0123456789abcdef";memcpy(escaped,"\\u00",4);escaped[4]=hex[*p>>4];escaped[5]=hex[*p&15];}else escaped[0]=char(*p);add(escaped);}add("\"");}
    esp_err_t send(httpd_req_t *r){if(!valid)return fail(r,"503 Service Unavailable","Navigation response memory limit");httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,buffer,used);}
  };
  static esp_err_t state(httpd_req_t *r){
    auto *s=(mcxnav::Snapshot *)heap_caps_malloc(sizeof(mcxnav::Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s)return fail(r,"503 Service Unavailable","Memory unavailable");
    new(s) mcxnav::Snapshot();
    if(!mcxnav::snapshot(*s)){s->~Snapshot();heap_caps_free(s);return fail(r,"503 Service Unavailable","Navigation busy");}
    Json body(32*1024);
    body.add("{\"revision\":");body.integer(s->revision);body.add(",\"mode\":");body.integer(s->mode);body.add(",\"current\":");body.integer(s->current);body.add(",\"status\":");body.text(s->status);body.add(",\"selected\":");body.integer(s->preferences.selected);body.add(",\"presets\":[");
    for(unsigned i=0;i<4;++i){if(i)body.add(",");const auto &p=s->preferences.presets[i];body.add("{\"name\":");body.text(p.name);body.add(",\"distance\":");body.integer(p.distance_m);body.add(",\"interval\":");body.integer(p.interval_s);body.add("}");}
    body.add("],\"tracks\":[");
    for(unsigned i=0;i<s->track_count;++i){
      if(i){body.add(",");}
      const auto &t=s->tracks[i];uint32_t color=0xff8a24;bool visible=false;
      for(const auto &v:s->preferences.styles)if(v.id==t.id){color=v.color;visible=v.visible;}
      body.add("{\"id\":");body.integer(t.id);body.add(",\"name\":");body.text(t.name);body.add(",\"folder\":");body.integer(t.folder);body.add(",\"points\":");body.integer(t.points);body.add(",\"metres\":");body.fixed(t.metres,3);body.add(",\"gpx\":");body.add(t.gpx?"true":"false");body.add(",\"visible\":");body.add(visible?"true":"false");body.add(",\"color\":");body.integer(color);body.add("}");
    }
    body.add("],\"folders_ready\":");body.add(s->folders_ready?"true":"false");body.add(",\"folders\":[");bool first=true;
    for(const auto &f:s->folders)if(f.id){if(!first)body.add(",");first=false;body.add("{\"id\":");body.integer(f.id);body.add(",\"name\":");body.text(f.name);body.add("}");}
    body.add("]}");
    s->~Snapshot();heap_caps_free(s);
    return body.send(r);
  }
  static bool number(const char *s,uint32_t &v){
    if(!s||!*s)return false;
    uint64_t n=0;
    for(;*s;++s){
      if(*s<'0'||*s>'9')return false;
      n=n*10+unsigned(*s-'0');
      if(n>0xffffffffu)return false;
    }
    v=uint32_t(n);
    return true;
  }
  // A required non-simple header keeps browser cross-origin forms from issuing
  // state changes. No CORS headers are emitted;
  // LVGL is never called by HTTP.
  static esp_err_t command(httpd_req_t *r){
    char header[8];
    if(httpd_req_get_hdr_value_str(r,"X-MCX-Request",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return fail(r,"403 Forbidden","Local application header required");
    if(!r->content_len||r->content_len>240)return fail(r,"400 Bad Request","Invalid command length");
    char data[241];
    unsigned used=0;
    while(used<r->content_len){
      int n=httpd_req_recv(r,data+used,r->content_len-used);
      if(n<=0)return fail(r,"408 Request Timeout","Incomplete command");
      used+=unsigned(n);
    }
    data[used]=0;
    // Fixed ASCII field protocol: action|id|value|distance|interval|name.
    char *fields[6];
    fields[0]=data;
    unsigned count=1;
    for(unsigned i=0;i<used;++i)if(data[i]=='|'){
      if(count==6)return fail(r,"400 Bad Request","Too many fields");
      data[i]=0;
      fields[count++]=data+i+1;
    }
    if(count!=6)return fail(r,"400 Bad Request","Missing fields");
    uint32_t action=0;
    mcxnav::Command c;
    if(!number(fields[0],action)||!number(fields[1],c.id)||!number(fields[2],c.value)||!number(fields[3],c.distance_m)||!number(fields[4],c.interval_s))return fail(r,"400 Bad Request","Invalid number");
    if(action!=mcxnav::START&&action!=mcxnav::PAUSE&&action!=mcxnav::RESUME&&action!=mcxnav::STOP&&action!=mcxnav::SET_VISIBLE&&action!=mcxnav::SET_COLOR&&action!=mcxnav::SELECT_PRESET&&action!=mcxnav::SAVE_PRESET&&action!=mcxnav::EXPORT_TRACK&&action!=mcxnav::RENAME_TRACK&&action!=mcxnav::DELETE_TRACK&&action!=mcxnav::START_CONFIGURED&&action!=mcxnav::CREATE_FOLDER&&action!=mcxnav::RENAME_FOLDER&&action!=mcxnav::DELETE_FOLDER&&action!=mcxnav::MOVE_TRACK)return fail(r,"400 Bad Request","Unsupported action");
    if(strlen(fields[5])>((action==mcxnav::RENAME_TRACK||action==mcxnav::STOP)?39u:(action==mcxnav::CREATE_FOLDER||action==mcxnav::RENAME_FOLDER)?31u:23u))return fail(r,"400 Bad Request","Name too long");
    for(const unsigned char *p=(const unsigned char *)fields[5];*p;++p)if(*p<32||(action==mcxnav::SAVE_PRESET&&*p>126))return fail(r,"400 Bad Request","Invalid name characters");
    if((action==mcxnav::CREATE_FOLDER||action==mcxnav::RENAME_FOLDER)&&!mcxnav::Store::folder_name_valid(fields[5]))return fail(r,"400 Bad Request","Folder name: 1-31 bytes; no slashes or separators");
    if((action==mcxnav::RENAME_FOLDER||action==mcxnav::DELETE_FOLDER||action==mcxnav::MOVE_TRACK)&&!c.id)return fail(r,"400 Bad Request","Invalid folder or track ID");
    c.action=mcxnav::Action(action);
    snprintf(c.name,sizeof(c.name),"%s",fields[5]);
    if((c.action==mcxnav::SET_COLOR&&c.value>0xffffff)||(c.action==mcxnav::SET_VISIBLE&&c.value>1)||(c.action==mcxnav::SELECT_PRESET&&c.id>=4)||(c.action==mcxnav::SAVE_PRESET&&(c.id>=4||!c.name[0]||c.distance_m<3||c.distance_m>1000||c.interval_s<1||c.interval_s>300)))return fail(r,"400 Bad Request","Settings outside supported range");
    if(c.action==mcxnav::START_CONFIGURED&&(c.id>4||c.value>0xffffff||(c.id==4&&(c.distance_m<3||c.distance_m>1000||c.interval_s<1||c.interval_s>300))))return fail(r,"400 Bad Request","Invalid recording mode");
    if((c.action==mcxnav::RENAME_TRACK||c.action==mcxnav::DELETE_TRACK||c.action==mcxnav::SET_COLOR||c.action==mcxnav::SET_VISIBLE||c.action==mcxnav::EXPORT_TRACK)&&!c.id)return fail(r,"400 Bad Request","Invalid track ID");
    if(!mcxnav::enqueue(c))return fail(r,"503 Service Unavailable","Navigation command queue busy");
    httpd_resp_set_status(r,"202 Accepted");
    return httpd_resp_send(r,"Command queued; check device status",HTTPD_RESP_USE_STRLEN);
  }
  static esp_err_t geometry_from(httpd_req_t *r,const char *root){
    char query[48],id_text[16];uint32_t id=0;
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"id",id_text,sizeof(id_text))!=ESP_OK||!number(id_text,id)||!id)return fail(r,"400 Bad Request","Invalid track ID");
    auto *s=(mcxnav::Snapshot*)heap_caps_malloc(sizeof(mcxnav::Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s){return fail(r,"503 Service Unavailable","Memory unavailable");}
    new(s) mcxnav::Snapshot();
    bool found=false,active=false;uint32_t total=0;
    if(mcxnav::snapshot(*s)){active=s->current==id&&(s->mode==mcxnav::RECORDING||s->mode==mcxnav::PAUSED);for(unsigned i=0;i<s->track_count;++i)if(s->tracks[i].id==id){found=true;total=s->tracks[i].points;}}
    s->~Snapshot();heap_caps_free(s);
    if(!found)return fail(r,"404 Not Found","Track unavailable");
    if(active)return fail(r,"409 Conflict","Finish recording before previewing this track");
    auto *g=(mcxnav::Geometry*)heap_caps_malloc(sizeof(mcxnav::Geometry),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!g){return fail(r,"503 Service Unavailable","Memory unavailable");}
    new(g) mcxnav::Geometry();
    if(!mcxnav::read_geometry(root,id,total,*g)){g->~Geometry();heap_caps_free(g);return fail(r,"404 Not Found","Track geometry unavailable or damaged");}
    Json json(96*1024);
    json.add("{\"id\":");json.integer(id);json.add(",\"total\":");json.integer(total);json.add(",\"simplified\":");json.add(g->count<total?"true":"false");json.add(",\"points\":[");
    for(unsigned i=0;i<g->count;++i){const auto &p=g->points[i];if(i)json.add(",");json.add("[");json.fixed(p.latitude,7);json.add(",");json.fixed(p.longitude,7);json.add(",");json.integer(p.segment);json.add("]");}json.add("]}");
    g->~Geometry();heap_caps_free(g);
    return json.send(r);
  }
  static esp_err_t geometry(httpd_req_t *r){return geometry_from(r,"/sdcard/meshcrossway/navigation");}
  static esp_err_t download(httpd_req_t *r){
    char query[48],id_text[16];
    uint32_t id=0;
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"id",id_text,sizeof(id_text))!=ESP_OK||!number(id_text,id)||!id)return fail(r,"400 Bad Request","Invalid track ID");
    char file[128];
    snprintf(file,sizeof(file),"/sdcard/meshcrossway/navigation/%08lx.gpx",(unsigned long)id);
    char marker[128];snprintf(marker,sizeof(marker),"/sdcard/meshcrossway/navigation/%08lx.del",(unsigned long)id);struct stat deleted={};if(stat(marker,&deleted)==0)return fail(r,"404 Not Found","Track deleted");
    FILE *f=fopen(file,"rb");
    if(!f)return fail(r,"404 Not Found","GPX unavailable; finish or export the track first");
    httpd_resp_set_type(r,"application/gpx+xml");
    char disposition[96];
    snprintf(disposition,sizeof(disposition),"attachment; filename=track-%08lx.gpx",(unsigned long)id);
    httpd_resp_set_hdr(r,"Content-Disposition",disposition);
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    char block[1024];
    size_t n;
    esp_err_t result=ESP_OK;
    while((n=fread(block,1,sizeof(block),f))&&result==ESP_OK)result=httpd_resp_send_chunk(r,block,n);
    if(ferror(f))result=ESP_FAIL;
    fclose(f);
    if(result==ESP_OK)result=httpd_resp_send_chunk(r,nullptr,0);
    return result;
  }
}
