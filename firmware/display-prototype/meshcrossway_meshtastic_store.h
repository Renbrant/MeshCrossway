#pragma once
// Separate MT namespace and format. Only the existing SD worker calls these functions.
#include "meshcrossway_meshtastic_protocol.h"
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <initializer_list>
namespace meshmtstore {
  struct Header {uint32_t magic=0x31544d58,version=1,bytes=sizeof(meshmt::Archive),checksum=0;
  };
  static uint32_t checksum(const meshmt::Archive &a){
    auto *p=(const uint8_t *)&a;
    uint32_t h=2166136261u;
    for(size_t i=0;i<sizeof(a);++i)h=(h^p[i])*16777619u;
    return h;
  }
  static bool valid(const meshmt::Archive &a,const uint8_t radio[7]){
    if(memcmp(a.radio,radio,7)||!a.next_seq||a.node_count>meshmt::MAX_NODES||a.message_count>meshmt::MAX_MESSAGES)return false;
    for(const auto &n:a.nodes)if(!memchr(n.name,0,sizeof(n.name))||!memchr(n.short_name,0,sizeof(n.short_name)))return false;
    for(const auto &c:a.channels)if(!memchr(c.name,0,sizeof(c.name))||c.role>2)return false;
    for(const auto &m:a.messages)if(m.channel>=meshmt::MAX_CHANNELS||m.delivery>meshmt::UNCERTAIN||
    !memchr(m.text,0,sizeof(m.text))||(m.seq&&(m.seq>=a.next_seq||a.next_seq-m.seq>meshmt::MAX_MESSAGES)))return false;
    for(const auto &d:a.drafts)if(d.used&&(memcmp(d.target.radio,radio,7)||d.target.channel>=meshmt::MAX_CHANNELS||!memchr(d.text,0,sizeof(d.text))))return false;
    return true;
  }
  static void path(char *out,size_t cap,const char *root,const uint8_t radio[7],const char *ext){
    char id[15];
    for(int i=0;i<7;++i)snprintf(id+i*2,3,"%02x",radio[i]);
    snprintf(out,cap,"%s/mt-%s.%s",root,id,ext);
  }
  // 1 valid, 0 absent, -1 I/O error, -2 invalid. Never truncate on read failure.
  static int read(const char *file,const uint8_t radio[7],meshmt::Archive &a){
    FILE *f=fopen(file,"rb");
    if(!f)return errno==ENOENT?0:-1;
    Header h;
    bool ok=fread(&h,1,sizeof(h),f)==sizeof(h)&&h.magic==Header{}.magic&&h.version==1&&h.bytes==sizeof(a);
    if(ok)ok=fread(&a,1,sizeof(a),f)==sizeof(a)&&fgetc(f)==EOF;
    bool error=ferror(f);
    if(fclose(f))error=true;
    if(error)return -1;
    return ok&&h.checksum==checksum(a)&&valid(a,radio)?1:-2;
  }
  static int load(const char *root,const uint8_t radio[7],meshmt::Archive &a){
    char file[192];
    int failure=0;
    for(const char *ext:{"bin","bak","tmp"}){
      path(file,sizeof(file),root,radio,ext);
      const int rc=read(file,radio,a);
      if(rc==1)return 1;
      if(rc<0)failure=rc;
    }return failure;
  }
  static bool save(const char *root,const meshmt::Archive &a){
    if(!valid(a,a.radio)){errno=EINVAL;
      return false;
    }
    if(mkdir(root,0777)&&errno!=EEXIST)return false;
    char file[192],temp[192],backup[192];
    path(file,sizeof(file),root,a.radio,"bin");
    path(temp,sizeof(temp),root,a.radio,"tmp");
    path(backup,sizeof(backup),root,a.radio,"bak");
    FILE *f=fopen(temp,"wb");
    if(!f)return false;
    Header h;
    h.checksum=checksum(a);
    bool ok=fwrite(&h,1,sizeof(h),f)==sizeof(h)&&fwrite(&a,1,sizeof(a),f)==sizeof(a)&&fflush(f)==0&&fsync(fileno(f))==0;
    if(fclose(f))ok=false;
    if(!ok)return false;
    // The caller authorizes writes only after a successful load (or confirmed absence).
    // Keep the last complete image during replacement; an interrupted .tmp can recover.
    struct stat st={};
    if(stat(file,&st)==0){if(remove(backup)&&errno!=ENOENT)return false;
      if(rename(file,backup))return false;
    }
    else if(errno!=ENOENT)return false;
    return rename(temp,file)==0;
  }
  static void merge(meshmt::Client &c,const meshmt::Archive &saved){
    if(!valid(saved,c.data.radio))return;
    if(!c.connected){c.data.my_num=saved.my_num;
      memcpy(c.data.channels,saved.channels,sizeof(saved.channels));
    }
    for(unsigned i=0;i<saved.node_count;++i)if(!c.node(saved.nodes[i].num)){
      if(auto *n=c.node(saved.nodes[i].num,true))*n=saved.nodes[i];
    }
    // Called after config complete and before draining the radio message backlog.
    // The synchronized channel table always wins; changed channels remain separate.
    if(!c.data.message_count){
      memcpy(c.data.messages,saved.messages,sizeof(saved.messages));
      c.data.next_seq=saved.next_seq;
      c.data.message_count=saved.message_count;
      memcpy(c.data.drafts,saved.drafts,sizeof(saved.drafts));
      for(auto &m:c.data.messages)if(m.seq&&m.outgoing&&(m.delivery==meshmt::QUEUED||m.delivery==meshmt::SUBMITTED||m.delivery==meshmt::RADIO_ACCEPTED))m.delivery=meshmt::UNCERTAIN;
    }
    ++c.generation;
  }
} // namespace meshmtstore
