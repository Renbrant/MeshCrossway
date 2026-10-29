#include "../firmware/display-prototype/emoji/emoji_small_sprite.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
  uint8_t original[mcxemoji::kEmojiOriginalBytes] = {};
  uint8_t reduced[mcxemoji::kEmojiDrawBytes] = {};
  assert(mcxemoji::kEmojiDrawBytes == 162);
  assert(mcxemoji::kEmojiOriginalBytes == 264);
  for (size_t i=0; i<64; ++i) original[i]=(uint8_t)i;
  for (size_t y=0; y<20; ++y) for (size_t x=0; x<20; x+=2) {
    original[64+y*10+x/2]=(uint8_t)(((x%16)<<4)|((x+1)%16));
  }
  assert(mcxemoji::downsample_emoji(original,sizeof(original),
                                   reduced,sizeof(reduced)));
  assert(!memcmp(original,reduced,64));
  for (size_t y=0; y<14; ++y) for (size_t x=0; x<14; ++x) {
    size_t sx=((2*x+1)*20)/28;
    size_t sy=((2*y+1)*20)/28;
    uint8_t src=original[64+sy*10+sx/2];
    uint8_t expected=(sx&1)?(src&15):(src>>4);
    uint8_t got=reduced[64+y*7+x/2];
    got=(x&1)?(got&15):(got>>4);
    assert(got==expected);
  }
  assert(!mcxemoji::downsample_emoji(nullptr,264,reduced,162));
  assert(!mcxemoji::downsample_emoji(original,263,reduced,162));
  assert(!mcxemoji::downsample_emoji(original,264,reduced,161));
  assert(!mcxemoji::downsample_emoji(original,264,original,264));
  std::puts("PASS: 20-to-14 indexed sprite geometry, palette, bounds, colors");
}
