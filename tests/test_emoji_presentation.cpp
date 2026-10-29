// Standalone host tests. No firmware or LVGL dependency.
// Standard string literals contain UTF-8 bytes under GCC ESP-IDF/C++20.
#include "../firmware/display-prototype/emoji/emoji_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != nullptr;
}
static void plain_and_accents() {
  const char src[] = "Acao / Sao / Caf\u00e9 123 # *!";
  char dst[128];
  const mcxemoji::Report r = mcxemoji::transform(src, strlen(src), dst, sizeof(dst));
  assert(strcmp(src, dst) == 0);
  assert(r.emoji_substitutions == 0 && r.unsupported_clusters == 0 && !r.truncated);
}
static void known_single_and_composed() {
  const char src[] = "Hello \U0001f604 / \u2764\ufe0f / \U0001f1e7\U0001f1f7 / \U0001f469\u200d\U0001f4bb / \U0001f44d\U0001f3fd!";
  char dst[256];
  char raw_copy[sizeof(src)]; memcpy(raw_copy, src, sizeof(src));
  const mcxemoji::Report r = mcxemoji::transform(src, strlen(src), dst, sizeof(dst));
  assert(r.emoji_substitutions == 5 && r.unsupported_clusters == 0 && !r.truncated);
  assert(strcmp(src, raw_copy) == 0); // never touches original text
  assert(strlen(dst) < strlen(src));
  assert(contains(dst,"Hello ") && contains(dst,"!"));
  assert(!contains(dst,"[emoji]"));
}
static void checkered_flag_in_sender_name() {
  const char source[]="\U0001F3C1PU2OOM RenBrant 02";
  const char variant[]="\U0001F3C1\uFE0FPU2OOM RenBrant 02";
  char dest[128]={};
  const mcxemoji::Report r=mcxemoji::transform(source,strlen(source),dest,sizeof(dest));
  assert(r.emoji_substitutions==1 && r.unsupported_clusters==0 && !r.truncated);
  assert(!contains(dest,"[emoji]") && contains(dest,"PU2OOM RenBrant 02"));
  assert(strcmp(source,"\U0001F3C1PU2OOM RenBrant 02")==0);
  const mcxemoji::Report v=mcxemoji::transform(variant,strlen(variant),dest,sizeof(dest));
  assert(v.emoji_substitutions==1 && v.unsupported_clusters==0 && !v.truncated);
  assert(!contains(dest,"[emoji]") && contains(dest,"PU2OOM RenBrant 02"));
}
static void unknown_and_zwj_stay_atomic() {
  const char src[] = "A \U0001f468\u200d\U0001f469\u200d\U0001f467 B \U0001f1ef\U0001f1f5";
  char dst[128];
  const mcxemoji::Report r = mcxemoji::transform(src, strlen(src), dst, sizeof(dst));
  assert(r.emoji_substitutions == 0 && r.unsupported_clusters == 2);
  assert(strcmp(dst, "A $ B $") == 0);
}
static void fallback_and_text_only_rendering() {
  const char source[]="A [emoji] B \U0001F90D C \U0001F9D1 D";
  char display[96] = {};
  const mcxemoji::Report r=mcxemoji::transform(source,strlen(source),
                                                 display,sizeof(display));
  assert(r.emoji_substitutions==1 && r.unsupported_clusters==2);
  assert(!contains(display,"[emoji]"));
  const mcxemoji::Report no_images=mcxemoji::transform(
    source,strlen(source),display,sizeof(display),false);
  assert(no_images.emoji_substitutions==0 && no_images.unsupported_clusters==3);
  assert(strcmp(display,"A $ B $ C $ D")==0);
  assert(strcmp(source,"A [emoji] B \U0001F90D C \U0001F9D1 D")==0);
}
static void keycaps_vs_normal_digits() {
  const char src[] = "No. 1 & #, but \u0031\ufe0f\u20e3 \u0023\ufe0f\u20e3";
  char dst[128];
  const mcxemoji::Report r = mcxemoji::transform(src, strlen(src), dst, sizeof(dst));
  assert(r.emoji_substitutions == 2);
  assert(contains(dst,"No. 1 & #, but "));
}
static void invalid_utf8_never_overreads() {
  const char src[] = {'A', (char)0xf0, (char)0x80, (char)0x80, (char)0x80, 'Z'};
  char dst[32];
  const mcxemoji::Report r = mcxemoji::transform(src, sizeof(src), dst, sizeof(dst));
  assert(strcmp(dst, "A????Z") == 0 && r.invalid_bytes == 4 && !r.truncated);
  const char incomplete[] = {(char)0xe2, (char)0x82};
  const mcxemoji::Report t = mcxemoji::transform(incomplete, sizeof(incomplete), dst, sizeof(dst));
  assert(strcmp(dst,"??") == 0 && t.invalid_bytes == 2);
}
static void truncation_never_splits_utf8() {
  const char src[] = "\U0001f604\U0001f1fa\U0001f1f8X";
  char dst[5]; // one PUA (3 bytes) and trailing null; cannot fit second glyph
  const mcxemoji::Report r = mcxemoji::transform(src, strlen(src), dst, sizeof(dst));
  assert(r.emoji_substitutions == 1 && r.truncated && strlen(dst) == 3);
  uint32_t cp = 0; size_t w = 0;
  assert(mcxemoji::decode(dst,strlen(dst),0,cp,w) && w == 3 && cp >= 0xe000);
  char tiny[1];
  const mcxemoji::Report q = mcxemoji::transform(src,strlen(src),tiny,sizeof(tiny));
  assert(q.truncated && tiny[0] == 0);
}
static void no_memory_allocation_or_side_effects() {
  char dst[128];
  const mcxemoji::Report r = mcxemoji::transform(nullptr, 0, dst, sizeof(dst));
  assert(!r.truncated && dst[0] == 0);
  const mcxemoji::Report t = mcxemoji::transform(nullptr, 2, dst, sizeof(dst));
  assert(t.invalid_bytes == 2 && dst[0] == 0);
  const mcxemoji::Report u = mcxemoji::transform("test", 4, nullptr, 0);
  assert(u.truncated);
}
int main() {
  plain_and_accents(); known_single_and_composed(); checkered_flag_in_sender_name(); unknown_and_zwj_stay_atomic();
  fallback_and_text_only_rendering();
  keycaps_vs_normal_digits(); invalid_utf8_never_overreads();
  truncation_never_splits_utf8(); no_memory_allocation_or_side_effects();
  printf("emoji presentation: 9 host test groups passed; %zu registry entries\n", mcxemoji::kGlyphCount);
}
