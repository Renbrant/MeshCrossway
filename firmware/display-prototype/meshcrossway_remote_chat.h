#pragma once
// One bounded PSRAM mailbox. HTTP never touches a protocol client; poll() runs
// on the existing LVGL owner task, beside both native protocol pollers.
#include "meshcrossway_meshcore.h"
#include "meshcrossway_meshtastic.h"
#include "meshcrossway_conversations.h"
#include "mbedtls/sha256.h"
#include <atomic>
#include <new>
namespace mcxremotechat {
static constexpr unsigned PAGE_SIZE=32, MESSAGE_LIMIT=40, RESPONSE_SIZE=96*1024;
struct Request {bool sending=false;char network[3]={},target[65]={},text[meshmt::MAX_TEXT+1]={};unsigned page=0;};
struct Workspace {Request request;char response[RESPONSE_SIZE];size_t length=0;int code=200;uint8_t salt[16]={};};
static Workspace *work=nullptr;
static std::atomic<unsigned> phase{0}; // 0 idle, 1 submitted, 2 processing, 3 complete
static bool init(){
  if(work)return true;
  void *p=heap_caps_malloc(sizeof(Workspace),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!p)return false;
  work=new(p) Workspace();esp_fill_random(work->salt,sizeof(work->salt));return true;
}
struct Json {
  char *data;size_t cap,used=0;bool ok=true;
  void add(const char *s){size_t n=strlen(s);if(n>=cap-used){ok=false;return;}memcpy(data+used,s,n);used+=n;data[used]=0;}
  void number(uint32_t n){char b[16];snprintf(b,sizeof(b),"%lu",(unsigned long)n);add(b);}
  void text(const char *s){add("\"");for(const unsigned char *p=(const unsigned char*)s;*p;++p){char b[7]={};
    if(*p=='"'||*p=='\\'){b[0]='\\';b[1]=char(*p);}else if(*p<32)snprintf(b,sizeof(b),"\\u%04x",*p);else b[0]=char(*p);add(b);
  }add("\"");}
};
// Per-boot salted opaque handles bind radio, protocol, destination and channel
// configuration. Never expose MeshCore channel secrets or accept list indices.
static void token(const char *network,const void *key,size_t length,char out[65]){
  uint8_t input[160]={},digest[32]={};
  memcpy(input,work->salt,16);memcpy(input+16,network,2);
  if(length>sizeof(input)-18){out[0]=0;return;}
  memcpy(input+18,key,length);
  if(mbedtls_sha256(input,length+18,digest,0)){out[0]=0;return;}
  static const char hex[]="0123456789abcdef";
  for(unsigned i=0;i<32;++i){out[2*i]=hex[digest[i]>>4];out[2*i+1]=hex[digest[i]&15];}out[64]=0;
}
static void mc_token(const mcxchat::Key &k,char out[65]){token("mc",&k,sizeof(k),out);}
static void mt_token(const meshmt::Target &t,char out[65]){
  // Target has padding; serialize fields explicitly to make handles stable.
  uint8_t b[20]={};memcpy(b,t.radio,7);memcpy(b+7,&t.node,4);b[11]=t.channel;memcpy(b+12,&t.channel_identity,8);token("mt",b,sizeof(b),out);
}
static const char *mc_delivery(meshmc::Delivery d){switch(d){case meshmc::QUEUED:return "Queued locally";case meshmc::ACCEPTED:return "Radio accepted";case meshmc::DELIVERED:return "Remote ACK";case meshmc::REJECTED:return "Rejected";case meshmc::UNCERTAIN:return "Unconfirmed";default:return "Received";}}
static bool mt_belongs(const meshmt::Message &m,const meshmt::Target &t,uint32_t self){
  return m.seq&&m.channel==t.channel&&m.channel_identity==t.channel_identity&&
    (t.node==meshmt::BROADCAST?m.to==meshmt::BROADCAST:(m.outgoing?m.to==t.node:(m.from==t.node&&m.to==self)));
}
static void result(const char *message,int code){
  Json j{work->response,sizeof(work->response)};j.add("{\"message\":");j.text(message);j.add("}");work->length=j.used;work->code=code;
}
static void process(){
  const Request &r=work->request;
  const bool mc=!strcmp(r.network,"mc");
  auto *c=meshmc::client;auto *m=meshmt::client;
  if((mc&&!c)||(!mc&&!m)){result("Connect this network's Bluetooth radio on the device first.",409);return;}
  Json j{work->response,sizeof(work->response)};
  mcxchat::Key mk;meshmt::Target mt;bool found=false;unsigned total=0,shown=0;
  const unsigned begin=r.page*PAGE_SIZE;
  const char *problem=nullptr;
  auto row=[&](const char *id,const char *name,bool group){
    unsigned pos=total++;
    if(r.sending||pos<begin||shown>=PAGE_SIZE)return;
    if(shown++)j.add(",");
    j.add("{\"id\":");j.text(id);j.add(",\"name\":");j.text(name);j.add(",\"kind\":");j.text(group?"Channel":"Direct");j.add("}");
  };
  j.add("{\"network\":");j.text(mc?"mc":"mt");j.add(",\"connected\":");j.add((mc?meshmc::link.load()&&c->connected:meshmt::link.load()&&m->connected)?"true":"false");
  j.add(",\"status\":");j.text(mc?c->status:m->status);j.add(",\"targets\":[");
  if(mc){
    for(unsigned i=0;i<c->channel_count;++i)if(c->channels[i].loaded&&c->channels[i].name[0]){
      auto k=mcxchat::key_for(*c,true,uint16_t(i));auto *pref=mcxhistory::preferences().get(k);if(pref&&pref->blocked)continue;char id[65];mc_token(k,id);row(id,c->channels[i].name,true);if(!strcmp(id,r.target)){mk=k;found=true;}
    }
    for(unsigned i=0;i<c->contact_count;++i){auto k=mcxchat::key_for(*c,false,uint16_t(i));
      auto *pref=mcxhistory::preferences().get(k);if(pref&&pref->blocked)continue;
      char id[65];mc_token(k,id);row(id,pref&&pref->alias[0]?pref->alias:c->contacts[i].name,false);if(!strcmp(id,r.target)){mk=k;found=true;}
    }
    if(found)problem=mcxchat::send_problem(*c,mk,r.sending?r.text:nullptr);
    if(!meshmc::link.load())problem="MeshCore radio disconnected. Reconnect to send.";
  }else{
    for(unsigned ch=0;ch<meshmt::MAX_CHANNELS;++ch)if(m->data.channels[ch].loaded&&m->data.channels[ch].role){
      auto t=m->target(meshmt::BROADCAST,uint8_t(ch));char id[65];mt_token(t,id);row(id,m->data.channels[ch].name[0]?m->data.channels[ch].name:"Primary channel",true);if(!strcmp(id,r.target)){mt=t;found=true;}
      // A DM is bound to its native channel, including secondary channels.
      for(unsigned i=0;i<m->data.node_count;++i){auto &n=m->data.nodes[i];if(n.num==m->data.my_num)continue;
        t=m->target(n.num,uint8_t(ch));mt_token(t,id);char name[104];snprintf(name,sizeof(name),"%.48s · %.48s",n.name[0]?n.name:"Unknown node",m->data.channels[ch].name);row(id,name,false);if(!strcmp(id,r.target)){mt=t;found=true;}
      }
    }
    if(found)problem=m->send_problem(mt,r.sending?r.text:nullptr);
    if(!meshmt::link.load())problem="Meshtastic radio disconnected. Reconnect to send.";
  }
  if(r.sending){
    if(!found){result("Conversation changed or belongs to another radio/network. Select it again.",409);return;}
    if(problem){result(problem,409);return;}
    const bool sent=mc?mcxchat::send(mcxchat::Network::MC,*c,mk,r.text):m->send_text(mt,r.text,esp_random()|1u,uint32_t(esp_timer_get_time()/1000));
    result(sent?"Queued on the selected network. Delivery is shown in the conversation.":"Could not queue this message. Check radio status before trying again.",sent?202:409);return;
  }
  j.add("],\"total\":");j.number(total);j.add(",\"page\":");j.number(r.page);j.add(",\"selected\":");j.add(found?"true":"false");
  j.add(",\"send_problem\":");j.text(!found?"Choose a conversation.":problem?problem:"");j.add(",\"limit\":");j.number(found?(mc?c->text_limit(mk.kind==mcxchat::Kind::Channel):m->text_limit(mt)):0);
  j.add(",\"messages\":[");
  // Sort pointers only; no archive copies or history mutation on the UI stack.
  const meshmc::Message *cm[meshmc::MAX_MESSAGES];const meshmt::Message *mm[meshmt::MAX_MESSAGES];unsigned count=0;
  if(found&&mc){for(const auto &msg:c->messages)if(mcxchat::belongs(msg,mk))cm[count++]=&msg;
    for(unsigned i=1;i<count;++i){auto *v=cm[i];unsigned x=i;while(x&&cm[x-1]->id>v->id){cm[x]=cm[x-1];--x;}cm[x]=v;}
  }else if(found){for(const auto &msg:m->data.messages)if(mt_belongs(msg,mt,m->data.my_num))mm[count++]=&msg;
    for(unsigned i=1;i<count;++i){auto *v=mm[i];unsigned x=i;while(x&&mm[x-1]->seq>v->seq){mm[x]=mm[x-1];--x;}mm[x]=v;}
  }
  const unsigned start=count>MESSAGE_LIMIT?count-MESSAGE_LIMIT:0;
  for(unsigned i=start;i<count;++i){if(i!=start)j.add(",");
    const bool outgoing=mc?cm[i]->outgoing:mm[i]->outgoing;
    const char *sender=outgoing?"You":"Peer";
    if(mc&&!outgoing){int n=c->find_contact(cm[i]->key,6);if(n>=0)sender=c->contacts[n].name;}
    if(!mc&&!outgoing){auto *n=m->node(mm[i]->from);if(n)sender=n->name;}
    j.add("{\"id\":");j.number(mc?cm[i]->id:mm[i]->seq);j.add(",\"outgoing\":");j.add(outgoing?"true":"false");j.add(",\"sender\":");j.text(sender);
    j.add(",\"text\":");j.text(mc?cm[i]->text:mm[i]->text);j.add(",\"time\":");j.number(mc?cm[i]->timestamp:mm[i]->time);j.add(",\"delivery\":");j.text(mc?mc_delivery(cm[i]->delivery):meshmt::delivery_name(mm[i]->delivery));j.add("}");
  }
  j.add("],\"older\":");j.number(start);j.add("}");work->length=j.used;work->code=200;
  if(!j.ok)result("Conversation response exceeded the available buffer.",503);
}
static void poll(){
  unsigned pending=1;if(!work||!phase.compare_exchange_strong(pending,2))return;
  process();phase.store(3);
}
} // namespace mcxremotechat
