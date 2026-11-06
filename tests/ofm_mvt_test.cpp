#include <cassert>
#include <cstdint>
#include <vector>
#include "../firmware/display-prototype/meshcrossway_ofm_mvt.h"

static void varint(std::vector<uint8_t> &b,uint32_t v){while(v>=128){b.push_back(uint8_t(v)|128);v>>=7;}b.push_back(uint8_t(v));}
static void field(std::vector<uint8_t> &b,unsigned n,const std::vector<uint8_t> &v){varint(b,(n<<3)|2);varint(b,uint32_t(v.size()));b.insert(b.end(),v.begin(),v.end());}
static std::vector<uint8_t> number(unsigned n,uint32_t v){std::vector<uint8_t>b;varint(b,(n<<3)|0);varint(b,v);return b;}
static void test_valid_point_tile(){
  // A tiny OpenMapTiles-like layer with one point at the tile center.
  std::vector<uint8_t> feature=number(3,1),geometry;
  varint(geometry,9);varint(geometry,4096);varint(geometry,4096);
  field(feature,4,geometry);
  std::vector<uint8_t> layer;field(layer,1,std::vector<uint8_t>{'p','l','a','c','e'});field(layer,2,feature);
  auto extent=number(5,4096);layer.insert(layer.end(),extent.begin(),extent.end());
  std::vector<uint8_t> tile;field(tile,3,layer);
  uint16_t pixels[256*256]={};mcxofm::Scratch scratch;mcxofm::Canvas canvas;canvas.pixels=pixels;canvas.points=scratch.points;canvas.crossings=scratch.crossings;canvas.width=canvas.height=256;
  assert(mcxofm::render(tile.data(),tile.size(),canvas));
  bool found=false;for(unsigned y=125;y<132;++y)for(unsigned x=125;x<132;++x)found|=pixels[y*256+x]!=0;
  assert(found);
}
static void test_bounds_and_malformed(){
  uint16_t pixel=0;mcxofm::Scratch scratch;mcxofm::Canvas c;c.pixels=&pixel;c.points=scratch.points;c.crossings=scratch.crossings;c.width=c.height=1;
  const uint8_t truncated[]={0x1a,0x80};assert(!mcxofm::render(truncated,sizeof(truncated),c));
  std::vector<uint8_t> too_large(mcxofm::MAX_TILE_BYTES+1,0);assert(!mcxofm::render(too_large.data(),too_large.size(),c));
  const uint8_t empty[]={};assert(!mcxofm::render(empty,0,c));
}
int main(){test_valid_point_tile();test_bounds_and_malformed();}
