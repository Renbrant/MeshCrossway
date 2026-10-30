#include "../firmware/display-prototype/meshcrossway_clock.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace mcxclock;
static void sentence(mcxgnss::Parser &p,const char *body,uint32_t ms){unsigned checksum=0;for(auto *s=body;*s;++s)checksum^=uint8_t(*s);char full[180];snprintf(full,sizeof(full),"$%s*%02X\r\n",body,checksum);for(auto *s=full;*s;++s)p.feed(*s,ms);}
static uint64_t stamp(const char *date,const char *clock){uint64_t t=0;assert(mcxgnss::Parser::utc(clock,date,t));return t;}
static int zone(const char *name){for(unsigned i=0;i<mcxtz::count;++i)if(!strcmp(name,mcxtz::zones[i].name))return int(i);assert(false);return -1;}
static void local(const char *name,const char *date,const char *clock,const char *expected){setenv("TZ",mcxtz::zones[zone(name)].rule,1);tzset();time_t epoch=stamp(date,clock);tm t={};localtime_r(&epoch,&t);char result[32];strftime(result,sizeof(result),"%Y-%m-%d %H:%M",&t);assert(!strcmp(result,expected));}
int main(){
 uint64_t value=0;assert(stamp("010126","000000")==1767225600);
 assert(stamp("290224","235959.50")==1709251199);
 for(const char *date:{"290223","310426","001026","321026","011326","011019","abcdef"})assert(!mcxgnss::Parser::utc("123456",date,value));
 for(const char *clock:{"240000","126000","123460","12345","123456x","123456.","123456.NaN"})assert(!mcxgnss::Parser::utc(clock,"301026",value));
 mcxgnss::Parser parser;sentence(parser,"GNRMC,120000.00,A,3952.000,N,10458.000,W,0.0,0.0,301026,,,A",1000);
 assert(parser.fix.valid&&parser.fix.have_time&&parser.fix.utc_seconds==stamp("301026","120000"));
 const auto utc=parser.fix.utc_seconds;
 sentence(parser,"GNRMC,120001.00,V,,,,,,,301026,,,N",2000);assert(parser.fix.utc_seconds==utc&&parser.fix.time_ms==1000);
 sentence(parser,"GNRMC,250000.00,A,3952.000,N,10458.000,W,0.0,0.0,311126,,,A",3000);assert(parser.fix.time_ms==1000);
 assert(mcxtz::lookup(39.868,-104.972)==zone("America/Denver"));
 assert(mcxtz::lookup(33.448,-112.074)==zone("America/Phoenix"));
 assert(mcxtz::lookup(-23.55,-46.63)==zone("America/Sao_Paulo"));
 assert(mcxtz::lookup(27.717,85.324)==zone("Asia/Kathmandu"));
 assert(mcxtz::lookup(std::numeric_limits<double>::quiet_NaN(),0)==-1);
 assert(mcxtz::lookup(91,0)==-1);
 local("America/Denver","080326","085959","2026-03-08 01:59");
 local("America/Denver","080326","090000","2026-03-08 03:00");
 local("America/Denver","011126","075959","2026-11-01 01:59");
 local("America/Denver","011126","080000","2026-11-01 01:00");
 local("America/Phoenix","010726","120000","2026-07-01 05:00");
 local("America/Sao_Paulo","010726","120000","2026-07-01 09:00");
 local("Asia/Kathmandu","301026","220000","2026-10-31 03:45");
 local("Australia/Adelaide","011026","120000","2026-10-01 21:30");
 local("Australia/Adelaide","051026","120000","2026-10-05 22:30");
 State s;mcxgnss::Settings cfg;mcxgnss::Fix f;
 assert(!s.update(f,cfg,1000)&&!s.synced&&s.zone==-1);
 f.have_time=f.valid=true;f.time_ms=f.position_ms=1000;f.utc_seconds=utc;f.latitude=39.868;f.longitude=-104.972;
 assert(s.update(f,cfg,1000)&&s.synced&&s.zone==zone("America/Denver"));
 assert(!s.update(f,cfg,2000));assert(s.epoch(61000)==utc+60);
 f.longitude=-112.074;f.latitude=33.448;
 for(unsigned i=1;i<=3;++i){f.position_ms=1000+i*1000;s.update(f,cfg,f.position_ms);assert(s.zone==zone(i<3?"America/Denver":"America/Phoenix"));}
 select(cfg,zone("Asia/Kathmandu")+1);assert(selection(cfg)==unsigned(zone("Asia/Kathmandu")+1));s.update(f,cfg,5000);assert(s.zone==zone("Asia/Kathmandu"));
 cfg.enabled=0;f.time_ms=6000;f.utc_seconds=utc+1000;assert(!s.update(f,cfg,6000));assert(s.epoch(61000)==utc+60);
 // Freshness arithmetic survives the 32-bit millisecond wrap; holdover uses 64 bits.
 cfg.enabled=1;State wrap;f.time_ms=UINT32_MAX-500;f.position_ms=f.time_ms;f.utc_seconds=utc;
 assert(wrap.update(f,cfg,int64_t(UINT32_MAX)+500));assert(wrap.epoch(int64_t(UINT32_MAX)+1500)==utc+2);
 puts("PASS: GPS UTC validation, offline zones, DST transitions, fractional offsets, holdover, override and millisecond wrap");
}
