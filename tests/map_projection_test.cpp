#include "../firmware/display-prototype/meshcrossway_map_projection.h"
#include "../firmware/display-prototype/meshcrossway_gnss_settings.h"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned z=3;z<=19;++z){auto p=mcxmap::project(45.123456,-110.654321,z);auto q=mcxmap::unproject(p.x,p.y,z);assert(fabs(q.x-45.123456)<1e-7&&fabs(q.y+110.654321)<1e-7);}
 auto p=mcxmap::project(0,0,0);assert(p.x==128&&p.y==128);
 auto a=mcxmap::project(0,179.999,10),b=mcxmap::project(0,-179.999,10);assert(fabs(mcxmap::delta(a.x,b.x,10))<2);
 p=mcxmap::project(90,0,19);assert(isfinite(p.y)&&p.y>-1);
 mcxgnss::Settings s;assert(mcxgnss::valid_settings(s));s.baud=123;assert(!mcxgnss::valid_settings(s));s.baud=38400;s.timeout_ms=0;assert(!mcxgnss::valid_settings(s));
 puts("PASS: Mercator round trips, poles, dateline and GPS settings bounds");
}
