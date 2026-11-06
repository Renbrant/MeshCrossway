#include "../firmware/display-prototype/emoji/emoji_sprite_data.h"
#include "../firmware/display-prototype/emoji/emoji_small_sprite.h"
#include "../firmware/display-prototype/emoji/emoji_assets_data.h"
#include "../firmware/display-prototype/emoji/emoji_flag_asset.h"
#include <cassert>
#include <vector>
#include <cstdio>
static std::vector<uint8_t> decode64(const char *s){
  std::vector<uint8_t> out;unsigned acc=0,bits=0;
  for(;*s;++s){const char *p=strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",*s);assert(p);
    acc=(acc<<6)|unsigned(p-"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");bits+=6;
    if(bits>=8){bits-=8;out.push_back(uint8_t(acc>>bits));}}
  return out;
}
int main(){
  auto legacy=decode64(mcxemoji::kEmojiAssetsBase64);auto flag=decode64(mcxemoji::kCheckeredFlagBase64);
  legacy.insert(legacy.end(),flag.begin(),flag.end());assert(legacy.size()==93*264);
  for(size_t i=0;i<mcxemoji::kEmojiSpriteCount;++i){
    const auto *sprite=mcxemoji::sprite_data(i);assert(sprite&&uintptr_t(sprite)%4==0);
    assert(sprite[162]==0&&sprite[163]==0);
    bool pixels=false,alpha=false;
    for(size_t j=3;j<64;j+=4)alpha|=sprite[j]!=0;
    for(size_t j=64;j<162;++j)pixels|=sprite[j]!=0;
    assert(alpha&&pixels);
    if(i<93){uint8_t expected[162];assert(mcxemoji::downsample_emoji(legacy.data()+i*264,264,expected,162));assert(!memcmp(sprite,expected,162));}
  }
  assert(!mcxemoji::sprite_data(mcxemoji::kEmojiSpriteCount));
  puts("PASS: all 750 aligned flash sprites, exact legacy pixels, no RAM atlas");
}
