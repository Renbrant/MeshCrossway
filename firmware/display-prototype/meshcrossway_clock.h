#pragma once
#include "meshcrossway_gnss_parser.h"
#include "meshcrossway_gnss_settings.h"
#include "timezone/zone_data.h"
#include <time.h>
#include <stdio.h>
namespace mcxclock {
// One-based selection; zero means automatic GPS geographic lookup.
static unsigned selection(const mcxgnss::Settings &s){
  const unsigned id=unsigned(s.reserved[0])|(unsigned(s.reserved[1])<<8);
  if(id)for(unsigned i=0;i<mcxtz::count;++i)if(mcxtz::zones[i].id==id)return i+1;
  return 0;
}
static void select(mcxgnss::Settings &s,unsigned zone){const unsigned id=zone&&zone<=mcxtz::count?mcxtz::zones[zone-1].id:0;s.reserved[0]=uint8_t(id);s.reserved[1]=uint8_t(id>>8);}
struct State {
  bool synced=false;int zone=-1,candidate=-1;unsigned confirmations=0;
  int64_t anchor_ms=0,last_sync_ms=0;uint64_t utc=0;uint32_t seen_time=0,seen_position=0;
  // Time and geographic freshness are independent. Stale positions never move the zone.
  bool update(const mcxgnss::Fix &f,const mcxgnss::Settings &cfg,int64_t now_ms){
    bool resync=false;
    if(cfg.enabled&&f.have_time&&uint32_t(uint32_t(now_ms)-f.time_ms)<=cfg.timeout_ms&&(!synced||f.time_ms!=seen_time)){
      seen_time=f.time_ms;
      const uint64_t next=f.utc_seconds+uint32_t(uint32_t(now_ms)-f.time_ms)/1000;
      const uint64_t previous=epoch(now_ms);
      resync=!synced||now_ms-last_sync_ms>=60000||next>previous+2||previous>next+2;
      utc=f.utc_seconds;anchor_ms=now_ms-uint32_t(uint32_t(now_ms)-f.time_ms);synced=true;
      if(resync)last_sync_ms=now_ms;
    }
    const unsigned manual=selection(cfg);
    if(manual&&manual<=mcxtz::count){zone=int(manual-1);candidate=-1;confirmations=0;}
    else if(cfg.enabled&&f.valid&&uint32_t(uint32_t(now_ms)-f.position_ms)<=cfg.timeout_ms&&(!confirmations||f.position_ms!=seen_position)){
      seen_position=f.position_ms;const int found=mcxtz::lookup(f.latitude,f.longitude);
      if(found>=0){
        if(candidate!=found){candidate=found;confirmations=1;}else ++confirmations;
        // Immediate first zone; subsequent border changes require three fresh fixes.
        if(zone<0||confirmations>=3)zone=found;
      }
    }
    return resync;
  }
  uint64_t epoch(int64_t now_ms)const{return synced?utc+uint64_t((now_ms-anchor_ms)/1000):0;}
};
}
