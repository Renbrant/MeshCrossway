#pragma once
// Read-only LVGL 8.4 sprites: palette/pixels stay in flash, no RAM atlas.
// Include from the LVGL owner only. No BLE, persistence or payload conversion.
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "emoji_presentation.h"
#include "emoji_sprite_data.h"
#include <string.h>
namespace mcxemoji { namespace view {
static lv_font_t *emoji_font=nullptr;
static bool attempted=false,scratch_attempted=false;
static char *scratch=nullptr;
static constexpr size_t kPresentationBytes=1200;
static_assert(kGlyphCount==kEmojiSpriteCount,"Every emoji needs a sprite");
inline bool image_cb(const lv_font_t *,void *img_src,uint16_t len,uint32_t unicode,uint32_t){
  if(!img_src||len<sizeof(lv_img_dsc_t)||unicode<0xe000u||unicode>=0xe000u+kGlyphCount)return false;
  lv_img_dsc_t image={};image.header.w=14;image.header.h=14;image.header.cf=LV_IMG_CF_INDEXED_4BIT;
  image.data_size=kEmojiSpriteBytes;image.data=sprite_data(unicode-0xe000u);
  memcpy(img_src,&image,sizeof(image));return true;
}
inline lv_font_t *font(){
  if(attempted)return emoji_font;
  attempted=true;
#if LV_USE_IMGFONT
  if(LV_COLOR_DEPTH!=16)return nullptr;
  emoji_font=lv_imgfont_create(16,image_cb);
  if(emoji_font)emoji_font->fallback=LV_FONT_DEFAULT;
#endif
  return emoji_font;
}
inline void set_message_text(lv_obj_t *label,const char *original_utf8){
  if(!label)return;
  if(!original_utf8)original_utf8="";
  if(!scratch_attempted){scratch_attempted=true;scratch=static_cast<char *>(heap_caps_malloc(kPresentationBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));}
  if(!scratch){
    // Smaller than the former 1200-byte per-label stack frame. If expansion
    // cannot fit either, retain the full original text with the default font.
    char fallback[512];const auto r=transform(original_utf8,strlen(original_utf8),fallback,sizeof(fallback),false);
    lv_obj_set_style_text_font(label,LV_FONT_DEFAULT,LV_PART_MAIN);
    lv_label_set_text(label,r.truncated?original_utf8:fallback);return;
  }
  lv_font_t *f=font();
  // Style events run before the shared scratch is filled. LVGL copies the
  // completed string before sending text/size events; no stored source changes.
  lv_obj_set_style_text_font(label,f?f:LV_FONT_DEFAULT,LV_PART_MAIN);
  const auto r=transform(original_utf8,strlen(original_utf8),scratch,kPresentationBytes,f!=nullptr);
  if(r.truncated){lv_obj_set_style_text_font(label,LV_FONT_DEFAULT,LV_PART_MAIN);lv_label_set_text(label,original_utf8);}
  else lv_label_set_text(label,scratch);
}
} }
