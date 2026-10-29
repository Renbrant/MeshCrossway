#pragma once
// Downsample only the *presentation bitmap*, never the Unicode source.
// 20x20 LVGL indexed-4 sprites include 64-byte BGRA palette + 200 pixels.
// Scale to 14x14 with center-aligned nearest neighbor and 16px font line-height.
// This format is pure C++11 so host CI can verify every output sprite.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace mcxemoji {
static constexpr size_t kEmojiOriginalSide = 20;
static constexpr size_t kEmojiDrawSide = 14;
static constexpr size_t kEmojiOriginalBytes = 64 + (20 * 20) / 2;
static constexpr size_t kEmojiDrawBytes = 64 + (14 * 14) / 2;
static inline bool downsample_emoji(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len) {
  if (!src || !dst || src_len < kEmojiOriginalBytes || dst_len < kEmojiDrawBytes)
    return false;
  // Caller allows overlap only by making one 264-byte local source copy.
  if (!(dst + kEmojiDrawBytes <= src || src + kEmojiOriginalBytes <= dst))
    return false;
  memcpy(dst,src,64);
  memset(dst+64,0,kEmojiDrawBytes-64);
  for (size_t y=0; y<kEmojiDrawSide; ++y) {
    const size_t sy=((2*y+1)*kEmojiOriginalSide)/(2*kEmojiDrawSide);
    for (size_t x=0; x<kEmojiDrawSide; ++x) {
      const size_t sx=((2*x+1)*kEmojiOriginalSide)/(2*kEmojiDrawSide);
      const uint8_t from=src[64+sy*(kEmojiOriginalSide/2)+sx/2];
      const uint8_t index=(sx&1) ? (from&0x0fu) : (from>>4);
      uint8_t &dest=dst[64+y*(kEmojiDrawSide/2)+x/2];
      if(x&1)dest|=index;
      else dest|=(uint8_t)(index<<4);
    }
  }
  return true;
}
} // namespace mcxemoji
