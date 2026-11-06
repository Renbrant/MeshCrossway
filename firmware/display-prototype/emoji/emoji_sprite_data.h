#pragma once
#include <stdint.h>
#include <stddef.h>
namespace mcxemoji {
static constexpr size_t kEmojiSpriteCount=750;
static constexpr size_t kEmojiSpriteBytes=162;
static constexpr size_t kEmojiSpriteStride=164;
alignas(4) static const char kEmojiSpriteBlock0[]=
#include "emoji_sprites_00.inc"
;
alignas(4) static const char kEmojiSpriteBlock1[]=
#include "emoji_sprites_01.inc"
;
alignas(4) static const char kEmojiSpriteBlock2[]=
#include "emoji_sprites_02.inc"
;
alignas(4) static const char kEmojiSpriteBlock3[]=
#include "emoji_sprites_03.inc"
;
alignas(4) static const char kEmojiSpriteBlock4[]=
#include "emoji_sprites_04.inc"
;
alignas(4) static const char kEmojiSpriteBlock5[]=
#include "emoji_sprites_05.inc"
;
alignas(4) static const char kEmojiSpriteBlock6[]=
#include "emoji_sprites_06.inc"
;
alignas(4) static const char kEmojiSpriteBlock7[]=
#include "emoji_sprites_07.inc"
;
alignas(4) static const char kEmojiSpriteBlock8[]=
#include "emoji_sprites_08.inc"
;
alignas(4) static const char kEmojiSpriteBlock9[]=
#include "emoji_sprites_09.inc"
;
alignas(4) static const char kEmojiSpriteBlock10[]=
#include "emoji_sprites_10.inc"
;
alignas(4) static const char kEmojiSpriteBlock11[]=
#include "emoji_sprites_11.inc"
;
static const char *const kEmojiSpriteChunks[]={kEmojiSpriteBlock0,kEmojiSpriteBlock1,kEmojiSpriteBlock2,kEmojiSpriteBlock3,kEmojiSpriteBlock4,kEmojiSpriteBlock5,kEmojiSpriteBlock6,kEmojiSpriteBlock7,kEmojiSpriteBlock8,kEmojiSpriteBlock9,kEmojiSpriteBlock10,kEmojiSpriteBlock11};
inline const uint8_t *sprite_data(size_t i){return i<kEmojiSpriteCount?reinterpret_cast<const uint8_t *>(kEmojiSpriteChunks[i/64])+i%64*kEmojiSpriteStride:nullptr;}
}
