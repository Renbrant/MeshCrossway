// Pure chat sender / time / hops fixtures compiled under C++11 + C++20.
#include <cassert>
#include <cstring>
#include <cstdio>
#include "../firmware/display-prototype/meshcrossway_chat_presentation.h"
int main(){
  using namespace mcxchat;
  const char *literal="CO-PW-HQ-MC01: @[emoji] HYPERSPACE PRIM 5 to PW";
  const Sender v=channel_sender(literal);
  assert(v.claimed && strcmp(v.name,"CO-PW-HQ-MC01")==0);
  assert(strcmp(v.body,"@[emoji] HYPERSPACE PRIM 5 to PW")==0);
  assert(strcmp(literal,"CO-PW-HQ-MC01: @[emoji] HYPERSPACE PRIM 5 to PW")==0);
  const char *with_emoji="🏁RenBrant: Test with emoji";
  const Sender e=channel_sender(with_emoji);
  assert(e.claimed && strcmp(e.name,"🏁RenBrant")==0);
  assert(strcmp(e.body,"Test with emoji")==0);
  assert(!channel_sender("http://example.org/m").claimed);
  assert(!channel_sender(": missing").claimed);
  assert(!channel_sender("This line has no sender").claimed);
  assert(!channel_sender("Bad\nname: text").claimed);
  const Sender empty=channel_sender(nullptr);
  assert(!empty.claimed && strcmp(empty.body,"")==0);
  assert(color_index("Alice")==color_index("Alice"));
  uint8_t hops=250;
  assert(route_hops(0x04,hops)==HOPS_KNOWN && hops==4);
  assert(route_hops(0x42,hops)==HOPS_KNOWN && hops==2);
  assert(route_hops(0x80,hops)==HOPS_KNOWN && hops==0);
  assert(route_hops(0xFF,hops)==DIRECT_ROUTE && hops==0);
  assert(route_hops(0xC1,hops)==ROUTE_UNKNOWN);
  assert(route_hops(0x7F,hops)==ROUTE_UNKNOWN);
  char result[80],tm[20];
  time_utc(1704067200u,tm,sizeof(tm)); assert(strcmp(tm,"00:00 UTC")==0);
  time_utc(1704067260u,tm,sizeof(tm)); assert(strcmp(tm,"00:01 UTC")==0);
  time_utc(0,tm,sizeof(tm)); assert(strcmp(tm,"--:--")==0);
  meta(1704067200u,0x04,false,nullptr,result,sizeof(result));
  assert(strcmp(result,"00:00 UTC  |  4 hops")==0);
  meta(1704067200u,0xff,false,nullptr,result,sizeof(result));
  assert(strcmp(result,"00:00 UTC  |  Direct route")==0);
  meta(1704067200u,0xc1,false,nullptr,result,sizeof(result));
  assert(strcmp(result,"00:00 UTC  |  Hops --")==0);
  meta(1704067200u,0xff,true,"Delivered",result,sizeof(result));
  assert(strcmp(result,"00:00 UTC  |  Delivered")==0);
  puts("PASS: stable colors, sender claims, genuine UTC timestamps and MeshCore hops");
  return 0;
}
