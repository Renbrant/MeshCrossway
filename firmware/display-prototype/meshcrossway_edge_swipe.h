#pragma once
// MeshCrossway v0.7.7: pure, deterministic edge-swipe classifier.
// Screen: 480x320 landscape. The top 34px header is excluded.
// Display-only state change. No network, radio, storage or LVGL coupling.
#include <stdint.h>
namespace mcxswipe {
enum Result : uint8_t { NONE=0, OPEN_LEFT, CLOSE_LEFT, OPEN_RIGHT, CLOSE_RIGHT };
enum Origin : uint8_t { NOT_ARMED=0, LEFT_OPEN_ZONE, LEFT_CLOSE_ZONE,
                        RIGHT_OPEN_ZONE, RIGHT_CLOSE_ZONE };
struct State {
  bool down=false;
  Origin origin=NOT_ARMED;
  int16_t start_x=0,start_y=0,last_x=0,last_y=0;
  int started_on_page=-1;
  uint32_t start_ms=0;
};
static inline void reset(State &s){s=State{};}
static inline int iabs(int x){return x<0?-x:x;}
static inline Origin arm(int x,int y,int page,bool left,bool right,
                         bool left_compact,bool right_compact,int lw,int rw){
  if(y<34||y>=320||x<0||x>=480)return NOT_ARMED;
  // Start on the actual rail. Never intercept content swipes, map pans,
  // text selection, header Quick Settings, or a Settings page without rails.
  if(left&&lw>0){
    if(left_compact&&x<=22)return LEFT_OPEN_ZONE;
    if(!left_compact&&x>=lw-38&&x<lw)return LEFT_CLOSE_ZONE;
  }
  if(right&&rw>0){
    if(right_compact&&x>=457)return RIGHT_OPEN_ZONE;
    if(!right_compact&&x>=480-rw&&x<=480-rw+38)return RIGHT_CLOSE_ZONE;
  }
  (void)page;
  return NOT_ARMED;
}
static inline Result sample(State &s,bool down,int x,int y,int page,
                            bool left,bool right,bool left_compact,
                            bool right_compact,int lw,int rw,
                            bool overlay,uint32_t ms){
  if(overlay||(!left&&!right)){reset(s);return NONE;}
  if(down){
    if(!s.down){
      s.down=true;s.start_x=x;s.start_y=y;s.last_x=x;s.last_y=y;
      s.started_on_page=page;s.start_ms=ms;
      s.origin=arm(x,y,page,left,right,left_compact,right_compact,lw,rw);
    }else{s.last_x=x;s.last_y=y;}
    return NONE;
  }
  if(!s.down)return NONE;
  // LVGL keeps the last pointer coordinate across RELEASED. Capture the
  // final sample so a quick swipe does not depend on timer sampling phase.
  if(x>=0&&x<480&&y>=0&&y<320){s.last_x=x;s.last_y=y;}
  const Origin from=s.origin;
  const int dx=int(s.last_x)-int(s.start_x);
  const int dy=int(s.last_y)-int(s.start_y);
  const uint32_t elapsed=ms-s.start_ms;
  const bool same_page=s.started_on_page==page;
  reset(s);
  if(!same_page||elapsed>1800||iabs(dx)<44||iabs(dx)<2*iabs(dy))
    return NONE;
  switch(from){
    case LEFT_OPEN_ZONE:  return left&&left_compact&&dx>0 ? OPEN_LEFT:NONE;
    case LEFT_CLOSE_ZONE: return left&&!left_compact&&dx<0 ? CLOSE_LEFT:NONE;
    case RIGHT_OPEN_ZONE: return right&&right_compact&&dx<0 ? OPEN_RIGHT:NONE;
    case RIGHT_CLOSE_ZONE:return right&&!right_compact&&dx>0 ? CLOSE_RIGHT:NONE;
    default: return NONE;
  }
}
} // namespace mcxswipe
