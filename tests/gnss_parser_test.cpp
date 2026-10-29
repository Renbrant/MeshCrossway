#include "../firmware/display-prototype/meshcrossway_gnss_parser.h"
#include <cassert>
#include <cstdio>
static void feed(mcxgnss::Parser &p,const char *s,unsigned now){for(;*s;++s)p.feed(*s,now);}
int main(){mcxgnss::Parser p;
feed(p,"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",100);
assert(p.fix.valid&&p.fix.satellites==8&&fabs(p.fix.latitude-48.1173)<1e-6&&fabs(p.fix.longitude-11.51666667)<1e-6);
assert(p.fix.have_hdop&&fabs(p.fix.hdop-0.9)<1e-6);assert(!p.snapshot(3200).valid);
const auto old=p.fix.latitude;feed(p,"$GPGGA,bogus*00\n",200);assert(p.fix.latitude==old&&p.fix.errors==1);
feed(p,"$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n",300);assert(p.fix.valid&&p.fix.speed_kmh>41);
for(int i=0;i<1000;++i){p.feed(i==0?'$':'X',400);}
assert(p.fix.errors>=2);
feed(p,"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",500);assert(p.fix.valid&&p.fix.frames==3);
puts("PASS GNSS checksum, GGA/RMC, coordinates, speed, stale fix, overflow and recovery");}
