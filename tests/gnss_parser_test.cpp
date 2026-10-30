#include "../firmware/display-prototype/meshcrossway_gnss_parser.h"
#include <cassert>
#include <cstdio>
static void feed(mcxgnss::Parser &p,const char *s,unsigned now){for(;*s;++s)p.feed(*s,now);}
static void sentence(mcxgnss::Parser &p,const char *body,unsigned now){unsigned checksum=0;for(auto *q=body;*q;++q)checksum^=(unsigned char)*q;char data[220];snprintf(data,sizeof(data),"$%s*%02X\r\n",body,checksum);feed(p,data,now);}
int main(){mcxgnss::Parser p;
feed(p,"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",100);
assert(p.fix.valid&&p.fix.satellites==8&&fabs(p.fix.latitude-48.1173)<1e-6&&fabs(p.fix.longitude-11.51666667)<1e-6);
assert(p.fix.have_hdop&&fabs(p.fix.hdop-0.9)<1e-6);assert(!p.snapshot(3200).valid&&!p.snapshot(3200).have_satellites&&!p.snapshot(3200).have_hdop);
const auto old=p.fix.latitude;feed(p,"$GPGGA,bogus*00\n",200);assert(p.fix.latitude==old&&p.fix.errors==1);
feed(p,"$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n",300);assert(p.fix.valid&&p.fix.speed_kmh>41);
uint64_t epoch=0;assert(mcxgnss::Parser::utc("000000","010170",epoch)&&epoch==3155760000LL);assert(mcxgnss::Parser::utc("000000.000","290224",epoch)&&epoch==1709164800);assert(!mcxgnss::Parser::utc("240000","010126",epoch));assert(!mcxgnss::Parser::utc("120000","290223",epoch));
for(int i=0;i<1000;++i){p.feed(i==0?'$':'X',400);}
assert(p.fix.errors>=2);
feed(p,"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",500);assert(p.fix.valid&&p.fix.frames==3);
assert(!p.fix.have_accuracy); // HDOP is not converted into invented metres.
const auto position_ms=p.fix.position_ms;
sentence(p,"PUBX,00,123519.00,4807.038,N,01131.000,E,545.4,G3,2.1,2.0,0.0,0.0,0.0,,0.9,1.0,1.0,8,0,0",600);
assert(p.fix.have_accuracy&&p.fix.accuracy_source==1&&fabs(p.fix.horizontal_accuracy-2.1)<1e-6&&p.fix.position_ms==position_ms);
sentence(p,"GPGST,123519.00,1.8,,,,1.7,1.3,2.2",700);
assert(p.fix.have_accuracy&&p.fix.accuracy_source==2&&fabs(p.fix.horizontal_accuracy-hypot(1.7,1.3))<1e-6);
assert(!p.snapshot(3701).have_accuracy); // Accuracy and position expire independently.
sentence(p,"PUBX,00,123519.00,4807.038,N,01131.000,E,545.4,G3,nan,2.0,0.0,0.0,0.0,,0.9,1.0,1.0,8,0,0",800);assert(!p.fix.have_accuracy);
sentence(p,"PUBX,00,123519.00,4807.038,N,01131.000,E,545.4,NF,2.1,2.0,0.0,0.0,0.0,,0.9,1.0,1.0,8,0,0",900);assert(!p.fix.have_accuracy);
sentence(p,"GNGGA,123519,4807.038,N,01131.000,E,5,08,0.9,545.4,M,46.9,M,,",1000);assert(p.fix.have_quality&&p.fix.quality==5);
sentence(p,"GNGGA,123519,4807.038,N,01131.000,E,4,08,0.9,545.4,M,46.9,M,,",1100);assert(p.fix.quality==4);
sentence(p,"GPRMC,123519,A,4807.038,N,01131.000,E,0,0,301026,,,A",4200);assert(p.snapshot(4200).valid&&!p.snapshot(4200).have_quality);
sentence(p,"GPGST,123519.00,1.8,,,,-1,1.3,2.2",4300);assert(!p.fix.have_accuracy);
sentence(p,"GPGST,123519.00,1.8,,,,0,0,2.2",4400);assert(!p.fix.have_accuracy);
sentence(p,"GPGST,123519.00,1.8,,,,1.7,1.3,2.2",4500);assert(p.fix.have_accuracy);
sentence(p,"GNGGA,123519,,,,,0,0,99,,,,,,",4600);assert(!p.fix.valid&&!p.fix.have_accuracy);
puts("PASS GNSS accuracy: PUBX hAcc, GST hRMS, quality, unavailable/invalid/stale/no-fix handling and no fabricated HDOP metres");
puts("PASS GNSS checksum, GGA/RMC, coordinates, speed, stale fix, overflow and recovery");}
