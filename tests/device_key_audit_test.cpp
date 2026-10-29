#include "../firmware/display-prototype/meshcrossway_device_key.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static mcxkey::Block empty() {
  const mcxkey::Block v = {0, true, false, false, false};
  return v;
}
static mcxkey::Block used(unsigned purpose, bool rd, bool wr, bool purpose_wr) {
  const mcxkey::Block v = {static_cast<uint8_t>(purpose), false, rd, wr, purpose_wr};
  return v;
}
static void fill_empty(mcxkey::Block (&a)[mcxkey::kKeySlots]) {
  for (unsigned i=0;i<mcxkey::kKeySlots;++i) a[i]=empty();
}
int main() {
  mcxkey::Block b[mcxkey::kKeySlots];
  fill_empty(b);
  auto a=mcxkey::classify(b);
  assert(a.state==mcxkey::State::UNPROVISIONED);
  assert(a.available==6 && !a.hmac_candidates && !a.ready_for_storage);
  b[0]=used(2,true,true,true); // existing flash encryption key must not be taken
  b[1]=used(9,false,true,true); // secure boot digest
  a=mcxkey::classify(b);
  assert(a.available==4 && a.state==mcxkey::State::UNPROVISIONED);
  b[2]=used(mcxkey::kHmacUpPurpose,true,true,true);
  a=mcxkey::classify(b);
  assert(a.available==3 && a.hmac_candidates==1);
  assert(a.state==mcxkey::State::HMAC_PRESENT_UNVERIFIED);
  assert(!a.ready_for_storage); // never adopt someone else's HMAC key
  char text[300];
  mcxkey::describe(a,text,sizeof(text));
  assert(strstr(text,"NOT ACTIVE"));
  assert(strstr(text,"NOT ENROLLED"));
  assert(!strstr(text,"KEY2")); // don't publish physical slot identifiers
  b[2]=used(mcxkey::kHmacUpPurpose,false,true,true);
  a=mcxkey::classify(b);
  assert(a.state==mcxkey::State::UNSAFE_HMAC);
  b[2]=used(mcxkey::kHmacUpPurpose,true,false,true);
  assert(mcxkey::classify(b).state==mcxkey::State::UNSAFE_HMAC);
  b[2]=used(mcxkey::kHmacUpPurpose,true,true,false);
  assert(mcxkey::classify(b).state==mcxkey::State::UNSAFE_HMAC);
  b[2]=empty();b[2].write_protected=true;
  assert(mcxkey::classify(b).state==mcxkey::State::INCONSISTENT);
  fill_empty(b);b[0]=used(255,false,false,false);
  assert(mcxkey::classify(b).state==mcxkey::State::INCONSISTENT);
  for(unsigned i=0;i<mcxkey::kKeySlots;++i) b[i]=used(2,true,true,true);
  a=mcxkey::classify(b);
  assert(a.available==0 && a.state==mcxkey::State::NO_FREE_BLOCKS);
  mcxkey::describe(a,nullptr,0);
  char tiny[2]={'X','X'};
  mcxkey::describe(a,tiny,sizeof(tiny));
  assert(tiny[1]==0);
  puts("PASS: device eFuse audit read-only classifier / 10 scenarios");
}
