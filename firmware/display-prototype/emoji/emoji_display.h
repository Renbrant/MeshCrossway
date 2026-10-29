#pragma once
// MeshCrossway read-only emoji renderer. LVGL 8.4, 20x20, color indexed images.
// Only include from meshcrossway_ui.h, which already owns the LVGL UI thread.
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "emoji_presentation.h"
#include "emoji_assets_data.h"
#include "emoji_flag_asset.h"
#include "emoji_small_sprite.h"
#include <string.h>

namespace mcxemoji {
namespace view {
static lv_font_t *emoji_font = nullptr;
static uint8_t *atlas = nullptr;
static constexpr size_t kEmojiRenderedCount = kEmojiAssetCount + 1;
static lv_img_dsc_t images[kEmojiRenderedCount];
static bool attempted = false;

inline int b64value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
// Decode into PSRAM once, outside BLE callbacks. No external PNG/SVG decoder.
inline bool unpack(const char *encoded, uint8_t *out, size_t capacity, size_t expected) {
  if (!encoded || !out || capacity < expected) return false;
  size_t p = 0;
  uint32_t bits = 0;
  unsigned filled = 0;
  for (const char *s = encoded; *s; ++s) {
    const int v = b64value(*s);
    if (v < 0) return false;
    bits = (bits << 6) | (uint32_t)v;
    filled += 6;
    if (filled >= 8) {
      filled -= 8;
      if (p >= expected) return false;
      out[p++] = (uint8_t)(bits >> filled);
      bits &= (1u << filled) - 1;
    }
  }
  return p == expected;
}

inline bool image_cb(const lv_font_t *, void *img_src, uint16_t len,
                     uint32_t unicode, uint32_t unicode_next) {
  (void)unicode_next;
  if (!atlas || !img_src || len < sizeof(lv_img_dsc_t) ||
      unicode < 0xE000u || unicode >= 0xE000u + kEmojiRenderedCount)
    return false;
  memcpy(img_src, &images[unicode - 0xE000u], sizeof(lv_img_dsc_t));
  return true;
}

inline lv_font_t *font() {
  if (attempted) return emoji_font;
  attempted = true;
#if LV_USE_IMGFONT
  if (LV_COLOR_DEPTH != 16 || kGlyphCount != kEmojiRenderedCount ||
      kCheckeredFlagAssetSize != kEmojiAssetSize) return nullptr;
  const size_t total = kEmojiAssetDecodedBytes + kCheckeredFlagAssetSize;
  atlas = (uint8_t *)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!atlas) return nullptr;
  if (!unpack(kEmojiAssetsBase64, atlas, kEmojiAssetDecodedBytes, kEmojiAssetDecodedBytes) ||
      !unpack(kCheckeredFlagBase64, atlas + kEmojiAssetDecodedBytes,
              kCheckeredFlagAssetSize, kCheckeredFlagAssetSize)) {
    heap_caps_free(atlas);
    atlas = nullptr;
    return nullptr;
  }
  // Use 14px pictures with a 16px font line box, close to the surrounding
  // 14px UI text. Keep the original 20px assets byte-for-byte unchanged.
  // The compact destination for glyph i ends before input i+1 begins;
  // a local 264-byte copy avoids overwriting the current source sprite.
  for (size_t i = 0; i < kEmojiRenderedCount; ++i) {
    uint8_t source[kEmojiOriginalBytes];
    memcpy(source, atlas+i*kEmojiOriginalBytes, sizeof(source));
    if (!downsample_emoji(source,sizeof(source),
                          atlas+i*kEmojiDrawBytes,kEmojiDrawBytes)) {
      heap_caps_free(atlas);
      atlas=nullptr;
      return nullptr;
    }
  }
  for (size_t i = 0; i < kEmojiRenderedCount; ++i) {
    memset(&images[i], 0, sizeof(images[i]));
    images[i].header.always_zero = 0;
    images[i].header.w = kEmojiDrawSide;
    images[i].header.h = kEmojiDrawSide;
    images[i].header.cf = LV_IMG_CF_INDEXED_4BIT;
    images[i].data_size = kEmojiDrawBytes;
    images[i].data = atlas + i * kEmojiDrawBytes;
  }
  emoji_font = lv_imgfont_create(16, image_cb);
  if (!emoji_font) {
    heap_caps_free(atlas);
    atlas = nullptr;
    return nullptr;
  }
  emoji_font->fallback = LV_FONT_DEFAULT;
#endif
  return emoji_font;
}

// View-only: never update message buffers or pass display PUA back to a radio.
inline void set_message_text(lv_obj_t *label, const char *original_utf8) {
  if (!label) return;
  if (!original_utf8) original_utf8 = "";
  lv_font_t *f = font();
  if (!f) {
    // No image font/PSRAM: show '$' even for recognized emoji rather than
    // leaking invisible private-use codepoints. Source UTF-8 stays unchanged.
    char out[1200] = {};
    const Report r = transform(original_utf8,strlen(original_utf8),
                               out,sizeof(out),false);
    lv_label_set_text(label,r.truncated?"[message too long]":out);
    return;
  }
  char out[1200];
  Report r = transform(original_utf8, strlen(original_utf8), out, sizeof(out));
  if (r.truncated) {
    // Conservative fallback rather than show partial/invalid representation.
    lv_label_set_text(label, "[message too long]");
    return;
  }
  lv_obj_set_style_text_font(label, f, LV_PART_MAIN);
  lv_label_set_text(label, out);
}
} // namespace view
} // namespace mcxemoji