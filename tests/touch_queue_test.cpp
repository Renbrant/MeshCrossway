#include <cassert>
#include <cstdio>
#include "../firmware/display-prototype/meshcrossway_touch_queue.h"
using namespace mcxtouch;
static Sample point(bool down,int x,unsigned ms){Sample s;s.down=down;s.x=x;s.y=100;s.ms=ms;return s;}
int main(){
  Queue<4> q;Sample s;
  q.push(point(true,10,0));q.push(point(true,20,10));
  q.push(point(true,30,20));q.push(point(true,40,30));
  q.push(point(true,50,40)); // MOVE coalescing preserves the original DOWN.
  assert(q.count==4&&q.peak==4&&q.overflows==0);
  assert(q.pop(s)&&s.down&&s.x==10&&s.ms==0);
  q.push(point(false,50,60));
  assert(q.pop(s)&&s.x==20);assert(q.pop(s)&&s.x==30);
  assert(q.pop(s)&&s.x==50&&s.down);assert(q.pop(s)&&!s.down&&s.ms==60);
  assert(!q.busy());
  // Overflow at an edge explicitly cancels, then quarantines until physical UP.
  q.push(point(true,10,100));q.push(point(false,10,110));
  q.push(point(true,20,120));q.push(point(false,20,130));q.push(point(true,30,140));
  assert(q.overflows==1&&q.count==1&&q.quarantine);
  assert(q.pop(s)&&s.cancel&&!s.down);
  q.push(point(true,40,150));assert(!q.count);
  q.push(point(false,40,160));assert(!q.busy());
  q.push(point(true,50,170));assert(q.pop(s)&&s.down&&!s.cancel);
  q.cancel(180);assert(q.pop(s)&&s.cancel&&!s.down);
  q.push(point(false,50,190));assert(!q.busy());
  // A complete short tap remains available when the consumer is delayed.
  q.push(point(true,12,200));q.push(point(false,12,260));
  assert(q.pop(s)&&s.down&&s.ms==200);assert(q.pop(s)&&!s.down&&s.ms==260);
  puts("PASS: touch FIFO preserves edges/timestamps, coalesces motion, cancels overflow, quarantines and retains a delayed short tap");
}
