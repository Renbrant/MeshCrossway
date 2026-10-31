#include "lvgl.h"
#include "meshcrossway_navigation.h"
#include <cassert>
#include <filesystem>
#include <map>
#include <string>
using esp_err_t=int;
#define ESP_FAIL -1
#define HTTPD_RESP_USE_STRLEN -1
struct httpd_req_t {unsigned content_len=0;std::string input,output,code="200 OK",query;std::map<std::string,std::string> headers;};
static int httpd_resp_set_status(httpd_req_t *r,const char *s){r->code=s;return 0;}
static int httpd_resp_set_type(httpd_req_t *,const char *){return 0;}
static int httpd_resp_set_hdr(httpd_req_t *,const char *,const char *){return 0;}
static int httpd_resp_send(httpd_req_t *r,const char *s,int n){r->output.assign(s,n<0?strlen(s):size_t(n));return 0;}
static int httpd_resp_send_chunk(httpd_req_t *r,const char *s,size_t n){if(s)r->output.append(s,n);return 0;}
static int httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t cap){auto p=r->headers.find(key);if(p==r->headers.end()||p->second.size()>=cap)return -1;snprintf(out,cap,"%s",p->second.c_str());return 0;}
static int httpd_req_recv(httpd_req_t *r,char *out,size_t cap){size_t n=std::min(cap,r->input.size());memcpy(out,r->input.data(),n);r->input.erase(0,n);return int(n);}
static int httpd_req_get_url_query_str(httpd_req_t *r,char *out,size_t cap){if(r->query.size()>=cap)return -1;snprintf(out,cap,"%s",r->query.c_str());return 0;}
static int httpd_query_key_value(const char *query,const char *key,char *out,size_t cap){std::string prefix=std::string(key)+"=";if(strncmp(query,prefix.c_str(),prefix.size()))return -1;const char *value=query+prefix.size();if(strlen(value)>=cap)return -1;snprintf(out,cap,"%s",value);return 0;}
#include "meshcrossway_navigation_http.h"
struct StopTick {};
static void step(){fake_delay_hook=[](unsigned){throw StopTick{};};try{mcxnav::worker(nullptr);}catch(StopTick&){}fake_delay_hook=nullptr;}
int main(int argc,char **argv){assert(argc==2);lv_init();std::filesystem::create_directories(argv[1]);// A failed internal stack allocation must leave a retryable service, not a
 // phantom running task or reset. PSRAM state survives the retry.
 fake_task_failure=true;assert(!mcxnav::start()&&!mcxnav::worker_task);
 auto *retained_store=mcxnav::store;assert(retained_store);fake_task_failure=false;
 assert(mcxnav::start()&&mcxnav::store==retained_store&&fake_task_stack==8192);
 const auto calls=fake_task_calls;assert(mcxnav::start()&&fake_task_calls==calls);
 assert(mcxnav::store->load(argv[1]));
 mcxgnss::latest.valid=true;mcxgnss::latest.latitude=39.9;mcxgnss::latest.longitude=-104.9;mcxgnss::latest.position_ms=lv_tick_get();
 assert(mcxnav::submit(mcxnav::START));step();assert(mcxnav::store->mode==mcxnav::RECORDING&&mcxnav::store->point_count==1);assert(mcxnav::stack_min.load()==2048);
 lv_tick_inc(2500);mcxgnss::latest.position_ms=lv_tick_get();mcxgnss::latest.latitude+=.0001;step();assert(mcxnav::store->point_count==2);
 const auto captured=mcxgnss::latest.latitude;assert(mcxnav::submit(mcxnav::SAVE_POI,0,"At button press"));mcxgnss::latest.latitude+=.01;step();assert(mcxnav::store->poi_count==1&&mcxnav::store->pois[0].point.latitude==captured);
 assert(mcxnav::submit(mcxnav::PAUSE));step();assert(mcxnav::store->mode==mcxnav::PAUSED);
 fake_free_space=1;assert(mcxnav::submit(mcxnav::RESUME));step();assert(mcxnav::store->mode==mcxnav::PAUSED);
 fake_free_space=1024ULL*1024*1024;assert(mcxnav::submit(mcxnav::STOP));step();assert(mcxnav::store->mode==mcxnav::STOPPED&&mcxnav::store->track_count==1&&mcxnav::store->tracks[0].gpx);
 auto *s=new mcxnav::Snapshot;assert(mcxnav::snapshot(*s)&&s->poi_count==1&&s->track_count==1);delete s;
 mcxgnss::settings.enabled=0;assert(!mcxnav::submit(mcxnav::SAVE_POI,0,"stale"));
 for(unsigned i=0;i<4;++i){assert(mcxnav::submit(mcxnav::NONE));}
 assert(!mcxnav::submit(mcxnav::NONE));step();assert(mcxnav::count==3);
 while(mcxnav::count)step();
 httpd_req_t state;assert(mcxnavhttp::state(&state)==0);assert(state.output.find("\"presets\"")!=std::string::npos&&state.output.find("\"tracks\"")!=std::string::npos);
 auto request=[](const char *body,bool header=true){httpd_req_t r;r.input=body;r.content_len=r.input.size();if(header)r.headers["X-MCX-Request"]="1";mcxnavhttp::command(&r);return r;};
 assert(request("10|0|0|0|0|",false).code=="403 Forbidden");
 assert(request("10|99|0|0|0|").code=="400 Bad Request");
 assert(request("11|3|0|0|2|Bad").code=="400 Bad Request");
 assert(request("11|3|0|12|4|Web preset").code=="202 Accepted");step();assert(mcxnav::store->preferences.presets[3].distance_m==12);
 assert(request("10|3|0|0|0|").code=="202 Accepted");step();assert(mcxnav::store->preferences.selected==3);
 assert(request("11|2|1|30|7|Device preset").code=="202 Accepted");step();assert(mcxnav::store->preferences.selected==2&&mcxnav::store->preferences.presets[2].distance_m==30);
 assert(request("999|0|0|0|0|").code=="400 Bad Request");
 httpd_req_t bad;bad.query="id=../../secret";mcxnavhttp::download(&bad);assert(bad.code=="400 Bad Request");
 const auto saved_id=mcxnav::store->tracks[0].id;
 assert(request("16|0|0|0|0|Hikes").code=="202 Accepted");step();const auto folder_id=mcxnav::store->folders.folders[0].id;assert(folder_id);
 assert(request("16|0|0|0|0|../bad").code=="400 Bad Request");
 std::string move_body="19|"+std::to_string(saved_id)+"|"+std::to_string(folder_id)+"|0|0|";assert(request(move_body.c_str()).code=="202 Accepted");step();assert(mcxnav::store->track_folder(saved_id)==folder_id);
 httpd_req_t grouped;mcxnavhttp::state(&grouped);assert(grouped.output.find("\"folders\"")!=std::string::npos&&grouped.output.find("Hikes")!=std::string::npos);
 httpd_req_t geometry;geometry.query="id="+std::to_string(saved_id);mcxnavhttp::geometry_from(&geometry,argv[1]);assert(geometry.code=="200 OK"&&geometry.output.find("\"points\":[[")!=std::string::npos);
 geometry={};geometry.query="id=4294967295";mcxnavhttp::geometry_from(&geometry,argv[1]);assert(geometry.code=="404 Not Found");

 std::string rename_body="13|"+std::to_string(saved_id)+"|0|0|0|Web named route";assert(request(rename_body.c_str()).code=="202 Accepted");step();assert(!strcmp(mcxnav::store->tracks[0].name,"Web named route"));
 assert(request("15|7|0|0|0|").code=="400 Bad Request");assert(request("15|4|1193046|20|5|").code=="202 Accepted");step();const auto running_id=mcxnav::store->current;httpd_req_t live;live.query="id="+std::to_string(running_id);mcxnavhttp::geometry_from(&live,argv[1]);assert(live.code=="409 Conflict");assert(mcxnav::store->mode==mcxnav::RECORDING&&mcxnav::store->recording_preset.distance_m==20&&mcxnav::store->style(running_id)->color==1193046);
 std::string active_delete="14|"+std::to_string(running_id)+"|0|0|0|";assert(request(active_delete.c_str()).code=="202 Accepted");step();assert(mcxnav::store->mode==mcxnav::RECORDING);
 assert(request("4|0|0|0|0|Finished name").code=="202 Accepted");step();assert(mcxnav::store->mode==mcxnav::STOPPED&&!strcmp(mcxnav::store->tracks[0].name,"Finished name"));
 std::string delete_body="14|"+std::to_string(saved_id)+"|0|0|0|";assert(request(delete_body.c_str()).code=="202 Accepted");step();for(unsigned i=0;i<mcxnav::store->track_count;++i)assert(mcxnav::store->tracks[i].id!=saved_id);
 puts("PASS: HTTP navigation state, command header, numeric bounds, presets via worker and download path validation");
 puts("PASS: production navigation worker, bounded command queue, button-time POI fix, stale-fix rejection, SD-space pause, snapshot and GPX completion");
}
