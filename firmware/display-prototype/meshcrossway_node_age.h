#pragma once
#include <stdint.h>
#include <atomic>
namespace mcxnodeage {
static constexpr uint32_t DAY=86400u,VALID_TIME=1577836800u;
static std::atomic<unsigned> retention{0}; // Activated only after saved config loads.
static std::atomic<uint32_t> epoch{0};
// All views and both importers use the same clock/threshold. Never infer age
// from sync time, uptime, a missing timestamp or a timestamp in the future.
static bool expired_at(uint32_t heard,uint32_t now,unsigned days){
  return days&&heard>=VALID_TIME&&now>=VALID_TIME&&heard<=now&&uint64_t(now-heard)>uint64_t(days)*DAY;
}
static bool expired(uint32_t heard){return expired_at(heard,epoch.load(),retention.load());}
static uint32_t color(uint32_t heard,uint32_t now){
  if(heard<VALID_TIME||now<VALID_TIME||heard>now)return 0x8993A1;
  const uint32_t age=now-heard;
  return age<DAY?0x36D399:age<7u*DAY?0xFFD45A:0x8993A1;
}
// UI installs a stable-key lookup for app-local MeshCore favorites/drafts.
using PinLookup=bool (*)(const uint8_t *);
static PinLookup &pin_lookup(){static PinLookup lookup=nullptr;return lookup;}
}
