#pragma once
// Small, bounded Mapbox Vector Tile (MVT) reader and rasterizer.
// It intentionally implements only the protobuf/MVT fields needed for a
// compact base map; it is not a general MapLibre style engine.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

namespace mcxofm {
static constexpr size_t MAX_TILE_BYTES=512u*1024u;
static constexpr unsigned MAX_FEATURES=4096,MAX_COMMANDS=120000,MAX_LABELS=0;
struct Reader {const uint8_t *p,*end;bool ok;Reader(const uint8_t *a=nullptr,const uint8_t *b=nullptr,bool good=true):p(a),end(b),ok(good){}};
static uint64_t varint(Reader &r){uint64_t v=0;unsigned shift=0;while(r.p<r.end&&shift<64){uint8_t b=*r.p++;if(shift==63&&(b&0x7e)){r.ok=false;return 0;}v|=uint64_t(b&127)<<shift;if(!(b&128))return v;shift+=7;}r.ok=false;return 0;}
static bool field(Reader &r,unsigned &number,unsigned &wire){const auto t=varint(r);if(!r.ok||!t){r.ok=false;return false;}number=unsigned(t>>3);wire=unsigned(t&7);if(!number||wire==3||wire==4||wire>5){r.ok=false;return false;}return true;}
static bool skip(Reader &r,unsigned wire){switch(wire){case 0:(void)varint(r);return r.ok;case 1:if(size_t(r.end-r.p)<8)return r.ok=false;r.p+=8;return true;case 2:{const auto n=varint(r);if(!r.ok||n>size_t(r.end-r.p))return r.ok=false;r.p+=size_t(n);return true;}case 5:if(size_t(r.end-r.p)<4)return r.ok=false;r.p+=4;return true;default:r.ok=false;return false;}}
static Reader bytes(Reader &r){const auto n=varint(r);if(!r.ok||n>size_t(r.end-r.p)){r.ok=false;return {r.p,r.p,false};}Reader out{r.p,r.p+size_t(n),true};r.p+=size_t(n);return out;}
static int32_t zigzag(uint32_t n){return int32_t((n>>1)^uint32_t(-int32_t(n&1)));}
struct Point {int x,y;};
struct Scratch {Point points[512];int crossings[512];};
struct Canvas {uint16_t *pixels=nullptr;Point *points=nullptr;int *crossings=nullptr;unsigned width=0,height=0;int origin_x=0,origin_y=0,tile_x=0,tile_y=0;unsigned zoom=0;};
static uint16_t color(uint32_t rgb){return uint16_t(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));}
static void pixel(Canvas &c,int x,int y,uint16_t v){if(x>=0&&y>=0&&unsigned(x)<c.width&&unsigned(y)<c.height)c.pixels[size_t(y)*c.width+unsigned(x)]=v;}
static void line(Canvas &c,int x0,int y0,int x1,int y1,uint16_t v){
  const int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1;int e=dx+dy;
  for(unsigned n=0;n<2048;++n){pixel(c,x0,y0,v);if(x0==x1&&y0==y1)break;const int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}
}
static void polygon(Canvas &c,const Point *pts,unsigned n,uint16_t fill,uint16_t edge){
  if(n<3||n>512||!c.crossings)return;
  int miny=pts[0].y,maxy=pts[0].y;for(unsigned i=1;i<n;++i){if(pts[i].y<miny)miny=pts[i].y;if(pts[i].y>maxy)maxy=pts[i].y;}
  const int y0=miny<0?0:miny,y1=maxy>=int(c.height)?int(c.height)-1:maxy;
  // Even/odd scanline fill. Per-feature work is bounded by screen height.
  for(int y=y0;y<=y1;++y){unsigned count=0;for(unsigned i=0,j=n-1;i<n;j=i++){
      const auto &a=pts[j],&b=pts[i];if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y)){
        if(count<512)c.crossings[count++]=a.x+int((int64_t(y-a.y)*(b.x-a.x))/(b.y-a.y));
      }}
    for(unsigned i=1;i<count;++i){int v=c.crossings[i];unsigned j=i;while(j&&c.crossings[j-1]>v){c.crossings[j]=c.crossings[j-1];--j;}c.crossings[j]=v;}
    for(unsigned i=0;i+1<count;i+=2){int a=c.crossings[i],b=c.crossings[i+1];if(a>b){int t=a;a=b;b=t;}if(a<0)a=0;if(b>=int(c.width))b=int(c.width)-1;for(int x=a;x<=b;++x)pixel(c,x,y,fill);}
  }
  for(unsigned i=0,j=n-1;i<n;j=i++)line(c,pts[j].x,pts[j].y,pts[i].x,pts[i].y,edge);
}
static uint16_t style(const char *name,bool polygon_feature){
  if(strstr(name,"water")||strstr(name,"ocean"))return color(polygon_feature?0xB9D8E6:0x72AFC9);
  if(strstr(name,"landcover")||strstr(name,"landuse")||strstr(name,"park")||strstr(name,"leisure"))return color(polygon_feature?0xDCE9D3:0xA5C89D);
  if(strstr(name,"building"))return color(polygon_feature?0xE8D9C8:0xC7B7A9);
  if(strstr(name,"transportation")||strstr(name,"road"))return color(polygon_feature?0xF3E5C8:0xD8A96D);
  if(strstr(name,"boundary"))return color(0xCEA4A4);
  return color(polygon_feature?0xE9E6DA:0x9CA6A3);
}
static bool draw_feature(Reader feature,const char *layer,Canvas &c,unsigned extent,unsigned &commands){
  unsigned type=0;Reader geom{feature.p,feature.p,true};
  while(feature.p<feature.end&&feature.ok){unsigned n,w;if(!field(feature,n,w))break;if(n==4&&w==2){geom=bytes(feature);break;}if(n==3&&w==0)type=unsigned(varint(feature));else if(!skip(feature,w))break;}
  if(!feature.ok||!geom.ok||!type||type>3)return feature.ok;
  Point *ring=c.points;if(!ring)return false;unsigned used=0;int64_t px=0,py=0;uint16_t ink=style(layer,type==3);bool have=false;
  while(geom.p<geom.end&&geom.ok&&commands<MAX_COMMANDS){const uint32_t cmd=uint32_t(varint(geom));const unsigned id=cmd&7,count=cmd>>3;if(!count||count>4096){geom.ok=false;break;}commands+=count;
    if(id==1||id==2){for(unsigned i=0;i<count;++i){const int32_t dx=zigzag(uint32_t(varint(geom))),dy=zigzag(uint32_t(varint(geom)));if(!geom.ok)break;px+=dx;py+=dy;
        const int x=c.tile_x*256+int(px*256/int64_t(extent))-c.origin_x,y=c.tile_y*256+int(py*256/int64_t(extent))-c.origin_y;
        if(id==1){if(have&&type==3&&used>=3)polygon(c,ring,used,ink,ink);used=0;have=true;}
        if(id==1&&type==2)used=0;
        if(type==2){if(used)line(c,ring[used-1].x,ring[used-1].y,x,y,ink);if(used<512)ring[used++]={x,y};}
        else if(type==3){if(used<512)ring[used++]={x,y};}
        else if(type==1){for(int oy=-2;oy<=2;++oy)for(int ox=-2;ox<=2;++ox)pixel(c,x+ox,y+oy,ink);}
      }}else if(id==7){if(type==3&&used>=3)polygon(c,ring,used,ink,ink);used=0;have=false;}else {geom.ok=false;break;}
  }
  if(type==3&&used>=3)polygon(c,ring,used,ink,ink);
  return geom.ok&&geom.p==geom.end;
}
static bool draw_layer(Reader layer,Canvas &c,unsigned &features,unsigned &commands){
  char name[32]="";unsigned extent=4096;Reader scan=layer;
  while(scan.p<scan.end&&scan.ok){unsigned n,w;if(!field(scan,n,w))break;if(n==1&&w==2){auto s=bytes(scan);const size_t len=size_t(s.end-s.p);const size_t take=len<sizeof(name)-1?len:sizeof(name)-1;memcpy(name,s.p,take);name[take]=0;}else if(n==5&&w==0)extent=unsigned(varint(scan));else if(!skip(scan,w))break;}
  if(!scan.ok||extent<256||extent>65536)return false;
  while(layer.p<layer.end&&layer.ok){unsigned n,w;if(!field(layer,n,w))break;if(n==2&&w==2){if(++features>MAX_FEATURES)return false;if(!draw_feature(bytes(layer),name,c,extent,commands))return false;}else if(!skip(layer,w))break;}
  return layer.ok;
}
static bool render(const uint8_t *data,size_t size,Canvas &c){
  if(!data||!size||size>MAX_TILE_BYTES||!c.pixels||!c.width||!c.height)return false;
  Reader tile{data,data+size,true};unsigned features=0,commands=0;
  while(tile.p<tile.end&&tile.ok){unsigned n,w;if(!field(tile,n,w))break;if(n==3&&w==2){if(!draw_layer(bytes(tile),c,features,commands))return false;}else if(!skip(tile,w))break;}
  return tile.ok&&tile.p==tile.end;
}
} // namespace mcxofm
