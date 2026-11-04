#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace mcxubx {
struct State {
  char software[31]={},hardware[11]={};
  uint32_t last_version_ms=0,last_sat_ms=0,last_config_ms=0;
  uint8_t satellites=0,used=0;
  uint8_t gps=0,glonass=0,galileo=0,beidou=0;
  uint8_t max_cno=0;
  uint8_t gps_enabled=255,galileo_enabled=255,beidou_enabled=255,glonass_enabled=255;
  uint32_t frames=0,errors=0;
};
static uint16_t u16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
static uint32_t u32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
static size_t packet(uint8_t cls,uint8_t id,const uint8_t *payload,size_t length,uint8_t *out,size_t cap){
  if(length>256||cap<length+8)return 0;
  out[0]=0xb5;out[1]=0x62;out[2]=cls;out[3]=id;out[4]=uint8_t(length);out[5]=uint8_t(length>>8);
  if(length)memcpy(out+6,payload,length);
  uint8_t a=0,b=0;for(size_t i=2;i<length+6;++i){a+=out[i];b+=a;}
  out[length+6]=a;out[length+7]=b;return length+8;
}
struct Parser {
  State state;
  uint8_t payload[512]={},header[4]={};
  uint16_t size=0,offset=0;
  uint8_t phase=0,ck_a=0,ck_b=0,cls=0,id=0;
  void reset(){phase=0;size=offset=0;ck_a=ck_b=0;}
  void sum(uint8_t b){ck_a+=b;ck_b+=ck_a;}
  void decode(uint32_t now){
    if(cls==0x0a&&id==0x04&&size>=40){
      memcpy(state.software,payload,30);state.software[30]=0;
      memcpy(state.hardware,payload+30,10);state.hardware[10]=0;
      state.last_version_ms=now;
    }else if(cls==0x01&&id==0x35&&size>=8&&payload[0]==1){
      const unsigned count=payload[5];
      if(size<8u+12u*count)return;
      State next=state;next.satellites=next.used=next.gps=next.glonass=next.galileo=next.beidou=next.max_cno=0;
      for(unsigned i=0;i<count;++i){
        const uint8_t *s=payload+8+12*i;
        if(s[0]==0)++next.gps;
        else if(s[0]==2)++next.galileo;
        else if(s[0]==3)++next.beidou;
        else if(s[0]==6)++next.glonass;
        if(s[2]>next.max_cno)next.max_cno=s[2];
        if(u32(s+8)&(1u<<3))++next.used;
        ++next.satellites;
      }
      next.last_sat_ms=now;state=next;
    }else if(cls==0x06&&id==0x8b&&size>=4){
      for(unsigned i=4;i+5<=size;i+=5){
        const uint32_t key=u32(payload+i);
        // CFG-SIGNAL-GPS_ENA, GAL_ENA, BDS_ENA, GLO_ENA: one-byte values.
        if(key==0x1031001f)state.gps_enabled=payload[i+4];
        if(key==0x10310021)state.galileo_enabled=payload[i+4];
        if(key==0x10310022)state.beidou_enabled=payload[i+4];
        if(key==0x10310025)state.glonass_enabled=payload[i+4];
      }
      state.last_config_ms=now;
    }
    ++state.frames;
  }
  void feed(uint8_t b,uint32_t now){
    switch(phase){
      case 0:if(b==0xb5)phase=1;break;
      case 1:phase=b==0x62?2:b==0xb5?1:0;break;
      case 2:cls=b;ck_a=ck_b=0;sum(b);phase=3;break;
      case 3:id=b;sum(b);phase=4;break;
      case 4:size=b;sum(b);phase=5;break;
      case 5:size|=uint16_t(b)<<8;sum(b);offset=0;if(size>sizeof(payload)){++state.errors;reset();}else phase=size?6:7;break;
      case 6:payload[offset++]=b;sum(b);if(offset==size)phase=7;break;
      case 7:if(b!=ck_a){++state.errors;reset();}else phase=8;break;
      case 8:if(b==ck_b)decode(now);else ++state.errors;reset();break;
      default:reset();break;
    }
  }
};
}
