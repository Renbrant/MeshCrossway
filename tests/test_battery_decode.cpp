// Host-only read-only power-status tests. Do not require IDF/LVGL.
#include <cassert>
#include <cstdio>
#include "../firmware/display-prototype/meshcrossway_battery_decode.h"
using namespace mcxbattery;
int main(){
  const Reading invalid=decode(0x00,0x28,0x01,77);
  assert(!invalid.valid && !invalid.charging);
  const Reading charging=decode(0x4A,0x28,0x01,78);
  assert(charging.valid && charging.battery_present && charging.usb_present);
  assert(charging.charging && !charging.full && charging.percent==78);
  const Reading discharging=decode(0x4A,0x08,0x45,33);
  assert(discharging.valid && !discharging.usb_present && !discharging.charging);
  assert(discharging.percent==33);
  const Reading completed=decode(0x4A,0x28,0x04,100);
  assert(!completed.charging && completed.full && completed.percent==100);
  const Reading no_battery=decode(0x4A,0x20,0x05,64);
  assert(no_battery.valid && !no_battery.battery_present);
  assert(!no_battery.charging && no_battery.percent==255);
  const Reading invalid_soc=decode(0x4A,0x28,0x01,0xFF);
  assert(invalid_soc.valid && invalid_soc.percent==255);
  assert(invalid_soc.charging);
  const Reading mirror=unpack(pack(charging));
  assert(mirror.valid&&mirror.battery_present&&mirror.usb_present);
  assert(mirror.charging&&mirror.percent==78);
  assert(!unpack(0).valid);
  std::puts("PASS: read-only AXP2101 register decode, charging, absent & invalid battery");
  return 0;
}
