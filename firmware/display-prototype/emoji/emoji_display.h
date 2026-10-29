#pragma once
// MeshCrossway read-only emoji renderer. LVGL 8.4, 20x20, color indexed images.
// Only include from meshcrossway_ui.h, which already owns the LVGL UI thread.
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "emoji_presentation.h"
#include "emoji_assets_data.h"
#include <string.h>

namespace mcxemoji {
namespace view {
static lv_font_t *emoji_font = nullptr;
static uint8_t *atlas = nullptr;
static lv_img_dsc_t images[kEmojiAssetCount];
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
inline bool unpack(uint8_t *out, size_t out_capacity) {
  if (!out || out_capacity < kEmojiAssetDecodedBytes) return false;
  size_t p = 0;
  uint32_t bits = 0;
  unsigned filled = 0;
  for (const char *s = kEmojiAssetsBase64; *s; ++s) {
    const int v = b64value(*s);
    if (v < 0) return false;
    bits = (bits << 6) | (uint32_t)v;
    filled += 6;
    if (filled >= 8) {
      filled -= 8;
      if (p >= out_capacity) return false;
      out[p++] = (uint8_t)(bits >> filled);
      bits &= (1u << filled) - 1;
    }
  }
  return p == kEmojiAssetDecodedBytes;
}

inline bool image_cb(const lv_font_t *, void *img_src, uint16_t len,
                     uint32_t unicode, uint32_t unicode_next) {
  (void)unicode_next;
  if (!atlas || !img_src || len < sizeof(lv_img_dsc_t) ||
      unicode < 0xE000u || unicode >= 0xE000u + kEmojiAssetCount)
    return false;
  memcpy(img_src, &images[unicode - 0xE000u], sizeof(lv_img_dsc_t));
  return true;
}

inline lv_font_t *font() {
  if (attempted) return emoji_font;
  attempted = true;
#if LV_USE_IMGFONT
  if (LV_COLOR_DEPTH != 16 || kGlyphCount != kEmojiAssetCount) return nullptr;
  atlas = (uint8_t *)heap_caps_malloc(kEmojiAssetDecodedBytes,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!atlas) return nullptr;
  if (!unpack(atlas, kEmojiAssetDecodedBytes)) {
    heap_caps_free(atlas);
    atlas = nullptr;
    return nullptr;
  }
  for (size_t i = 0; i < kEmojiAssetCount; ++i) {
    memset(&images[i], 0, sizeof(images[i]));
    images[i].header.always_zero = 0;
    images[i].header.w = 20;
    images[i].header.h = 20;
    images[i].header.cf = LV_IMG_CF_INDEXED_4BIT;
    images[i].data_size = kEmojiAssetSize;
    images[i].data = atlas + i * kEmojiAssetSize;
  }
  emoji_font = lv_imgfont_create(20, image_cb);
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
    // If the image subsystem cannot start, use a legible text-only version.
    // Do not expose invisible PUA characters without their image font.
    char out[1200];
    const Report report = transform(original_utf8, strlen(original_utf8), out, sizeof(out));
    (void)report;
    for (char *c = out; *c;) {
      // Decode PUA U+E000..U+E05B as a text placeholder, only on failure.
      if ((uint8_t)c[0] == 0xEE && (uint8_t)c[1] >= 0x80 &&
          (uint8_t)c[1] <= 0x81 && ((uint8_t)c[2] & 0xC0) == 0x80) {
        // Avoid modifying variable-length string in place: fallback to raw.
        lv_label_set_text(label, "[emoji unavailable]");
        return;
      }
      ++c;
    }
    lv_label_set_text(label, out);
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