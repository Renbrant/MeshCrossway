#include "meshcrossway_conversations.h"
#include "meshcrossway_chat_presentation.h"
#include "meshcrossway_meshtastic_protocol.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <string>
#include <openssl/sha.h>
static bool memory_ok=true;
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static void *heap_caps_malloc(size_t n,int caps){assert(caps==3);return memory_ok?malloc(n):nullptr;}
static void esp_fill_random(void *p,size_t n){memset(p,0xa5,n);}
static uint32_t esp_random(){static uint32_t n=100;return ++n;}
static uint64_t esp_timer_get_time(){return 1000000;}
static int mbedtls_sha256(const unsigned char *p,size_t n,unsigned char out[32],int){SHA256(p,n,out);return 0;}
namespace meshmc {static Client *client=nullptr;static std::atomic<bool> link{false};}
namespace meshmt {static Client *client=nullptr;static std::atomic<bool> link{false};}
namespace mcxhistory {static mcxchat::Local local;static mcxchat::Local &preferences(){return local;}}
#include "remote_model.h"
using esp_err_t=int;
static constexpr int ESP_OK=0;
#define HTTPD_RESP_USE_STRLEN -1
struct httpd_req_t {std::string query,body,header,response,status;int content_len=0,pos=0;};
static int httpd_resp_set_status(httpd_req_t *r,const char *s){r->status=s;return 0;}
static int httpd_resp_set_type(httpd_req_t *,const char *){return 0;}
static int httpd_resp_set_hdr(httpd_req_t *,const char *,const char *){return 0;}
static int httpd_resp_send(httpd_req_t *r,const char *p,int n){r->response.assign(p,n<0?strlen(p):size_t(n));return 0;}
static int copy(const std::string &s,char *out,size_t n){if(s.size()>=n)return -1;strcpy(out,s.c_str());return 0;}
static int httpd_req_get_url_query_str(httpd_req_t *r,char *out,size_t n){return copy(r->query,out,n);}
static int httpd_query_key_value(const char *q,const char *key,char *out,size_t n){std::string s=q,k=std::string(key)+'=';auto at=s.find(k);if(at==s.npos)return -1;auto end=s.find('&',at);return copy(s.substr(at+k.size(),end==s.npos?end:end-at-k.size()),out,n);}
static int httpd_req_get_hdr_value_str(httpd_req_t *r,const char *,char *out,size_t n){return r->header.empty()?-1:copy(r->header,out,n);}
static int httpd_req_recv(httpd_req_t *r,char *out,int n){int take=std::min(n,int(r->body.size())-r->pos);if(take<=0)return -1;memcpy(out,r->body.data()+r->pos,take);r->pos+=take;return take;}
static bool owner_poll=true;
static void vTaskDelay(unsigned){if(owner_poll)mcxremotechat::poll();}
#define pdMS_TO_TICKS(n) (n)
#include "remote_http.h"
using namespace mcxremotechat;
static std::string snapshot(const char *network,const char *target="",unsigned page=0,unsigned category=0){Request r;strcpy(r.network,network);strcpy(r.target,target);r.page=page;r.category=category;work->request=r;phase=1;poll();assert(phase==3);auto out=std::string(work->response,work->length);phase=0;return out;}
static httpd_req_t submit(const std::string &net,const std::string &target,const std::string &text,const std::string &header="1"){
 httpd_req_t r;r.body=net+'|'+target+'|'+text;r.content_len=r.body.size();r.header=header;send(&r);return r;
}
int main(){
 setenv("TZ","MST7MDT,M3.2.0,M11.1.0",1);tzset();
 memory_ok=false;assert(!init());memory_ok=true;assert(init());
 assert(snapshot("mc").find("Connect this network")!=std::string::npos);
 auto cp=std::make_unique<meshmc::Client>();auto mp=std::make_unique<meshmt::Client>();auto &c=*cp;auto &m=*mp;
 meshmc::client=&c;meshmt::client=&m;meshmc::link=true;meshmt::link=true;
 c.connected=c.initialized=c.history_ready=true;c.payload_limit=173;c.clock=1800000000;c.protocol=14;c.public_key[0]=1;c.contact_count=2;
 c.contacts[0].key[0]=9;strcpy(c.contacts[0].name,"Alice <script>");c.contacts[1].key[0]=10;strcpy(c.contacts[1].name,"Bob");
 c.channel_count=1;c.channels[0].loaded=true;strcpy(c.channels[0].name,"Trail");memcpy(c.channels[0].secret,"private-channel!",16);
 auto k=mcxchat::key_for(c,false,0);char alice[65];mc_token(k,alice);char repeated[65];mc_token(k,repeated);assert(!strcmp(alice,repeated));
 auto &rx=c.append();rx.timestamp=1704067200u;rx.observed_at=1704070800u;memcpy(rx.key,c.contacts[0].key,6);strcpy(rx.text,"Olá \"friend\" | 😀\n<script>alert(1)</script>");
 auto &bob=c.append();memcpy(bob.key,c.contacts[1].key,6);strcpy(bob.text,"Bob private text");
 auto body=snapshot("mc",alice);assert(body.find("Olá")!=body.npos&&body.find("Bob private text")==body.npos&&body.find("private-channel!")==body.npos);puts(body.c_str());assert(rx.read&&!bob.read);assert(body.find("2023-12-31 17:00 MST")!=body.npos);
 auto groups=snapshot("mc","",0,2);assert(groups.find("Trail")!=groups.npos&&groups.find("Alice")==groups.npos);
 auto nodes=snapshot("mc","",0,3);assert(nodes.find("Alice")!=nodes.npos&&nodes.find("Trail")==nodes.npos);
 auto chats=snapshot("mc","",0,1);assert(chats.find("Alice")!=chats.npos&&chats.find("Trail")==chats.npos);
 auto bad=submit("mc",alice,"Hi","");assert(bad.status=="403 Forbidden"&&c.qcount==0);
 bad=submit("xx",alice,"Hi");assert(bad.status=="400 Bad Request");
 bad=submit("mc",alice,std::string(151,'x'));assert(bad.status=="409 Conflict"&&c.qcount==0);
 owner_poll=false;bad=submit("mc",alice,"Not sent on timeout");assert(bad.status=="503 Service Unavailable"&&phase==0&&c.qcount==0);owner_poll=true;poll();assert(c.qcount==0);
 auto ok=submit("mc",alice,"Web reply | UTF-8 😀");assert(ok.status=="202 Accepted"&&c.qcount==1&&m.data.message_count==0);puts(ok.response.c_str());
 bad=submit("mt",alice,"Do not cross networks");assert(bad.status=="409 Conflict"&&c.qcount==1);
 meshmc::link=false;bad=submit("mc",alice,"Offline");assert(bad.status=="409 Conflict"&&c.qcount==1);meshmc::link=true;
 c.public_key[1]=1;bad=submit("mc",alice,"Other radio");assert(bad.status=="409 Conflict");c.public_key[1]=0;
 mcxhistory::local.get(k,true)->blocked=true;bad=submit("mc",alice,"Blocked");assert(bad.status=="409 Conflict");mcxhistory::local.get(k)->blocked=false;
 auto channel=mcxchat::key_for(c,true,0);char group[65];mc_token(channel,group);c.channels[0].secret[0]^=1;bad=submit("mc",group,"Changed channel");assert(bad.status=="409 Conflict");
 m.connected=m.initialized=m.history_ready=m.tx_enabled=true;m.region=1;m.mtu=517;memcpy(m.data.radio,"1234567",7);m.data.my_num=42;m.data.channels[0].loaded=true;m.data.channels[0].role=1;m.data.channels[0].identity=123;strcpy(m.data.channels[0].name,"Adventure");
 auto *node=m.node(99,true);strcpy(node->name,"Trail peer");auto mt=m.target(99,0);char peer[65];mt_token(mt,peer);
 auto &incoming=m.append();incoming.time=1719792000u;incoming.from=99;incoming.to=42;incoming.channel=0;incoming.channel_identity=123;strcpy(incoming.text,"Meshtastic incoming");
 auto &foreign=m.append();foreign.from=99;foreign.to=88;foreign.channel=0;foreign.channel_identity=123;strcpy(foreign.text,"Not our DM");
 body=snapshot("mt",peer);assert(body.find("Meshtastic incoming")!=body.npos&&body.find("Not our DM")==body.npos);assert(body.find("2024-06-30 18:00 MDT")!=body.npos);puts(body.c_str());
 ok=submit("mt",peer,"Meshtastic reply");assert(ok.status=="202 Accepted"&&m.data.message_count==3&&c.qcount==1);
 bad=submit("mc",peer,"Do not cross networks");assert(bad.status=="409 Conflict"&&c.qcount==1);
 m.data.channels[0].identity++;bad=submit("mt",peer,"Old channel");assert(bad.status=="409 Conflict");
 httpd_req_t query;query.query="network=mc&page=999";state(&query);assert(query.status=="400 Bad Request");
 query={};query.query="network=mc&target="+std::string(70,'a');state(&query);assert(query.status=="400 Bad Request");
 query={};query.query="network=mc&page=0";state(&query);assert(query.status=="200 OK");
 // All supported targets are reachable in bounded pages.
 c.contact_count=meshmc::MAX_CONTACTS;c.channel_count=meshmc::MAX_CHANNELS;
 for(unsigned i=0;i<c.channel_count;++i){c.channels[i].loaded=true;snprintf(c.channels[i].name,33,"Channel %u",i);}
 body=snapshot("mc","",23);assert(body.find("\"total\":768")!=body.npos&&body.find("\"page\":23")!=body.npos);
 // Worst-case escaped message content remains a complete bounded response.
 for(auto &msg:c.messages){msg={};msg.id=100+unsigned(&msg-c.messages);memcpy(msg.key,k.native,6);memset(msg.text,'\x01',176);msg.text[176]=0;}
 body=snapshot("mc",alice);assert(work->code==200&&work->length<RESPONSE_SIZE);puts(body.c_str());
 return 0;
}
