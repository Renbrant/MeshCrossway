#pragma once
#include "meshcrossway_navigation.h"
#include <string>
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
  static esp_err_t state(httpd_req_t *r){
    auto *s=(mcxnav::Snapshot *)heap_caps_malloc(sizeof(mcxnav::Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s)return fail(r,"503 Service Unavailable","Memory unavailable");
    new(s) mcxnav::Snapshot();
    if(!mcxnav::snapshot(*s)){
      s->~Snapshot();
      heap_caps_free(s);
      return fail(r,"503 Service Unavailable","Navigation busy");
    }
    std::string body="{\"revision\":"+std::to_string(s->revision)+",\"mode\":"+std::to_string(s->mode)+",\"current\":"+std::to_string(s->current)+",\"status\":"+quote(s->status)+",\"selected\":"+std::to_string(s->preferences.selected)+",\"presets\":[";
    for(unsigned i=0;i<4;++i){
      if(i)body+=',';
      auto &p=s->preferences.presets[i];
      body+="{\"name\":"+quote(p.name)+",\"distance\":"+std::to_string(p.distance_m)+",\"interval\":"+std::to_string(p.interval_s)+"}";
    }
    body+="],\"tracks\":[";
    for(unsigned i=0;i<s->track_count;++i){
      if(i)body+=',';
      const auto &t=s->tracks[i];
      uint32_t color=0xff8a24;
      bool visible=false;
      for(const auto &v:s->preferences.styles)if(v.id==t.id){
        color=v.color;
        visible=v.visible;
      }
      body+="{\"id\":"+std::to_string(t.id)+",\"points\":"+std::to_string(t.points)+",\"metres\":"+std::to_string(t.metres)+",\"gpx\":"+(t.gpx?"true":"false")+",\"visible\":"+(visible?"true":"false")+",\"color\":"+std::to_string(color)+"}";
    }
    body+="]}";
    s->~Snapshot();
    heap_caps_free(s);
    httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,body.c_str(),body.size());
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
    if(action!=mcxnav::START&&action!=mcxnav::PAUSE&&action!=mcxnav::RESUME&&action!=mcxnav::STOP&&action!=mcxnav::SET_VISIBLE&&action!=mcxnav::SET_COLOR&&action!=mcxnav::SELECT_PRESET&&action!=mcxnav::SAVE_PRESET&&action!=mcxnav::EXPORT_TRACK)return fail(r,"400 Bad Request","Unsupported action");
    if(strlen(fields[5])>23)return fail(r,"400 Bad Request","Name too long");
    for(char *p=fields[5];*p;++p)if(*p<32||*p>126)return fail(r,"400 Bad Request","Use ASCII preset names");
    c.action=mcxnav::Action(action);
    snprintf(c.name,sizeof(c.name),"%s",fields[5]);
    if((c.action==mcxnav::SET_COLOR&&c.value>0xffffff)||(c.action==mcxnav::SET_VISIBLE&&c.value>1)||(c.action==mcxnav::SELECT_PRESET&&c.id>=4)||(c.action==mcxnav::SAVE_PRESET&&(c.id>=4||!c.name[0]||c.distance_m<3||c.distance_m>1000||c.interval_s<1||c.interval_s>300)))return fail(r,"400 Bad Request","Settings outside supported range");
    if(!mcxnav::enqueue(c))return fail(r,"503 Service Unavailable","Navigation command queue busy");
    httpd_resp_set_status(r,"202 Accepted");
    return httpd_resp_send(r,"Command queued; check device status",HTTPD_RESP_USE_STRLEN);
  }
  static esp_err_t download(httpd_req_t *r){
    char query[48],id_text[16];
    uint32_t id=0;
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"id",id_text,sizeof(id_text))!=ESP_OK||!number(id_text,id)||!id)return fail(r,"400 Bad Request","Invalid track ID");
    char file[128];
    snprintf(file,sizeof(file),"/sdcard/meshcrossway/navigation/%08lx.gpx",(unsigned long)id);
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
