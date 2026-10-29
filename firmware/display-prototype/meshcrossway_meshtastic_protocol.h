#pragma once
// Original, bounded PhoneAPI implementation. Wire facts: see docs/meshtastic-v0712.md.
// No upstream .proto/generated binding/firmware code is compiled into this client.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <new>

namespace meshmt {
  static constexpr size_t MAX_FRAME=512, MAX_TEXT=237, MAX_NODES=128, MAX_CHANNELS=8, MAX_MESSAGES=96;
  static constexpr uint32_t BROADCAST=0xffffffffu;
  struct Frame { uint16_t len=0;
    uint8_t data[MAX_FRAME]={};
  };
  struct View { const uint8_t *data=nullptr;
    size_t len=0;
  };
  struct Field { uint32_t tag=0;
    uint8_t wire=0;
    uint64_t value=0;
    View bytes;
  };
  // Unknown length-delimited/fixed/varint fields are skipped, including future oneofs.
  struct Reader {
    View v;
    size_t offset=0;
    bool ok=true;
    explicit Reader(View input):v(input) {if(!v.data&&v.len)ok=false;
    }
    bool varint(uint64_t &value) {
      value=0;
      for(unsigned i=0;i<10;++i){
        if(offset>=v.len){ok=false;
          return false;
        }
        uint8_t b=v.data[offset++];
        if(i==9&&b>1){ok=false;
          return false;
        }
        value|=uint64_t(b&127)<<(i*7);
        if(!(b&128))return true;
      }
      ok=false;
      return false;
    }
    bool next(Field &f) {
      if(!ok||offset==v.len)return false;
      f=Field{};
      uint64_t key=0;
      if(!varint(key))return false;
      if(!(key>>3)||(key>>3)>0x1fffffffu){ok=false;
        return false;
      }
      f.tag=uint32_t(key>>3);
      f.wire=uint8_t(key&7);
      if(f.wire==0)return varint(f.value);
      size_t n=0;
      if(f.wire==1)n=8;
      else if(f.wire==5)n=4;
      else if(f.wire==2){uint64_t size=0;
        if(!varint(size)||size>v.len-offset){ok=false;
          return false;
        }n=size;
      }
      else {ok=false;
        return false;
      } // Groups are not used by this proto3 protocol.
      if(n>v.len-offset){ok=false;
        return false;
      }
      f.bytes={v.data+offset,n};
      if(f.wire!=2)for(size_t i=0;i<n;++i)f.value|=uint64_t(v.data[offset+i])<<(i*8);
      offset+=n;
      return true;
    }
  };
  static bool valid(View v){Reader r(v);
    Field f;
    while(r.next(f)){}return r.ok;
  }
  static bool utf8(const char *p,size_t n){
    for(size_t i=0;i<n;){uint8_t a=uint8_t(p[i++]);
      if(!a)return false;
      if(a<128)continue;
      unsigned extra=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:0;
      if(!extra||extra>n-i)return false;
      uint8_t b=uint8_t(p[i]);
      if((a==0xe0&&b<0xa0)||(a==0xed&&b>=0xa0)||(a==0xf0&&b<0x90)||(a==0xf4&&b>=0x90))return false;
      while(extra--)if((uint8_t(p[i++])&0xc0)!=0x80)return false;
    }return true;
  }
  static bool string_field(const Field &f,char *out,size_t cap){
    if(f.wire!=2||f.bytes.len>=cap||!utf8((const char *)f.bytes.data,f.bytes.len))return false;
    memcpy(out,f.bytes.data,f.bytes.len);
    out[f.bytes.len]=0;
    return true;
  }
  struct Writer {
    Frame &f;
    bool ok=true;
    explicit Writer(Frame &out):f(out){f.len=0;
    }
    void byte(uint8_t b){if(f.len==MAX_FRAME){ok=false;
        return;
      }f.data[f.len++]=b;
    }
    void varint(uint64_t v){while(v>127){byte(uint8_t(v)|128);
        v>>=7;
      }byte(uint8_t(v));
    }
    void scalar(uint32_t tag,uint32_t value){varint(uint64_t(tag)<<3);
      varint(value);
    }
    void fixed(uint32_t tag,uint32_t value){varint((uint64_t(tag)<<3)|5);
      for(int i=0;i<4;++i)byte(value>>(8*i));
    }
    void bytes(uint32_t tag,const void *p,size_t n){varint((uint64_t(tag)<<3)|2);
      varint(n);
      const auto *s=(const uint8_t *)p;
      for(size_t i=0;i<n;++i)byte(s[i]);
    }
    void message(uint32_t tag,const Frame &s){bytes(tag,s.data,s.len);
    }
  };
  static uint64_t fingerprint(View v){uint64_t h=14695981039346656037ull;
    for(size_t i=0;i<v.len;++i)h=(h^v.data[i])*1099511628211ull;
    return h?h:1;
  }
  static float float_value(uint32_t bits){float value;
    memcpy(&value,&bits,4);
    return isfinite(value)?value:0;
  }
  struct Node {
    uint32_t num=0,last_heard=0,hardware=0;
    char name[49]={},short_name[17]={};
    uint8_t public_key[32]={};
    bool has_key=false,unmessagable=false,position=false,has_lat=false,has_lon=false;
    int32_t latitude=0,longitude=0,altitude=0;
    uint32_t position_time=0;
    float snr=0,voltage=0,utilization=0;
    uint16_t battery=0;
    bool has_battery=false;
  };
  struct Channel {bool loaded=false;
    uint8_t role=0;
    char name[49]={};
    uint64_t identity=0;
  };
  enum Delivery : uint8_t { RECEIVED, QUEUED, SUBMITTED, RADIO_ACCEPTED, REMOTE_ACK, REJECTED, UNCERTAIN };
  static const char *delivery_name(Delivery d){
    switch(d){case QUEUED:return "Queued locally";
      case SUBMITTED:return "Bluetooth accepted";
      case RADIO_ACCEPTED:return "Radio accepted";
      case REMOTE_ACK:return "Remote ACK";
      case REJECTED:return "Rejected";
      case UNCERTAIN:return "Unconfirmed";
      default:return "Received";
    }
  }
  struct Message {
    uint32_t seq=0,id=0,from=0,to=0,time=0,queued_at=0;
    uint64_t channel_identity=0;
    uint8_t channel=0,error=0;
    Delivery delivery=RECEIVED;
    bool outgoing=false,read=false,pki=false,via_mqtt=false;
    char text[MAX_TEXT+1]={};
  };
  struct Target {uint8_t radio[7]={};
    uint32_t node=BROADCAST;
    uint8_t channel=0;
    uint64_t channel_identity=0;
  };
  struct Draft {bool used=false;
    Target target;
    char text[MAX_TEXT+1]={};
  };
  struct Archive {
    uint8_t radio[7]={};
    uint32_t my_num=0,next_seq=1;
    uint16_t node_count=0;
    uint16_t message_count=0;
    Node nodes[MAX_NODES];
    Channel channels[MAX_CHANNELS];
    Message messages[MAX_MESSAGES];
    Draft drafts[16];
  };
  struct Client {
    Archive data;
    bool connected=false,initialized=false,fault=false,history_ready=false,have_my_info=false,have_metadata=false,have_lora=false,tx_enabled=false,has_pki=false;
    uint8_t region=0,hop_limit=3;
    uint16_t mtu=0;
    uint32_t config_nonce=0,generation=0,started=0,last_tx=0,min_app_version=0,hardware=0;
    uint32_t ignored=0,dropped_nodes=0;
    char firmware[49]={},status[112]="Connect Meshtastic in More / Radios";
    Frame pending;
    uint32_t pending_id=0;
    bool pending_ready=false,writing=false,config_sent=false;
    void status_text(const char *text){snprintf(status,sizeof(status),"%s",text);
      ++generation;
    }
    void fail(const char *why){fault=true;
      initialized=false;
      status_text(why);
    }
    Node *node(uint32_t id,bool create=false){
      if(!id||id==BROADCAST)return nullptr;
      for(unsigned i=0;i<data.node_count;++i)if(data.nodes[i].num==id)return &data.nodes[i];
      if(!create)return nullptr;
      if(data.node_count==MAX_NODES){++dropped_nodes;
        return nullptr;
      }
      auto &n=data.nodes[data.node_count++];
      n=Node{};
      n.num=id;
      snprintf(n.name,sizeof(n.name),"!%08lx",(unsigned long)id);
      return &n;
    }
    Message *message(uint32_t id){for(auto &m:data.messages)if(m.seq&&m.outgoing&&m.id==id)return &m;
      return nullptr;
    }
    Message &append(){
      // No queue state is reconstructed from persisted messages.
      auto &m=data.messages[(data.next_seq-1)%MAX_MESSAGES];
      m=Message{};
      m.seq=data.next_seq++;
      if(data.message_count<MAX_MESSAGES)++data.message_count;
      return m;
    }
    void detach(){
      connected=initialized=false;
      writing=pending_ready=false;
      pending_id=0;
      for(auto &m:data.messages)if(m.seq&&m.outgoing&&(m.delivery==QUEUED||m.delivery==SUBMITTED||m.delivery==RADIO_ACCEPTED))m.delivery=UNCERTAIN;
      status_text("Meshtastic disconnected / saved history");
    }
    void attach(const uint8_t identity[7],uint16_t payload_mtu,uint32_t nonce,uint32_t now){
      new(&data) Archive();
      memcpy(data.radio,identity,7);
      connected=true;
      initialized=fault=history_ready=false;
      have_my_info=have_metadata=have_lora=tx_enabled=has_pki=false;
      mtu=payload_mtu;
      config_nonce=nonce?nonce:1;
      if(config_nonce==69420||config_nonce==69421)config_nonce+=2;
      started=last_tx=now;
      min_app_version=hardware=ignored=dropped_nodes=0;
      firmware[0]=0;
      region=0;
      hop_limit=3;
      writing=pending_ready=config_sent=false;
      pending_id=0;
      status_text("Meshtastic: downloading configuration and nodes...");
    }
    bool parse_user(View v,Node &n){
      Reader r(v);
      Field f;
      while(r.next(f)){
        if(f.tag==2&&!string_field(f,n.name,sizeof(n.name)))return false;
        if(f.tag==3&&!string_field(f,n.short_name,sizeof(n.short_name)))return false;
        if(f.tag==5){if(f.wire!=0)return false;
          n.hardware=f.value;
        }
        if(f.tag==8){if(f.wire!=2||(f.bytes.len!=0&&f.bytes.len!=32))return false;
          n.has_key=f.bytes.len==32;
          memset(n.public_key,0,32);
          if(n.has_key)memcpy(n.public_key,f.bytes.data,32);
        }
        if(f.tag==9){if(f.wire!=0)return false;
          n.unmessagable=f.value!=0;
        }
      }return r.ok;
    }
    bool parse_position(View v,Node &n){
      Reader r(v);
      Field f;
      while(r.next(f)){
        if(f.tag==1||f.tag==2||f.tag==4){if(f.wire!=5)return false;
          if(f.tag==1){n.latitude=int32_t(uint32_t(f.value));
            n.has_lat=true;
          }
          if(f.tag==2){n.longitude=int32_t(uint32_t(f.value));
            n.has_lon=true;
          }
          if(f.tag==4)n.position_time=f.value;
        }
        if(f.tag==3){if(f.wire!=0)return false;
          n.altitude=int32_t(uint32_t(f.value));
        }
      }
      n.position=n.has_lat&&n.has_lon&&n.latitude>=-900000000&&n.latitude<=900000000&&n.longitude>=-1800000000&&n.longitude<=1800000000;
      return r.ok;
    }
    bool parse_metrics(View v,Node &n){
      Reader r(v);
      Field f;
      while(r.next(f)){
        if(f.tag==1){if(f.wire!=0||f.value>101)return false;
          n.battery=f.value;
          n.has_battery=true;
        }
        if(f.tag==2||f.tag==3){if(f.wire!=5)return false;
          if(f.tag==2)n.voltage=float_value(f.value);
          else n.utilization=float_value(f.value);
        }
      }return r.ok;
    }
    bool parse_node(View v){
      if(!valid(v))return false;
      uint32_t id=0;
      Reader scan(v);
      Field f;
      while(scan.next(f))if(f.tag==1){if(f.wire!=0||f.value>UINT32_MAX)return false;
        id=f.value;
      }
      Node temp;
      auto *old=node(id);
      if(old)temp=*old;
      else temp.num=id;
      Reader r(v);
      while(r.next(f)){
        if(f.tag==2&&(f.wire!=2||!parse_user(f.bytes,temp)))return false;
        if(f.tag==3&&(f.wire!=2||!parse_position(f.bytes,temp)))return false;
        if(f.tag==4){if(f.wire!=5)return false;
          temp.snr=float_value(f.value);
        }
        if(f.tag==5){if(f.wire!=5)return false;
          temp.last_heard=f.value;
        }
        if(f.tag==6&&(f.wire!=2||!parse_metrics(f.bytes,temp)))return false;
      }
      if(auto *dest=node(id,true))*dest=temp;
      return r.ok;
    }
    bool parse_channel(View v){
      Reader r(v);
      Field f;
      uint32_t index=0;
      Channel c;
      while(r.next(f)){
        if(f.tag==1){if(f.wire!=0||f.value>=MAX_CHANNELS)return false;
          index=f.value;
        }
        if(f.tag==2){if(f.wire!=2||!valid(f.bytes))return false;
          c.identity=fingerprint(f.bytes);
          Reader settings(f.bytes);
          Field s;
          while(settings.next(s))if(s.tag==3&&!string_field(s,c.name,sizeof(c.name)))return false;
        }
        if(f.tag==3){if(f.wire!=0||f.value>2)return false;
          c.role=f.value;
        }
      }
      if(!r.ok|| (c.role&&!c.identity))return false;
      c.loaded=true;
      if(!c.name[0])snprintf(c.name,sizeof(c.name),"Channel %lu",(unsigned long)index);
      data.channels[index]=c;
      return true;
    }
    bool parse_config(View v){
      Reader r(v);
      Field f;
      while(r.next(f))if(f.tag==6){
        if(f.wire!=2)return false;
        Reader l(f.bytes);
        Field p;
        bool tx=false;
        uint8_t reg=0,hops=3;
        while(l.next(p))if(p.tag==7||p.tag==8||p.tag==9){if(p.wire!=0||p.value>255)return false;
          if(p.tag==7)reg=p.value;
          if(p.tag==8){if(p.value>7)return false;
            hops=p.value;
          }if(p.tag==9)tx=p.value!=0;
        }
        if(!l.ok)return false;
        have_lora=true;
        region=reg;
        hop_limit=hops;
        tx_enabled=tx;
      }return r.ok;
    }
    bool parse_packet(View v,uint32_t now){
      Reader r(v);
      Field f;
      Message m;
      View decoded;
      bool has_decoded=false;
      while(r.next(f)){
        if(f.tag==1||f.tag==2||f.tag==6||f.tag==7){if(f.wire!=5)return false;
          if(f.tag==1)m.from=f.value;
          if(f.tag==2)m.to=f.value;
          if(f.tag==6)m.id=f.value;
          if(f.tag==7)m.time=f.value;
        }
        if(f.tag==3){if(f.wire!=0||f.value>=MAX_CHANNELS){++ignored;
            return true;
          }m.channel=f.value;
        }
        if(f.tag==4){if(f.wire!=2)return false;
          decoded=f.bytes;
          has_decoded=true;
        }
        if(f.tag==14||f.tag==17){if(f.wire!=0)return false;
          if(f.tag==14)m.via_mqtt=f.value!=0;
          else m.pki=f.value!=0;
        }
      }
      if(!r.ok)return false;
      if(!has_decoded){++ignored;
        return true;
      }
      uint32_t port=0,request=0;
      View payload;
      Reader d(decoded);
      while(d.next(f)){
        if(f.tag==1){if(f.wire!=0)return false;
          port=f.value;
        }
        if(f.tag==2){if(f.wire!=2)return false;
          payload=f.bytes;
        }
        if(f.tag==6){if(f.wire!=5)return false;
          request=f.value;
        }
      }
      if(!d.ok)return false;
      if(port==5){ // ROUTING_APP; ATT/queue acceptance is not a remote ACK.
        Reader route(payload);
        bool error_seen=false;
        uint32_t error=0;
        while(route.next(f))if(f.tag==3){if(f.wire!=0)return false;
          error=f.value;
          error_seen=true;
        }
        if(!route.ok)return false;
        auto *sent=message(request);
        if(sent&&error_seen&&m.to==data.my_num&&(m.from==data.my_num||m.from==sent->to)){
          if(error){sent->delivery=REJECTED;
            sent->error=uint8_t(error);
            ++generation;
          }
          else if(sent->to!=BROADCAST&&m.from==sent->to){sent->delivery=REMOTE_ACK;
            ++generation;
          }
          else if(sent->delivery<REMOTE_ACK){sent->delivery=RADIO_ACCEPTED;
            ++generation;
          }
        }return true;
      }
      if(port==1){
        if(!payload.len||payload.len>MAX_TEXT||!utf8((const char *)payload.data,payload.len)){++ignored;
          return true;
        }
        // Only this radio's own traffic and broadcast channels enter its conversation store.
        if(!m.id||!m.from||(m.to!=BROADCAST&&m.to!=data.my_num&&m.from!=data.my_num)){++ignored;
          return true;
        }
        for(const auto &old:data.messages)if(old.seq&&old.id==m.id&&old.from==m.from)return true;
        memcpy(m.text,payload.data,payload.len);
        m.text[payload.len]=0;
        m.outgoing=m.from==data.my_num;
        m.read=m.outgoing;
        m.delivery=m.outgoing?RADIO_ACCEPTED:RECEIVED;
        m.queued_at=now;
        m.channel_identity=data.channels[m.channel].identity;
        auto &added=append();
        m.seq=added.seq;
        added=m;
      }else if(port==3||port==4||port==67){
        // POSITION_APP, NODEINFO_APP, TELEMETRY_APP. Never relay any packet.
        Node n;
        auto *old=node(m.from);
        if(old)n=*old;
        else n.num=m.from;
        if(port==3&&!parse_position(payload,n))return false;
        if(port==4&&!parse_user(payload,n))return false;
        if(port==67){Reader t(payload);
          while(t.next(f))if(f.tag==2&&(f.wire!=2||!parse_metrics(f.bytes,n)))return false;
          if(!t.ok)return false;
        }
        n.last_heard=m.time;
        if(auto *dest=node(m.from,true))*dest=n;
      }else ++ignored;
      ++generation;
      return true;
    }
    bool receive(const uint8_t *p,size_t n,uint32_t now){
      if(!connected||fault||!n||n>MAX_FRAME||!valid({p,n}))return false;
      Reader r({p,n});
      Field f;
      bool ok=true;
      while(r.next(f)){
        if(f.tag==2)ok=f.wire==2&&parse_packet(f.bytes,now);
        else if(f.tag==3){
          if(f.wire!=2)return false;
          Reader info(f.bytes);
          Field s;
          while(info.next(s)){
            if(s.tag==1){if(s.wire!=0||!s.value||s.value>=BROADCAST)return false;
              data.my_num=s.value;
              have_my_info=true;
            }
            if(s.tag==11){if(s.wire!=0)return false;
              min_app_version=s.value;
            }
          }ok=info.ok;
        }else if(f.tag==4)ok=f.wire==2&&parse_node(f.bytes);
        else if(f.tag==5)ok=f.wire==2&&parse_config(f.bytes);
        else if(f.tag==7){
          if(f.wire!=0)return false;
          if(f.value!=config_nonce)continue;
          if(!have_my_info||!have_metadata||!have_lora){fail("Incomplete Meshtastic config. Use T-Deck BaseUI / Bluetooth mode.");
            return false;
          }
          unsigned major=0,minor=0;
          if(sscanf(firmware,"%u.%u",&major,&minor)!=2||major!=2||minor<5){fail("Unsupported Meshtastic firmware: use official 2.5+ / 2.x PhoneAPI.");
            return false;
          }
          initialized=true;
          status_text("Meshtastic synchronized / restoring local history...");
        }else if(f.tag==8&&f.value){fail("Meshtastic radio rebooted. Reconnect to synchronize.");
          return false;
        }
        else if(f.tag==10)ok=f.wire==2&&parse_channel(f.bytes);
        else if(f.tag==11){
          if(f.wire!=2)return false;
          Reader q(f.bytes);
          Field s;
          uint32_t id=0,res=0;
          while(q.next(s)){if(s.tag==1||s.tag==4){if(s.wire!=0)return false;
              if(s.tag==1)res=s.value;
              else id=s.value;
            }}
          ok=q.ok;
          auto *m=message(id);
          if(m){if(res){m->delivery=REJECTED;
              m->error=uint8_t(res);
            }else if(m->delivery<REMOTE_ACK)m->delivery=RADIO_ACCEPTED;
            ++generation;
          }
        }else if(f.tag==13){
          if(f.wire!=2)return false;
          Reader meta(f.bytes);
          Field s;
          while(meta.next(s)){
            if(s.tag==1&&!string_field(s,firmware,sizeof(firmware)))return false;
            if(s.tag==9){if(s.wire!=0)return false;
              hardware=s.value;
            }
            if(s.tag==11){if(s.wire!=0)return false;
              has_pki=s.value!=0;
            }
          }ok=meta.ok;
          have_metadata=ok;
        }else if(f.tag==18){
          if(f.wire!=2)return false;
          Reader lock(f.bytes);
          Field s;
          while(lock.next(s))if(s.tag==1&&s.value!=3&&s.value!=5){fail("Radio API is locked. Unlock it with the official Meshtastic client.");
            return false;
          }
          ok=lock.ok;
        }else ++ignored;
        if(!ok){fail("Malformed Meshtastic protobuf. Reconnect / check radio firmware.");
          return false;
        }
      }
      if(!r.ok)return false;
      ++generation;
      return true;
    }
    Target target(uint32_t dest,uint8_t channel)const{
      Target t;
      memcpy(t.radio,data.radio,7);
      t.node=dest;
      t.channel=channel;
      if(channel<MAX_CHANNELS)t.channel_identity=data.channels[channel].identity;
      return t;
    }
    Draft *draft(const Target &t,bool create=false){
      Draft *empty=nullptr;
      for(auto &d:data.drafts){
        if(d.used&&!memcmp(d.target.radio,t.radio,7)&&d.target.node==t.node&&d.target.channel==t.channel&&d.target.channel_identity==t.channel_identity)return &d;
        if(!d.used&&!empty)empty=&d;
      }
      if(create&&empty){empty->used=true;
        empty->target=t;
        return empty;
      }return nullptr;
    }
    const char *send_problem(const Target &t,const char *text=nullptr){
      if(!connected||fault)return "Meshtastic radio disconnected. Draft kept locally.";
      if(!initialized||!history_ready)return "Meshtastic is synchronizing / restoring history.";
      if(memcmp(t.radio,data.radio,7))return "Conversation belongs to another radio. Select it again.";
      if(!region||!tx_enabled)return "LoRa region is unset or TX disabled. Configure the T-Deck first.";
      if(t.channel>=MAX_CHANNELS)return "Invalid Meshtastic channel.";
      const auto &c=data.channels[t.channel];
      if(!c.loaded||!c.role||!t.channel_identity||c.identity!=t.channel_identity)return "Channel changed or is disabled. Select it again.";
      if(t.node!=BROADCAST){auto *n=node(t.node);
        if(!n||n->unmessagable||t.node==data.my_num)return "Node unavailable or cannot receive messages.";
      }
      if(pending_ready||writing)return "Bluetooth write in progress. Wait before sending again.";
      if(text){size_t n=strlen(text);
        if(!n||n>MAX_TEXT||!utf8(text,n))return "Enter valid UTF-8 text, up to 237 bytes.";
      }
      return nullptr;
    }
    bool send_text(const Target &t,const char *text,uint32_t id,uint32_t now){
      if(!text||!id||message(id)||send_problem(t,text))return false;
      Frame payload,packet;
      Writer d(payload);
      d.scalar(1,1);
      d.bytes(2,text,strlen(text));
      Writer p(packet);
      p.fixed(2,t.node);
      p.scalar(3,t.channel);
      p.message(4,payload);
      p.fixed(6,id);
      p.scalar(9,hop_limit);
      p.scalar(10,t.node!=BROADCAST);
      p.scalar(11,t.node==BROADCAST?64:70);
      Node *dest=t.node==BROADCAST?nullptr:node(t.node);
      bool pki=dest&&has_pki&&dest->has_key;
      if(pki){p.bytes(16,dest->public_key,32);
        p.scalar(17,1);
      }
      Writer out(pending);
      out.message(1,packet);
      if(!d.ok||!p.ok||!out.ok||pending.len>mtu-3)return false;
      auto &m=append();
      m.id=id;
      m.from=data.my_num;
      m.to=t.node;
      m.channel=t.channel;
      m.channel_identity=t.channel_identity;
      m.outgoing=m.read=true;
      m.delivery=QUEUED;
      m.queued_at=now;
      m.pki=pki;
      snprintf(m.text,sizeof(m.text),"%s",text);
      pending_ready=true;
      pending_id=id;
      ++generation;
      return true;
    }
    bool next(uint32_t now,Frame &out){
      if(!connected||fault)return false;
      if(!initialized&&(uint32_t)(now-started)>90000){fail("Config timeout. T-Deck: BaseUI / Bluetooth mode; Wi-Fi off; close phone app.");
        return false;
      }
      for(auto &m:data.messages)if(m.seq&&m.outgoing&&(m.delivery==SUBMITTED||m.delivery==RADIO_ACCEPTED)&&(uint32_t)(now-m.queued_at)>120000){m.delivery=UNCERTAIN;
        ++generation;
      }
      if(writing)return false;
      if(!config_sent){Writer w(out);
        w.scalar(3,config_nonce);
        config_sent=true;
        writing=true;
        pending_id=0;
        last_tx=now;
        return w.ok;
      }
      if(initialized&&history_ready&&pending_ready){out=pending;
        pending_ready=false;
        writing=true;
        last_tx=now;
        return true;
      }
      if(initialized&&(uint32_t)(now-last_tx)>30000){Writer w(out);
        w.bytes(7,nullptr,0);
        writing=true;
        pending_id=0;
        last_tx=now;
        return true;
      }
      return false;
    }
    void write_done(bool ok){
      if(!writing)return;
      writing=false;
      if(auto *m=message(pending_id)){if(!ok)m->delivery=UNCERTAIN;
        else if(m->delivery==QUEUED)m->delivery=SUBMITTED;
      }
      pending_id=0;
      ++generation;
      if(!ok)fail("Meshtastic Bluetooth write failed. Reconnect; message will not auto-resend.");
    }
  };
  static_assert(sizeof(Client)<96*1024,"Meshtastic client must stay bounded in PSRAM");
} // namespace meshmt
