#pragma once
// MeshCrossway display-only UTF-8 emoji parser (read-only presentation).
// No external dependencies, allocation, BLE access, storage or LVGL calls.
// C++11 and later. Registry glyph IDs are append-only and never sent over radio.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace mcxemoji {

struct EmojiGlyph {
  const char *utf8;
  uint32_t private_use; // U+E000..; only a temporary LVGL presentation marker
  const char *description;
};

// Append-only glyphs with a separately sorted, flash-resident lookup index.
// A color image MUST be supplied for each glyph before enabling its display.
static const EmojiGlyph kGlyphs[] = {
#include "emoji_registry.inc"
};
static const size_t kGlyphCount = sizeof(kGlyphs) / sizeof(kGlyphs[0]);
static const uint16_t kLookupOrder[] = {
#include "emoji_lookup_order.inc"
};
static_assert(sizeof(kLookupOrder)/sizeof(kLookupOrder[0])==kGlyphCount,"Emoji lookup index mismatch");

struct Report {
  size_t bytes_written;
  size_t emoji_substitutions;
  size_t unsupported_clusters;
  size_t invalid_bytes;
  bool truncated;
};

inline bool continuation(uint8_t c) { return (c & 0xc0) == 0x80; }

// A bounded, strict decoder: rejects overlongs, UTF-16 surrogates and > U+10FFFF.
inline bool decode(const char *s, size_t n, size_t pos, uint32_t &cp, size_t &width) {
  if (!s || pos >= n) return false;
  const uint8_t a = (uint8_t)s[pos];
  if (a < 0x80) { cp = a; width = 1; return true; }
  if (a < 0xc2 || a > 0xf4) return false;
  if (a <= 0xdf) {
    if (n - pos < 2) return false;
    const uint8_t b = (uint8_t)s[pos + 1];
    if (!continuation(b)) return false;
    cp = ((a & 0x1f) << 6) | (b & 0x3f); width = 2; return true;
  }
  if (a <= 0xef) {
    if (n - pos < 3) return false;
    const uint8_t b = (uint8_t)s[pos + 1], c = (uint8_t)s[pos + 2];
    if (!continuation(b) || !continuation(c) || (a == 0xe0 && b < 0xa0) ||
        (a == 0xed && b >= 0xa0)) return false;
    cp = ((a & 0x0f) << 12) | ((b & 0x3f) << 6) | (c & 0x3f);
    width = 3; return true;
  }
  if (n - pos < 4) return false;
  const uint8_t b = (uint8_t)s[pos + 1], c = (uint8_t)s[pos + 2], d = (uint8_t)s[pos + 3];
  if (!continuation(b) || !continuation(c) || !continuation(d) ||
      (a == 0xf0 && b < 0x90) || (a == 0xf4 && b > 0x8f)) return false;
  cp = ((a & 7) << 18) | ((b & 0x3f) << 12) | ((c & 0x3f) << 6) | (d & 0x3f);
  width = 4; return true;
}

inline bool append(char *dst, size_t cap, size_t &size, const char *bytes, size_t n) {
  if (!dst || !cap || n > cap - 1 || size > cap - 1 - n) return false;
  memcpy(dst + size, bytes, n);
  size += n;
  dst[size] = 0;
  return true;
}

inline size_t encode(uint32_t cp, char out[4]) {
  if (cp <= 0x7f) { out[0] = (char)cp; return 1; }
  if (cp <= 0x7ff) {
    out[0] = (char)(0xc0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); return 2;
  }
  if (cp <= 0xffff) {
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 63)); out[2] = (char)(0x80 | (cp & 63)); return 3;
  }
  out[0] = (char)(0xf0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 63));
  out[2] = (char)(0x80 | ((cp >> 6) & 63)); out[3] = (char)(0x80 | (cp & 63)); return 4;
}

inline bool regional(uint32_t cp) { return cp >= 0x1f1e6 && cp <= 0x1f1ff; }
inline bool tone(uint32_t cp) { return cp >= 0x1f3fb && cp <= 0x1f3ff; }
inline bool variation(uint32_t cp) { return cp == 0xfe0f || cp == 0xfe0e; }
inline bool base_emoji(uint32_t cp) {
  // Deliberately conservative outside modern pictograph blocks.
  return (cp >= 0x1f000 && cp <= 0x1faff) ||
         (cp >= 0x2600 && cp <= 0x27bf) || cp == 0x2b50 || cp == 0x2b55 ||
         cp == 0x2139 || cp == 0x231a || cp == 0x231b || cp == 0x23f0 ||
         cp==0xa9 || cp==0xae || cp==0x203c || cp==0x2049 || cp==0x2122 ||
         (cp>=0x2194&&cp<=0x2199)||(cp>=0x21a9&&cp<=0x21aa)||
         (cp>=0x23e9&&cp<=0x23f3)||(cp>=0x23f8&&cp<=0x23fa)||
         cp==0x25aa||cp==0x25ab||cp==0x25b6||cp==0x25c0||
         (cp>=0x25fb&&cp<=0x25fe)||cp==0x2934||cp==0x2935||
         (cp>=0x2b05&&cp<=0x2b07)||cp==0x3030||cp==0x303d||cp==0x3297||cp==0x3299;
}

// Returns full cluster length in source bytes. ASCII keycap digits are only
// emoji when followed by U+20E3, with an optional FE0F.
inline size_t cluster_length(const char *src, size_t len, size_t start) {
  uint32_t cp; size_t w;
  if (!decode(src, len, start, cp, w)) return 0;
  const size_t first_width = w;
  const bool keycap_candidate = cp == '#' || cp == '*' || (cp >= '0' && cp <= '9');
  if (!base_emoji(cp) && !keycap_candidate) return 0;
  size_t pos = start + w;
  uint32_t next; size_t next_w;
  if (keycap_candidate) {
    if (decode(src, len, pos, next, next_w) && next == 0xfe0f) pos += next_w;
    if (decode(src, len, pos, next, next_w) && next == 0x20e3) return pos + next_w - start;
    return 0;
  }
  if (regional(cp)) {
    if (decode(src, len, pos, next, next_w) && regional(next)) pos += next_w;
    return pos - start;
  }
  while (decode(src, len, pos, next, next_w) && (variation(next) || tone(next))) pos += next_w;
  // Consume ZWJ-qualified sequences atomically; no splitting family/gender emoji.
  for (;;) {
    size_t zwj_w; uint32_t zwj;
    if (!decode(src, len, pos, zwj, zwj_w) || zwj != 0x200d) break;
    uint32_t part; size_t part_w;
    if (!decode(src, len, pos + zwj_w, part, part_w) || !base_emoji(part)) {pos+=zwj_w;break;}
    pos += zwj_w + part_w;
    while (decode(src, len, pos, next, next_w) && (variation(next) || tone(next))) pos += next_w;
  }
  // Subdivision flags are one black-flag + tag sequence, not separate glyphs.
  if(cp==0x1f3f4)while(decode(src,len,pos,next,next_w)&&next>=0xe0020&&next<=0xe007f)pos+=next_w;
  // A valid standalone emoji occupies at least the first Unicode code point.
  return pos - start >= first_width ? pos - start : first_width;
}

inline bool selector_at(const char *s,size_t n,size_t i,uint8_t tail){
  return i+2<n&&(uint8_t)s[i]==0xef&&(uint8_t)s[i+1]==0xb8&&(uint8_t)s[i+2]==tail;
}
inline int compare_key(const char *a,size_t n,const char *b){
  const size_t m=strlen(b);size_t i=0,j=0;
  for(;;){
    while(selector_at(a,n,i,0x8f))i+=3;
    while(selector_at(b,m,j,0x8f))j+=3;
    if(i==n||j==m)return i==n?(j==m?0:-1):1;
    const uint8_t x=uint8_t(a[i++]),y=uint8_t(b[j++]);if(x!=y)return x<y?-1:1;
  }
}
inline const EmojiGlyph *lookup(const char *src,size_t length){
  if(!src||!length)return nullptr;
  // VS15 requests text; never silently map it to a colored sprite.
  for(size_t i=0;i<length;++i)if(selector_at(src,length,i,0x8e))return nullptr;
  size_t lo=0,hi=kGlyphCount;
  while(lo<hi){const size_t mid=lo+(hi-lo)/2;const auto &glyph=kGlyphs[kLookupOrder[mid]];
    const int cmp=compare_key(src,length,glyph.utf8);if(!cmp)return &glyph;
    if(cmp<0)hi=mid;else lo=mid+1;
  }
  return nullptr;
}

inline Report transform(const char *src, size_t src_len, char *dst, size_t dst_cap,
                        bool image_font_available = true) {
  Report r = {0, 0, 0, 0, false};
  if (!dst || !dst_cap) { r.truncated = true; return r; }
  dst[0] = 0;
  if (!src && src_len) { r.invalid_bytes = src_len; return r; }
  for (size_t pos = 0; pos < src_len;) {
    // Some old clients send a literal [emoji] placeholder; on-screen only.
    if (src_len - pos >= 7 && !memcmp(src + pos, "[emoji]", 7)) {
      if (!append(dst, dst_cap, r.bytes_written, "[emoji]", 7)) { r.truncated = true; break; }
      ++r.unsupported_clusters; pos += 7;
      continue;
    }
    size_t width; uint32_t cp;
    if (!decode(src, src_len, pos, cp, width)) {
      const char replacement[] = "?";
      if (!append(dst, dst_cap, r.bytes_written, replacement, 1)) { r.truncated = true; break; }
      ++r.invalid_bytes; ++pos;
      continue;
    }
    const size_t cluster = cluster_length(src, src_len, pos);
    if (cluster) {
      const EmojiGlyph *glyph = lookup(src + pos, cluster);
      if (glyph && image_font_available) {
        char pua[4]; const size_t pua_len = encode(glyph->private_use, pua);
        if (!append(dst, dst_cap, r.bytes_written, pua, pua_len)) { r.truncated = true; break; }
        ++r.emoji_substitutions;
      } else {
        const char fallback[] = "[emoji]";
        if (!append(dst, dst_cap, r.bytes_written, fallback, sizeof(fallback) - 1)) {
          r.truncated = true; break;
        }
        ++r.unsupported_clusters;
      }
      pos += cluster;
    } else {
      if (!append(dst, dst_cap, r.bytes_written, src + pos, width)) { r.truncated = true; break; }
      pos += width;
    }
  }
  return r;
}

} // namespace mcxemoji
