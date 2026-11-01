#pragma once
// Included inside meshcrossway. Only the LVGL owner task touches this cache.
namespace chatview {
struct Row {uint32_t id=0;uint64_t hash=0;lv_obj_t *object=nullptr;bool used=false;};
struct Cache {Row rows[96];uint16_t order[96]={};lv_obj_t *parent=nullptr,*empty=nullptr,*title=nullptr,*subtitle=nullptr,*history=nullptr;
  uint64_t title_hash=0;uint32_t anchor=0;int offset=0,y=96;bool follow=true;};
static Cache *cache=nullptr;
static void invalidate(){if(cache)new(cache) Cache();} // Reset in PSRAM, no full Cache temporary on the UI stack.
static bool active(lv_obj_t *parent){return cache&&cache->parent==parent;}
static void label(lv_obj_t *object,const char *text){
  if(object&&strcmp(lv_label_get_text(object),text))lv_label_set_text(object,text);
}
static uint64_t hash(const char *a,const char *b,const char *c,unsigned color=0){
  uint64_t h=14695981039346656037ULL;
  const char *parts[]={a,b,c};
  for(const char *s:parts){for(;*s;++s){h^=uint8_t(*s);h*=1099511628211ULL;}h^=0xff;h*=1099511628211ULL;}
  return (h^color)*1099511628211ULL;
}
static void header(const char *name,const char *subtitle,const char *history){
  const int width=lv_obj_get_width(cache->parent)-16;
  const auto fingerprint=hash(name,"","");
  if(!cache->title){cache->title=single_label(cache->parent,"",8,8,width,white);lv_obj_set_height(cache->title,19);}
  if(cache->title_hash!=fingerprint){
#if defined(MCX_EMOJI_RENDER_ENABLED)
    mcxemoji::view::set_message_text(cache->title,name);
#else
    label(cache->title,name);
#endif
    cache->title_hash=fingerprint;
  }
  if(!cache->subtitle)cache->subtitle=single_label(cache->parent,subtitle,8,30,width,muted);
  else label(cache->subtitle,subtitle);
  if(!cache->history)cache->history=single_label(cache->parent,history,114,61,width-106,muted);
  else label(cache->history,history);
}
static bool interacting(lv_obj_t *parent){
  if(mcxtouch::busy()||lv_obj_is_scrolling(parent))return true;
  for(auto *d=lv_indev_get_next(nullptr);d;d=lv_indev_get_next(d))
    if(d->proc.state==LV_INDEV_STATE_PRESSED||lv_indev_get_scroll_obj(d))return true;
  return false;
}
static bool begin(lv_obj_t *parent){
  if(!cache){void *p=lv_mem_alloc(sizeof(Cache));if(!p)return false;cache=new(p) Cache();}
  const bool initial=cache->parent!=parent;
  if(initial){new(cache) Cache();cache->parent=parent;}
  lv_obj_update_layout(parent);
  const int scroll=lv_obj_get_scroll_y(parent);
  cache->follow=initial||lv_obj_get_scroll_bottom(parent)<=24;
  cache->anchor=0;cache->offset=0;cache->y=96;
  for(auto &r:cache->rows){r.used=false;
    if(r.object&&lv_obj_get_y(r.object)+lv_obj_get_height(r.object)>scroll&&
       (!cache->anchor||lv_obj_get_y(r.object)<cache->offset+scroll)){
      cache->anchor=r.id;cache->offset=lv_obj_get_y(r.object)-scroll;
    }
  }
  return true;
}
template<class Create> static void row(uint32_t id,uint64_t fingerprint,Create create){
  Row *slot=nullptr;
  for(auto &r:cache->rows)if(r.id==id){slot=&r;break;}
  if(!slot)for(auto &r:cache->rows)if(!r.object){slot=&r;break;}
  // Evict an unseen oldest row when the bounded message ring rolls over.
  if(!slot)for(auto &r:cache->rows)if(!r.used&&(!slot||r.id<slot->id))slot=&r;
  if(!slot)return;
  if(slot->id!=id||slot->hash!=fingerprint){
    if(slot->object)lv_obj_del(slot->object);
    slot->object=create(cache->y);slot->id=id;slot->hash=fingerprint;
    // set_height() dirties layout; get_height() otherwise returns the previous
    // coordinates and packs variable-height cards differently on next refresh.
    lv_obj_update_layout(slot->object);
  }
  slot->used=true;lv_obj_set_y(slot->object,cache->y);
  cache->y+=lv_obj_get_height(slot->object)+8;
}
static void end(const char *empty_text){
  bool any=false;
  for(auto &r:cache->rows){if(r.object&&!r.used){lv_obj_del(r.object);r=Row{};}any|=r.object!=nullptr;}
  if(any&&cache->empty){lv_obj_del(cache->empty);cache->empty=nullptr;}
  if(!any&&!cache->empty)cache->empty=single_label(cache->parent,empty_text,8,100,lv_obj_get_width(cache->parent)-16,muted);
  lv_obj_update_layout(cache->parent);
  if(cache->follow)lv_obj_scroll_to_y(cache->parent,cache->y,LV_ANIM_OFF);
  else for(auto &r:cache->rows)if(r.id==cache->anchor){lv_obj_scroll_to_y(cache->parent,lv_obj_get_y(r.object)-cache->offset,LV_ANIM_OFF);break;}
}
}
