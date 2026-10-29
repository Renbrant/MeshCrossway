// Deterministic host tests for edge-only LVGL gesture policy (C++11/C++20).
#include <cassert>
#include <cstdio>
#include "../firmware/display-prototype/meshcrossway_edge_swipe.h"
using namespace mcxswipe;

static Result gesture(int x1,int y1,int x2,int y2,int page,
                      bool left,bool right,bool lc,bool rc,int lw,int rw,
                      bool overlay=false,unsigned duration=180) {
  State s;
  assert(sample(s,true,x1,y1,page,left,right,lc,rc,lw,rw,overlay,100)==NONE);
  assert(sample(s,true,x2,y2,page,left,right,lc,rc,lw,rw,overlay,100+duration/2)==NONE);
  return sample(s,false,x2,y2,page,left,right,lc,rc,lw,rw,overlay,100+duration);
}
int main(){
  // Both directions on both edges; open takes priority over any other action.
  assert(gesture(10,100,70,104,0,true,false,true,true,40,0)==OPEN_LEFT);
  assert(gesture(65,120,12,122,0,true,false,false,true,79,0)==CLOSE_LEFT);
  assert(gesture(468,100,403,100,3,true,true,true,true,40,40)==OPEN_RIGHT);
  assert(gesture(365,120,428,124,4,true,true,true,false,40,125)==CLOSE_RIGHT);
  // Normal taps, long holds, vertical scrolling, header, center-map drags.
  assert(gesture(10,100,18,102,0,true,false,true,true,40,0)==NONE);
  assert(gesture(10,100,63,200,0,true,false,true,true,40,0)==NONE);
  assert(gesture(10,20,80,20,0,true,false,true,true,40,0)==NONE);
  assert(gesture(100,100,180,100,4,true,true,true,true,40,40)==NONE);
  assert(gesture(470,100,385,100,3,true,true,true,true,40,40,false,2200)==NONE);
  // No right gestures outside Chat and Map, no rails at all on Settings.
  assert(gesture(470,100,380,100,1,true,false,true,true,40,0)==NONE);
  assert(gesture(10,100,80,100,5,false,false,true,true,0,0)==NONE);
  // Modal screens (PIN keyboard, Wi-Fi, help and Quick Settings) own all touches.
  assert(gesture(10,100,100,100,3,true,true,true,true,40,40,true)==NONE);
  // Do not toggle twice on release, and do not toggle if navigation changed.
  State s;
  assert(sample(s,true,10,110,0,true,false,true,true,40,0,false,0)==NONE);
  assert(sample(s,true,90,110,0,true,false,true,true,40,0,false,75)==NONE);
  assert(sample(s,false,90,110,1,true,false,true,true,40,0,false,100)==NONE);
  assert(sample(s,false,90,110,1,true,false,true,true,40,0,false,105)==NONE);
  // No false positive if open/closed state changed during the press.
  assert(gesture(10,100,90,100,0,true,false,false,true,79,0)==NONE);
  // Closing requires a start near the INNER edge of an expanded rail.
  assert(gesture(5,100,0,100,0,true,false,false,true,79,0)==NONE);
  assert(gesture(430,90,475,91,3,true,true,true,false,40,125)==NONE);
  std::puts("PASS: 16 edge-swipe gesture scenarios; horizontal/vertical/modal rules");
  return 0;
}
