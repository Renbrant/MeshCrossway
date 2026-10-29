#pragma once
// LVGL 8 custom allocator. UI objects and text must not consume the internal
// heap reserved for BLE, task stacks and DMA. Realloc preserves this constraint.
#include <stddef.h>
#include "esp_heap_caps.h"
static inline void *mcx_ui_allocate(size_t bytes){
  return bytes?heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):NULL;
}
static inline void mcx_ui_release(void *block){heap_caps_free(block);}
static inline void *mcx_ui_resize(void *block,size_t bytes){
  if(!bytes){mcx_ui_release(block);return NULL;}
  return heap_caps_realloc(block,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
// lv_conf_internal.h supplies defaults before including this header in lv_mem.c.
#undef LV_MEM_CUSTOM_ALLOC
#undef LV_MEM_CUSTOM_FREE
#undef LV_MEM_CUSTOM_REALLOC
#define LV_MEM_CUSTOM_ALLOC mcx_ui_allocate
#define LV_MEM_CUSTOM_FREE mcx_ui_release
#define LV_MEM_CUSTOM_REALLOC mcx_ui_resize
