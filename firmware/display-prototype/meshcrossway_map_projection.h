#pragma once
#include <math.h>
#include <stdint.h>
namespace mcxmap {
struct Point {double x=0,y=0;};
static double wrap(double x,double size){x=fmod(x,size);return x<0?x+size:x;}
static Point project(double lat,double lon,unsigned zoom){const double size=256.0*double(uint32_t(1)<<zoom);lat=fmax(-85.05112878,fmin(85.05112878,lat));const double r=lat*3.14159265358979323846/180;return {wrap((lon+180)/360*size,size),(1-asinh(tan(r))/3.14159265358979323846)*size/2};}
static Point unproject(double x,double y,unsigned zoom){const double size=256.0*double(uint32_t(1)<<zoom);y=fmax(0,fmin(size,y));return {atan(sinh(3.14159265358979323846*(1-2*y/size)))*180/3.14159265358979323846,wrap(x,size)/size*360-180};}
static double delta(double x,double center,unsigned zoom){const double size=256.0*double(uint32_t(1)<<zoom);return wrap(x-center+size/2,size)-size/2;}
}
