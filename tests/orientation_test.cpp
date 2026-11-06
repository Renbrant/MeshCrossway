#include <cassert>
#include <initializer_list>
#include <limits>
#include "meshcrossway_orientation_policy.h"
using namespace mcxorientation;
int main(){
  assert(gravity_sign(1,0,0)==1&&gravity_sign(-1,0,0)==-1);
  assert(!gravity_sign(0,0,1)&&!gravity_sign(.7f,.7f,0));
  assert(gravity_sign(0,1,0)==2&&gravity_sign(0,-1,0)==-2);
  assert(!gravity_sign(1.5f,0,0)&&!gravity_sign(0,0,0));
  assert(!gravity_sign(std::numeric_limits<float>::quiet_NaN(),0,0));
  Policy p;bool target=false;
  assert(!p.update(1,100,false,false,false,target));
  assert(!p.update(1,1100,false,false,false,target)&&p.normal==1);
  assert(!p.update(-1,1200,false,false,false,target));
  assert(!p.update(-1,2000,false,false,false,target));
  assert(!p.update(0,2100,false,false,false,target)); // flat/expired clears debounce
  assert(!p.update(-1,2200,false,false,false,target));
  assert(p.update(-1,3200,false,false,false,target)&&target);
  p.applied(3200);
  assert(!p.update(1,3300,false,false,true,target));
  assert(!p.update(1,4300,false,false,true,target)); // cooldown
  assert(p.update(1,5300,false,false,true,target)&&!target);
  p.applied(5300);
  assert(!p.update(-1,8000,true,false,false,target));
  assert(!p.update(-1,9000,false,true,false,target));
  assert(!p.update(-1,9100,false,false,false,target));
  assert(p.update(-1,10100,false,false,false,target));
  Policy inverted;assert(!inverted.update(-1,0,false,false,true,target));
  assert(!inverted.update(-1,1000,false,false,true,target)&&inverted.normal==1);
  Policy yaxis;
  assert(!yaxis.update(2,0,false,false,false,target));
  assert(!yaxis.update(2,1000,false,false,false,target)&&yaxis.normal==2);
  assert(!yaxis.update(1,2000,false,false,false,target));
  assert(!yaxis.update(1,4000,false,false,false,target)); // portrait edge
  assert(!yaxis.update(-2,5000,false,false,false,target));
  assert(yaxis.update(-2,6000,false,false,false,target)&&target);
  Policy wrap;wrap.normal=1;
  assert(!wrap.update(-1,UINT32_MAX-500,false,false,false,target));
  assert(wrap.update(-1,600,false,false,false,target));
  for(int16_t x:{int16_t(0),int16_t(479)})for(int16_t y:{int16_t(0),int16_t(319)}){
    int16_t a=x,b=y;transform(true,a,b);assert(a==479-x&&b==319-y);
    transform(true,a,b);assert(a==x&&b==y);
    transform(false,a,b);assert(a==x&&b==y);
  }
}
