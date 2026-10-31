#include "meshcrossway_navigation_store.h"
#include <cassert>
#include <string>
#include <map>
#include <memory>
#include <limits>
using esp_err_t=int;
#define ESP_OK 0
#define ESP_FAIL -1
#define HTTPD_RESP_USE_STRLEN -1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static int fail_after=-1,allocations=0;
static void *heap_caps_malloc(size_t n,int caps){assert(caps==3);if(fail_after==0)return nullptr;if(fail_after>0)--fail_after;void *p=malloc(n);if(p)++allocations;return p;}
static void heap_caps_free(void *p){if(p){--allocations;free(p);}}
struct httpd_req_t{unsigned content_len=0;std::string input,output,code="200 OK",query;};
static int httpd_resp_set_status(httpd_req_t *r,const char *s){r->code=s;return 0;}
static int httpd_resp_set_type(httpd_req_t *,const char *){return 0;}
static int httpd_resp_set_hdr(httpd_req_t *,const char *,const char *){return 0;}
static int httpd_resp_send(httpd_req_t *r,const char *s,int n){r->output.assign(s,n<0?strlen(s):size_t(n));return 0;}
static int httpd_resp_send_chunk(httpd_req_t *r,const char *s,size_t n){if(s)r->output.append(s,n);return 0;}
static int httpd_req_get_hdr_value_str(httpd_req_t *,const char *,char *,size_t){return -1;}
static int httpd_req_recv(httpd_req_t *,char *,size_t){return -1;}
static int httpd_req_get_url_query_str(httpd_req_t *,char *,size_t){return -1;}
static int httpd_query_key_value(const char *,const char *,char *,size_t){return -1;}
namespace mcxnav{
static std::unique_ptr<Snapshot> current(new Snapshot);
static bool available=true;
static bool snapshot(Snapshot &out){if(!available)return false;out=*current;return true;}
static bool enqueue(const Command &){return true;}
}
#include "navigation_http_under_test.h"
int main(int argc,char **argv){
 assert(argc==2);using namespace mcxnav;
 // Exercise the real SD folder catalog, then read it through actual HTTP JSON.
 auto store=std::make_unique<Store>();assert(store->load(argv[1]));assert(store->create_folder("Weekend \"hikes\""));
 const auto id=store->folders.folders[0].id;assert(id);store.reset(new Store);assert(store->load(argv[1]));assert(store->folder(id));
 memcpy(current->folders,store->folders.folders,sizeof(current->folders));
 current->track_count=MAX_TRACKS;
 for(unsigned i=0;i<MAX_TRACKS;++i){auto &t=current->tracks[i];t.id=i+1;t.folder=id;t.metres=1234.5674+i;t.points=UINT32_MAX;memset(t.name,'x',39);t.name[39]=0;}
 strcpy(current->tracks[0].name,"Trilha \"á\"\\\n");
 httpd_req_t reply;mcxnavhttp::state(&reply);assert(reply.code=="200 OK"&&allocations==0);puts(reply.output.c_str());
 const auto expected=reply.output;
 for(unsigned i=0;i<200;++i){reply={};mcxnavhttp::state(&reply);assert(reply.output==expected&&allocations==0);}
 for(int fail=0;fail<2;++fail){fail_after=fail;reply={};mcxnavhttp::state(&reply);assert(reply.code=="503 Service Unavailable"&&allocations==0);}fail_after=-1;
 available=false;reply={};mcxnavhttp::state(&reply);assert(reply.code=="503 Service Unavailable"&&allocations==0);available=true;
 // All sixteen folders and 32 long track names must fit the fixed response.
 for(unsigned i=1;i<MAX_FOLDERS;++i){current->folders[i].id=i+100;memset(current->folders[i].name,'"',31);current->folders[i].name[31]=0;}
 reply={};mcxnavhttp::state(&reply);assert(reply.code=="200 OK");puts(reply.output.c_str());
 httpd_req_t formatted;{mcxnavhttp::Json j(1024);j.add("[");j.fixed(-104.90000005,7);j.add(",");j.fixed(39.90000004,7);j.add(",");j.fixed(-0.0,7);j.add(",");j.fixed(1.9999,3);j.add(",");j.fixed(std::numeric_limits<double>::infinity(),3);j.add(",");j.fixed(std::numeric_limits<double>::quiet_NaN(),3);j.add(",");j.fixed(1e100,3);j.add(",");j.integer(UINT64_MAX);j.add("]");j.send(&formatted);}puts(formatted.output.c_str());assert(allocations==0);
 httpd_req_t small;{mcxnavhttp::Json j(8);j.text("too long to fit");j.send(&small);}assert(small.code=="503 Service Unavailable"&&allocations==0);
 assert(store->rename_folder(id,"Renamed"));assert(store->delete_folder(id));store.reset(new Store);assert(store->load(argv[1])&&!store->folder(id));
 memcpy(current->folders,store->folders.folders,sizeof(current->folders));reply={};mcxnavhttp::state(&reply);assert(reply.output.find("Weekend")==std::string::npos&&allocations==0);
}
