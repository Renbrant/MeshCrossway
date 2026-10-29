#pragma once
// AXP2101 read-only decoder. The chip estimates state of charge from voltage;
// percentage is approximate, particularly during load or charging transitions.
// All values originate in READ-ONLY PMU registers (0x03, 0x00, 0x01, 0xA4).
#include <stdint.h>
namespace mcxbattery {
struct Reading {
  bool valid;
  bool battery_present;
  bool usb_present;
  bool charging;
  bool full;
  uint8_t percent;
};
static inline Reading unknown(){return Reading{false,false,false,false,false,0};}
static inline Reading decode(uint8_t chip_id,uint8_t status1,uint8_t status2,uint8_t soc){
  Reading out=unknown();
  if(chip_id!=0x4Au)return out;
  out.valid=true;
  out.battery_present=(status1 & 0x08u)!=0;
  out.usb_present=(status1 & 0x20u)!=0;
  const uint8_t state=status2&0x07u;
  out.charging=out.battery_present && out.usb_present && state<=3u;
  out.full=out.battery_present && out.usb_present && state==4u;
  // SOC may temporarily be 0xFF or inaccurate before a battery is detected.
  // Never present a battery percentage if the battery is absent.
  if(!out.battery_present || soc>100u)out.percent=255u;
  else out.percent=soc;
  return out;
}
static inline uint32_t pack(const Reading &s){
  return uint32_t(s.percent) |
         (uint32_t(s.valid)<<8) |
         (uint32_t(s.battery_present)<<9) |
         (uint32_t(s.usb_present)<<10) |
         (uint32_t(s.charging)<<11) |
         (uint32_t(s.full)<<12);
}
static inline Reading unpack(uint32_t state){
  return Reading{(state&0x100u)!=0,(state&0x200u)!=0,
                 (state&0x400u)!=0,(state&0x800u)!=0,
                 (state&0x1000u)!=0,uint8_t(state&0xFFu)};
}
} // namespace mcxbattery
