#pragma once
// Local conversation model. Network and radio identity are mandatory at the send boundary.
#include "meshcrossway_meshcore_protocol.h"
#include <ctype.h>
namespace mcxchat {
enum class Network : uint8_t { MC=1, MT=2 };
enum class Kind : uint8_t { Direct=1, Channel=2, Room=3 };
enum class Notify : uint8_t { All=0, Mentions=1, Muted=2 };
struct Key {
  Network network=Network::MC;
  Kind kind=Kind::Direct;
  uint8_t radio[32]={}, native[32]={};
};
static bool equal(const Key &a,const Key &b) {
  return a.network==b.network&&a.kind==b.kind&&!memcmp(a.radio,b.radio,32)&&!memcmp(a.native,b.native,32);
}
static bool belongs(const meshmc::Message &m,const Key &key) {
  return m.id && key.network==Network::MC && m.channel==(key.kind==Kind::Channel) &&
    (m.channel ? (m.slot==key.native[0]&&!memcmp(m.channel_identity,key.native+1,16)) : !memcmp(m.key,key.native,6));
}
static Key key_for(const meshmc::Client &c,bool group,uint16_t index) {
  Key k;memcpy(k.radio,c.public_key,32);
  if(group){k.kind=Kind::Channel;k.native[0]=uint8_t(index);if(index<c.channel_count)memcpy(k.native+1,c.channels[index].secret,16);}
  else if(index<c.contact_count){
    k.kind=c.contacts[index].type==3?Kind::Room:Kind::Direct;
    memcpy(k.native,c.contacts[index].key,32);
  }
  return k;
}
static Key key_for_message(meshmc::Client &c,const meshmc::Message &m) {
  if(m.channel){Key k=key_for(c,true,m.slot);memcpy(k.native+1,m.channel_identity,16);return k;}
  const int i=c.find_contact(m.key,6);
  if(i>=0)return key_for(c,false,uint16_t(i));
  Key k;memcpy(k.radio,c.public_key,32);memcpy(k.native,m.key,6);return k;
}
static constexpr size_t MAX_THREADS=128, QUICK_COUNT=4;
struct Preference {
  bool used=false, favorite=false, blocked=false;
  Notify notify=Notify::All;
  Key key;
  char alias[33]={}, draft[meshmc::MAX_TEXT+1]={};
};
struct Local {
  Preference threads[MAX_THREADS];
  char quick[QUICK_COUNT][meshmc::MAX_TEXT+1] = {"OK, received.","On my way.","What is your location?","I need assistance."};
  Key context;
  bool has_context=false, previews=true;
  uint32_t revision=0;
  Preference *get(const Key &key,bool create=false) {
    Preference *empty=nullptr;
    for(auto &p:threads){if(p.used&&equal(p.key,key))return &p;if(!p.used&&!empty)empty=&p;}
    if(create&&empty){*empty=Preference{};empty->used=true;empty->key=key;++revision;return empty;}
    return nullptr;
  }
};
static bool contains(const char *text,const char *query) {
  if(!query||!*query)return true;
  for(;*text;++text){size_t i=0;while(query[i]&&text[i]&&tolower((unsigned char)query[i])==tolower((unsigned char)text[i]))++i;
    if(!query[i])return true;}
  return false;
}
static bool mention(const char *text,const char *name) {
  if(!name||!*name)return false;
  char token[36];snprintf(token,sizeof(token),"@%s",name);
  const size_t n=strlen(token);
  for(const char *p=text;*p;++p)if(*p=='@'&&!strncmp(p,token,n)&&(!p[n]||isspace((unsigned char)p[n])||strchr(".,!?;:",p[n])))return true;
  return false;
}
static bool notify(const Preference *p,const meshmc::Message &m,const char *name) {
  return !m.outgoing&&!m.read&&(!p||(!p->blocked&&p->notify!=Notify::Muted&&
      (p->notify!=Notify::Mentions||mention(m.text,name))));
}
static int unread(meshmc::Client &c,const Key &k) {
  int n=0;for(const auto &m:c.messages)if(belongs(m,k)&&!m.read&&!m.outgoing)++n;return n;
}
static void mark_read(meshmc::Client &c,const Key *k=nullptr) {
  bool changed=false;for(auto &m:c.messages)if(m.id&&!m.outgoing&&!m.read&&(!k||belongs(m,*k))){m.read=true;changed=true;}
  if(changed)++c.generation;
}
static bool clear(meshmc::Client &c,const Key &k) {
  // A live queue entry must never lose its message/ACK correlation.
  for(const auto &m:c.messages)if(belongs(m,k)&&m.outgoing&&(m.delivery==meshmc::QUEUED||m.delivery==meshmc::ACCEPTED))return false;
  for(auto &m:c.messages)if(belongs(m,k))m=meshmc::Message{};
  ++c.generation;return true;
}
struct Thread {Key key;uint32_t last=0;uint16_t unread=0;};
// This entire workspace belongs in PSRAM, never in static internal .data/.bss.
struct Workspace {
  Local local;
  Thread rows[meshmc::MAX_MESSAGES];
  uint16_t indices[meshmc::MAX_CONTACTS]={};
  uint64_t scores[meshmc::MAX_CONTACTS]={};
};
static_assert(sizeof(Workspace)<64*1024,"Bound the chat PSRAM workspace");
static size_t list(meshmc::Client &c,Local &local,Thread *out,size_t cap,const char *query="",bool groups_only=false,bool include_blocked=false) {
  size_t count=0;
  for(auto &m:c.messages){
    if(!m.id)continue;
    Key k=key_for_message(c,m);
    if(groups_only&&k.kind==Kind::Direct)continue;
    auto *pref=local.get(k);
    if(pref&&pref->blocked&&!include_blocked)continue;
    int ci=m.channel?-1:c.find_contact(m.key,6);
    const char *name=pref&&pref->alias[0]?pref->alias:m.channel?c.channels[m.slot].name:ci>=0?c.contacts[ci].name:"Unknown sender";
    if(!contains(m.text,query)&&!contains(name,query))continue;
    size_t i=0;while(i<count&&!equal(out[i].key,k))++i;
    if(i==count){if(count==cap)continue;out[count++]=Thread{};out[i].key=k;}
    if(m.id>out[i].last)out[i].last=m.id;
    if(!m.outgoing&&!m.read)++out[i].unread;
  }
  for(const auto &p:local.threads)if(p.used&&p.draft[0]&&p.key.network==Network::MC&&
    !memcmp(p.key.radio,c.public_key,32)&&(!p.blocked||include_blocked)&&contains(p.draft,query)){
    size_t i=0;while(i<count&&!equal(out[i].key,p.key))++i;
    if(i==count&&count<cap){out[count]=Thread{};out[count++].key=p.key;}
  }
  for(size_t i=1;i<count;++i){Thread value=out[i];size_t j=i;while(j&&out[j-1].last<value.last){out[j]=out[j-1];--j;}out[j]=value;}
  return count;
}
static bool send(Network network,meshmc::Client &c,const Key &k,const char *text) {
  if(network!=Network::MC||k.network!=network||memcmp(k.radio,c.public_key,32))return false;
  if(k.kind==Kind::Channel){if(memcmp(k.native+1,c.channels[k.native[0]].secret,16))return false;return c.send_text(true,k.native[0],text);}
  if(k.kind==Kind::Room){auto *r=c.room(k.native);if(!r||r->state!=meshmc::RoomState::Joined)return false;}
  const int target=c.find_contact(k.native);
  return target>=0&&c.send_text(false,uint16_t(target),text);
}
// Text sharing only: no web navigation, no implicit network request.
static bool url(const char *text,char *out,size_t cap) {
  const char *p=strstr(text,"https://");if(!p)p=strstr(text,"http://");if(!p||cap<2)return false;
  size_t n=0;while(p[n]&&!isspace((unsigned char)p[n])&&p[n]!='<'&&p[n]!='>')++n;
  if(n>=cap)return false;
  memcpy(out,p,n);out[n]=0;return true;
}
} // namespace mcxchat
