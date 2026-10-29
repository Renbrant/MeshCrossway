#pragma once
// MeshCrossway chat presentation: pure, read-only UTF-8 and metadata helpers.
// No radio packet mutation, no LVGL dependency and no unauthenticated identity
// verification. Channel senders are visible *claims* extracted from "Name: text".
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace mcxchat {
struct Sender {
  char name[65];
  const char *body;
  bool claimed;
};
static inline Sender channel_sender(const char *raw){
  Sender v={};
  v.body=raw?raw:"";
  if(!raw)return v;
  // MeshCore channel chat convention prefixes user input as "Name: message".
  // Do not guess an identity when the convention is absent or malformed.
  const char *sep=strstr(raw,": ");
  if(!sep)return v;
  const size_t n=(size_t)(sep-raw);
  if(n<1||n>64)return v;
  // Skip only ASCII spaces before ':', never split a UTF-8 codepoint.
  size_t trim=n;
  while(trim && raw[trim-1]==' ')--trim;
  if(!trim)return v;
  for(size_t i=0;i<trim;i++)
    if(raw[i]=='\n'||raw[i]=='\r'||(unsigned char)raw[i]<0x20u)return v;
  memcpy(v.name,raw,trim);
  v.name[trim]=0;
  v.body=sep+2;
  v.claimed=true;
  return v;
}
static inline uint8_t color_index(const char *name){
  // Deterministic per-sender appearance, independent of message/order.
  uint32_t hash=2166136261u;
  if(name)for(const unsigned char *p=(const unsigned char *)name;*p;p++){
    hash^=*p;hash*=16777619u;
  }
  return (uint8_t)(hash%6u);
}
enum Route : uint8_t { HOPS_KNOWN=0, DIRECT_ROUTE, ROUTE_UNKNOWN };
static inline Route route_hops(uint8_t path,uint8_t &hops){
  hops=0;
  // MeshCore receive path 0xFF means direct route, NOT 255 hops.
  if(path==0xFFu)return DIRECT_ROUTE;
  const uint8_t hash_size=(uint8_t)((path>>6)+1);
  const uint8_t count=(uint8_t)(path&0x3Fu);
  // 0xC0 prefix is reserved; max on-wire path is 64 bytes.
  if(hash_size>3u || (uint16_t)count*hash_size>64u)return ROUTE_UNKNOWN;
  hops=count;
  return HOPS_KNOWN;
}
static inline void time_utc(uint32_t stamp,char *out,size_t cap){
  if(!out||!cap)return;
  if(stamp<1577836800u){snprintf(out,cap,"--:--");return;}
  const uint32_t min=(stamp/60u)%60u;
  const uint32_t hour=(stamp/3600u)%24u;
  snprintf(out,cap,"%02lu:%02lu UTC",(unsigned long)hour,(unsigned long)min);
}
static inline void meta(uint32_t observed_at,uint8_t path,bool outgoing,
                        const char *outgoing_status,char *out,size_t cap){
  if(!out||!cap)return;
  char clock[16];time_utc(observed_at,clock,sizeof(clock));
  if(outgoing){snprintf(out,cap,"%s  |  %s",clock,outgoing_status?outgoing_status:"Sent");return;}
  uint8_t hops=0;
  const Route route=route_hops(path,hops);
  if(route==DIRECT_ROUTE)snprintf(out,cap,"%s  |  Direct route",clock);
  else if(route==HOPS_KNOWN)
    snprintf(out,cap,"%s  |  %u %s",clock,(unsigned)hops,hops==1?"hop":"hops");
  else snprintf(out,cap,"%s  |  Hops --",clock);
}
} // namespace mcxchat
