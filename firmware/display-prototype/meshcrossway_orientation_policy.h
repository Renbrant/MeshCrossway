#pragma once
#include <stdint.h>
#include <math.h>
namespace mcxorientation {
// Sensor X is the landscape vertical axis. Learn its sign once, so PCB/case
// mounting does not impose a hard-coded definition of the upright edge.
static inline int gravity_sign(float x,float y,float z){
  const float n=x*x+y*y+z*z;
  if(!isfinite(n)||n<0.70f*0.70f||n>1.30f*1.30f||
     fabsf(x)<0.65f||fabsf(x)<fabsf(y)+0.20f||fabsf(z)>0.75f)return 0;
  return x>0?1:-1;
}
struct Policy {
  int normal=0,candidate=0;uint32_t since=0,last=0;bool has_rotated=false;
  void reset(){candidate=0;}
  // A full second of consistent gravity; no response to flat placement,
  // transients, expired data, active gestures, or changes while locked.
  bool update(int sign,uint32_t now,bool locked,bool busy,bool current,bool &target){
    if(locked||busy||!sign){reset();return false;}
    if(candidate!=sign){candidate=sign;since=now;return false;}
    if(uint32_t(now-since)<1000)return false;
    if(!normal){normal=current?-sign:sign;return false;}
    target=sign!=normal;
    if(target==current||(has_rotated&&uint32_t(now-last)<2000))return false;
    return true;
  }
  void applied(uint32_t now){last=now;has_rotated=true;reset();}
};
static inline void transform(bool flipped,int16_t &x,int16_t &y){
  if(flipped){x=int16_t(479-x);y=int16_t(319-y);}
}
}
