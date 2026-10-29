#pragma once
// Versioned, bounded plaintext journal. Every committed transaction is recoverable locally;
// recovery never calls a protocol command or enqueues a transmission.
#include "meshcrossway_conversations.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <atomic>
namespace mcxstore {
enum Failure { NONE, INVALID_STATE, DIRECTORY, OPEN, READ, WRITE, FLUSH, SYNC, CLOSE, TRUNCATE, SEEK, ROTATE, INSTALL, MEMORY, CORRUPT };
static std::atomic<int> failure{NONE},failure_errno{0};
static const char *step_name(int step){
  switch(step){case INVALID_STATE:return "state";case DIRECTORY:return "directory";case OPEN:return "open";
    case READ:return "read";case WRITE:return "write";case FLUSH:return "flush";case SYNC:return "sync";
    case CLOSE:return "close";case TRUNCATE:return "truncate";case SEEK:return "seek";
    case ROTATE:return "rotate";case INSTALL:return "rename";case MEMORY:return "memory";
    case CORRUPT:return "journal";default:return "none";}
}
static bool failed(Failure step,int error){failure_errno.store(error);failure.store(step);return false;}
static constexpr uint32_t MAGIC=0x3248434d, SCHEMA=2;
static constexpr size_t QUOTA=1024*1024; // Per radio: active + previous compacted journal <= ~2 MiB.
static constexpr uint32_t MAX_TRANSACTIONS=64; // Also bound replay CPU for many tiny deltas.
struct Meta {
  mcxchat::Network network=mcxchat::Network::MC;
  uint8_t radio[32]={};
  uint16_t contacts=0,channels=0,messages=0;
  uint32_t next_id=1;
};
struct State {
  Meta meta;
  meshmc::Contact contacts[meshmc::MAX_CONTACTS];
  meshmc::Channel channels[meshmc::MAX_CHANNELS];
  meshmc::Message messages[meshmc::MAX_MESSAGES];
  mcxchat::Local local;
};
// Fixed width envelope; object sizes form an ABI fence. Never interpret another layout as this one.
struct Header {uint32_t magic=MAGIC,schema=SCHEMA,bytes=0,sequence=0,checksum=0;uint16_t kind=0,index=0;};
enum : uint16_t { BEGIN=1, CONTACT=2, CHANNEL=3, MESSAGE=4, LOCAL=5, COMMIT=6 };
static uint32_t hash(const void *data,size_t n) {
  const auto *p=(const uint8_t *)data;uint32_t h=2166136261u;
  for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;
  return h;
}
static bool nonzero(const uint8_t *p){for(int i=0;i<32;++i)if(p[i])return true;return false;}
static bool valid(const State &s,const uint8_t *radio) {
  if(s.meta.network!=mcxchat::Network::MC||memcmp(s.meta.radio,radio,32)||!nonzero(radio)||s.meta.contacts>meshmc::MAX_CONTACTS||
     s.meta.channels>meshmc::MAX_CHANNELS||s.meta.messages>meshmc::MAX_MESSAGES||!s.meta.next_id)return false;
  for(const auto &c:s.contacts)if(!memchr(c.name,0,sizeof(c.name)))return false;
  for(const auto &c:s.channels)if(!memchr(c.name,0,sizeof(c.name)))return false;
  for(const auto &m:s.messages)if(!memchr(m.text,0,sizeof(m.text))||m.delivery>meshmc::UNCERTAIN||
    (m.id&&(m.id>=s.meta.next_id||s.meta.next_id-m.id>meshmc::MAX_MESSAGES)))return false;
  for(const auto &p:s.local.threads)if(p.used&&(p.key.network!=mcxchat::Network::MC||
      memcmp(p.key.radio,radio,32)||p.key.kind<mcxchat::Kind::Direct||p.key.kind>mcxchat::Kind::Room||
      p.notify>mcxchat::Notify::Muted||!memchr(p.alias,0,sizeof(p.alias))||!memchr(p.draft,0,sizeof(p.draft))))return false;
  for(const auto &q:s.local.quick)if(!memchr(q,0,sizeof(q)))return false;
  return !s.local.has_context||(s.local.context.network==mcxchat::Network::MC&&!memcmp(s.local.context.radio,radio,32));
}
static void path(char *out,size_t cap,const char *root,const uint8_t *radio,const char *ext) {
  char id[65];meshmc::hex(radio,32,id,sizeof(id));snprintf(out,cap,"%s/mc-%s.%s",root,id,ext);
}
static bool record(FILE *f,uint16_t kind,uint16_t index,uint32_t sequence,const void *p,size_t n) {
  Header h;h.kind=kind;h.index=index;h.sequence=sequence;h.bytes=n;h.checksum=hash(p,n);
  if(fwrite(&h,1,sizeof(h),f)!=sizeof(h)||fwrite(p,1,n,f)!=n)return failed(WRITE,errno?errno:EIO);
  return true;
}
struct Recovery {bool found=false,recovered=false;long end=0;uint32_t sequence=0;int error=0;};
// Worker-owned cursor. Bulk state stays in its existing PSRAM buffers. A healthy
// append compares against the last flushed state instead of replaying the SD log.
struct Cursor {bool ready=false;uint8_t radio[32]={};Recovery recovery;};
// Caller supplies PSRAM staging buffers; no large stack objects or unbounded allocation.
static Recovery replay(const char *file,const uint8_t *radio,State &out,State &staging) {
  Recovery r;FILE *f=fopen(file,"rb");if(!f){if(errno!=ENOENT)r.error=errno?errno:EIO;return r;}
  struct stat st={};if(fstat(fileno(f),&st)){r.error=errno?errno:EIO;fclose(f);return r;}
  if(st.st_size>long(QUOTA+sizeof(State)*2)){fclose(f);r.recovered=true;return r;}
  new (&out) State();bool transaction=false;
  while(true){Header h;size_t got=fread(&h,1,sizeof(h),f);if(!got)break;
    if(got!=sizeof(h)||h.magic!=MAGIC||h.schema!=SCHEMA||h.sequence!=r.sequence+1){r.recovered=true;break;}
    void *target=nullptr;size_t size=0;
    if(h.kind==BEGIN&&!transaction){staging=out;target=&staging.meta;size=sizeof(Meta);transaction=true;}
    else if(transaction){
      if(h.kind==CONTACT&&h.index<meshmc::MAX_CONTACTS){target=&staging.contacts[h.index];size=sizeof(meshmc::Contact);}
      if(h.kind==CHANNEL&&h.index<meshmc::MAX_CHANNELS){target=&staging.channels[h.index];size=sizeof(meshmc::Channel);}
      if(h.kind==MESSAGE&&h.index<meshmc::MAX_MESSAGES){target=&staging.messages[h.index];size=sizeof(meshmc::Message);}
      if(h.kind==LOCAL){target=&staging.local;size=sizeof(mcxchat::Local);}
    }
    if(h.kind==COMMIT&&transaction&&h.bytes==sizeof(uint32_t)){
      uint32_t checksum=0;
      if(fread(&checksum,1,sizeof(checksum),f)!=sizeof(checksum)||h.checksum!=hash(&checksum,sizeof(checksum))||
        checksum!=hash(&staging,sizeof(staging))||!valid(staging,radio)){r.recovered=true;break;}
      out=staging;r.found=true;r.sequence=h.sequence;r.end=ftell(f);transaction=false;continue;
    }
    if(!target||h.bytes!=size||fread(target,1,size,f)!=size||h.checksum!=hash(target,size)){
      r.recovered=true;break;
    }
  }
  if(transaction)r.recovered=true;
  if(ferror(f))r.error=errno?errno:EIO;
  if(fclose(f)&&!r.error)r.error=errno?errno:EIO;
  return r;
}
static bool transaction(FILE *f,const State &state,const State *previous,uint32_t seq) {
  bool ok=record(f,BEGIN,0,seq,&state.meta,sizeof(Meta));
  for(size_t i=0;ok&&i<meshmc::MAX_CONTACTS;++i)if(!previous||memcmp(&state.contacts[i],&previous->contacts[i],sizeof(meshmc::Contact)))
    ok=record(f,CONTACT,i,seq,&state.contacts[i],sizeof(meshmc::Contact));
  for(size_t i=0;ok&&i<meshmc::MAX_CHANNELS;++i)if(!previous||memcmp(&state.channels[i],&previous->channels[i],sizeof(meshmc::Channel)))
    ok=record(f,CHANNEL,i,seq,&state.channels[i],sizeof(meshmc::Channel));
  for(size_t i=0;ok&&i<meshmc::MAX_MESSAGES;++i)if(!previous||memcmp(&state.messages[i],&previous->messages[i],sizeof(meshmc::Message)))
    ok=record(f,MESSAGE,i,seq,&state.messages[i],sizeof(meshmc::Message));
  if(ok&&(!previous||memcmp(&state.local,&previous->local,sizeof(mcxchat::Local))))
    ok=record(f,LOCAL,0,seq,&state.local,sizeof(mcxchat::Local));
  const uint32_t sum=hash(&state,sizeof(state));
  if(ok)ok=record(f,COMMIT,0,seq,&sum,sizeof(sum));
  if(!ok)return false;
  if(fflush(f))return failed(FLUSH,errno?errno:EIO);
  if(fsync(fileno(f)))return failed(SYNC,errno?errno:EIO);
  return true;
}
// A compacted file is installed only after a full transaction is flushed. The previous
// log survives interruptions between renames. An incomplete append is truncated before retry.
static bool save(const char *root,const State &s,State &previous,State &staging,Cursor *cursor=nullptr) {
  failure.store(NONE);failure_errno.store(0);
  if(!valid(s,s.meta.radio))return failed(INVALID_STATE,EINVAL);
  if(mkdir(root,0755)&&errno!=EEXIST){if(cursor)cursor->ready=false;return failed(DIRECTORY,errno);}
  char file[240],backup[240],temp[240];path(file,sizeof(file),root,s.meta.radio,"log");
  path(backup,sizeof(backup),root,s.meta.radio,"bak2");path(temp,sizeof(temp),root,s.meta.radio,"tmp2");
  Recovery r;struct stat st={};bool cached=false;
  if(cursor&&cursor->ready&&!memcmp(cursor->radio,s.meta.radio,32)){
    cached=stat(file,&st)==0&&st.st_size==cursor->recovery.end;
    if(cached)r=cursor->recovery;
  }
  if(!cached){if(cursor)cursor->ready=false;r=replay(file,s.meta.radio,previous,staging);}
  // An I/O failure is not a corrupt tail: never truncate or replace on read error.
  if(r.error){if(cursor)cursor->ready=false;return failed(READ,r.error);}
  if(cached&&!memcmp(&s,&previous,sizeof(s)))return true;
  if(cursor)cursor->ready=false; // Any failed mutation forces replay before retry.
  if(r.found&&r.end<long(QUOTA)&&r.sequence<MAX_TRANSACTIONS){
    FILE *f=fopen(file,"r+b");if(!f)return failed(OPEN,errno);
    bool ok=true;
    if(r.recovered&&ftruncate(fileno(f),r.end))ok=failed(TRUNCATE,errno);
    if(ok&&fseek(f,r.end,SEEK_SET))ok=failed(SEEK,errno);
    if(ok)ok=transaction(f,s,&previous,r.sequence+1);
    const long end=ok?ftell(f):0;
    if(ok&&end<0)ok=failed(SEEK,errno);
    if(fclose(f)&&ok)ok=failed(CLOSE,errno);
    if(ok&&cursor){previous=s;cursor->ready=true;memcpy(cursor->radio,s.meta.radio,32);
      cursor->recovery=Recovery{};cursor->recovery.found=true;cursor->recovery.end=end;cursor->recovery.sequence=r.sequence+1;}
    return ok;
  }
  FILE *f=fopen(temp,"wb");if(!f)return failed(OPEN,errno);
  bool ok=transaction(f,s,nullptr,1);const long end=ok?ftell(f):0;
  if(ok&&end<0)ok=failed(SEEK,errno);
  if(fclose(f)&&ok)ok=failed(CLOSE,errno);
  if(!ok){remove(temp);return false;}
  if(r.found){
    if(remove(backup)&&errno!=ENOENT)return failed(ROTATE,errno);
    if(rename(file,backup))return failed(ROTATE,errno);
  }
  if(!r.found&&remove(file)&&errno!=ENOENT)return failed(ROTATE,errno);
  if(rename(temp,file))return failed(INSTALL,errno);
  if(cursor){previous=s;cursor->ready=true;memcpy(cursor->radio,s.meta.radio,32);
    cursor->recovery=Recovery{};cursor->recovery.found=true;cursor->recovery.end=end;cursor->recovery.sequence=1;}
  return true;
}
static Recovery load(const char *root,const uint8_t *radio,State &out,State &staging) {
  char file[240];path(file,sizeof(file),root,radio,"log");Recovery r=replay(file,radio,out,staging);
  if(r.found||r.error)return r;
  const bool corrupt=r.recovered;
  path(file,sizeof(file),root,radio,"bak2");r=replay(file,radio,out,staging);if(r.found||corrupt)r.recovered=true;return r;
}
static void capture(const meshmc::Client &c,const mcxchat::Local &local,State &s) {
  memcpy(s.meta.radio,c.public_key,32);s.meta.contacts=c.contact_count;s.meta.channels=c.channel_count;
  s.meta.messages=c.message_count;s.meta.next_id=c.next_id;
  memcpy(s.contacts,c.contacts,sizeof(s.contacts));memcpy(s.channels,c.channels,sizeof(s.channels));
  memcpy(s.messages,c.messages,sizeof(s.messages));s.local=local;
}
static void restore(const State &s,meshmc::Client &c,mcxchat::Local &local,bool live) {
  if(!live){c.contact_count=s.meta.contacts;c.channel_count=s.meta.channels;
    memcpy(c.contacts,s.contacts,sizeof(c.contacts));memcpy(c.channels,s.channels,sizeof(c.channels));
    memcpy(c.public_key,s.meta.radio,32);
  }
  // Bootstrap must finish before radio backlog is drained, so historical ordering wins.
  memcpy(c.messages,s.messages,sizeof(c.messages));c.message_count=s.meta.messages;c.next_id=s.meta.next_id;
  for(auto &m:c.messages)if(m.id&&m.outgoing){
    if(m.timestamp>c.last_timestamp)c.last_timestamp=m.timestamp;
    if(m.delivery==meshmc::QUEUED||m.delivery==meshmc::ACCEPTED)m.delivery=meshmc::UNCERTAIN;
    m.ack=0;m.ack_deadline=0; // No old ACK may confirm a new session's message.
  }
  local=s.local;++c.generation;
}
} // namespace mcxstore
