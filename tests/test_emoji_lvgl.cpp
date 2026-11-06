// Real LVGL 8.4 rendering, bounded allocations and failure-path regression.
#include "lvgl.h"
#include "emoji/emoji_display.h"
#include <string>
#include <cstdio>
static lv_color_t drawbuf[480*20];
static unsigned flushes=0;
static void flush(lv_disp_drv_t *d,const lv_area_t *,lv_color_t *){++flushes;lv_disp_flush_ready(d);}
static void advance(){for(unsigned i=0;i<5;++i){lv_tick_inc(30);lv_timer_handler();}}
int main(){
  check_allocator();lv_init();lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,drawbuf,nullptr,480*20);
  lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=480;d.ver_res=320;d.draw_buf=&buf;d.flush_cb=flush;lv_disp_drv_register(&d);
  auto *label=lv_label_create(lv_scr_act());lv_obj_set_width(label,400);
  const char raw[]="\U0001F3C1 RenBrant \U0001F1E7\U0001F1F7 \U0001F44D\U0001F3FF \U0001F3F3\uFE0F\u200D\U0001F308 end";
  const std::string copy=raw;
  mcxemoji::view::set_message_text(label,raw);advance();
  assert(flushes&&copy==raw&&strstr(lv_label_get_text(label),"end"));
  assert(mcxemoji::view::scratch&&mcxemoji::view::emoji_font);
  const auto baseline=ui_live_bytes;auto *scratch=mcxemoji::view::scratch;
  lv_img_dsc_t image={};
  assert(!mcxemoji::view::image_cb(nullptr,&image,sizeof(image),0xe000-1,0));
  assert(!mcxemoji::view::image_cb(nullptr,&image,sizeof(image),0xe000+750,0));
  assert(!mcxemoji::view::image_cb(nullptr,&image,1,0xe000,0));
  assert(!mcxemoji::view::image_cb(nullptr,nullptr,sizeof(image),0xe000,0));
  for(size_t i=0;i<mcxemoji::kGlyphCount;++i){
    assert(mcxemoji::view::image_cb(nullptr,&image,sizeof(image),0xe000+i,0));
    assert(image.data==mcxemoji::sprite_data(i)&&image.data_size==162&&image.header.w==14&&image.header.h==14);
    mcxemoji::view::set_message_text(label,mcxemoji::kGlyphs[i].utf8);advance();
  }
  for(unsigned i=0;i<200;++i)mcxemoji::view::set_message_text(label,raw);
  assert(mcxemoji::view::scratch==scratch&&ui_live_bytes<=baseline+1024);
  // Scratch allocation failure is isolated from LVGL's own object allocations.
  heap_caps_free(scratch);mcxemoji::view::scratch=nullptr;mcxemoji::view::scratch_attempted=false;ui_fail_size=1200;
  mcxemoji::view::set_message_text(label,raw);assert(strstr(lv_label_get_text(label),"[emoji]")&&strstr(lv_label_get_text(label),"end"));
  assert(lv_obj_get_style_text_font(label,LV_PART_MAIN)==LV_FONT_DEFAULT);
  assert(!mcxemoji::view::scratch&&mcxemoji::view::scratch_attempted);ui_fail_size=0;
  // An expanded fallback must never replace the complete message with an error.
  const std::string huge=std::string(1900,'x')+raw;
  mcxemoji::view::set_message_text(label,huge.c_str());assert(huge==lv_label_get_text(label));
  // Font-creation failure after a successful scratch allocation remains readable.
  mcxemoji::view::scratch_attempted=false;
  lv_imgfont_destroy(mcxemoji::view::emoji_font);mcxemoji::view::emoji_font=nullptr;mcxemoji::view::attempted=false;
  ui_fail=true;assert(!mcxemoji::view::font());ui_fail=false;
  mcxemoji::view::set_message_text(label,raw);assert(strstr(lv_label_get_text(label),"[emoji]")&&strstr(lv_label_get_text(label),"end"));
  mcxemoji::view::attempted=false;mcxemoji::view::set_message_text(label,raw);advance();
  assert(copy==raw&&strstr(lv_label_get_text(label),"end"));lv_obj_del(label);
  puts("PASS: real LVGL renders 750 sprites, bounded shared scratch, names/payload unchanged and allocation-failure fallback");
}
