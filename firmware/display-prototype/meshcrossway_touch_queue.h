#pragma once
#include <stdint.h>
// Single producer/consumer, externally locked. Never discard an edge silently.
namespace mcxtouch {
struct Sample { int16_t x=0,y=0; uint32_t ms=0; bool down=false,cancel=false; };
template<unsigned N=64> struct Queue {
  static_assert(N>=4,"Need room for edges and motion");
  Sample items[N]={},last={};
  unsigned head=0,count=0,peak=0,overflows=0;
  bool quarantine=false;
  void cancel(uint32_t ms){
    head=0;count=1;items[0]=last;items[0].down=false;
    items[0].cancel=true;items[0].ms=ms;quarantine=true;
  }
  void push(Sample s){
    if(quarantine){if(!s.down){quarantine=false;last=s;}return;}
    const bool edge=s.down!=last.down;
    last=s;
    if(!edge&&!s.down)return;
    if(count==N){
      // The last queued point may be replaced only by another MOVE, never UP
      // or DOWN. On edge overflow cancel the gesture and wait for release.
      if(!edge&&count>1&&items[(head+count-1)%N].down&&items[(head+count-2)%N].down){
        items[(head+count-1)%N]=s;return;
      }
      ++overflows;cancel(s.ms);return;
    }
    items[(head+count)%N]=s;++count;if(count>peak)peak=count;
  }
  bool pop(Sample &s){if(!count)return false;s=items[head];head=(head+1)%N;--count;return true;}
  bool busy()const{return count||last.down||quarantine;}
};
}
