#include "meshcrossway_meshtastic_store.h"
#include "meshcrossway_meshcore_protocol.h"
#include <cassert>
#include <memory>
#include <cstdio>
#include <string>
#include <sys/stat.h>
static bool pinned(const uint8_t *key){return key[0]==4;}
int main(int argc,char **argv){
 assert(argc==2);const uint32_t now=1800000000u,D=mcxnodeage::DAY;
 assert(mcxnodeage::color(now,now)==0x36D399);
 assert(mcxnodeage::color(now-D+1,now)==0x36D399);
 assert(mcxnodeage::color(now-D,now)==0xFFD45A);
 assert(mcxnodeage::color(now-7*D+1,now)==0xFFD45A);
 assert(mcxnodeage::color(now-7*D,now)==0x8993A1);
 assert(mcxnodeage::color(0,now)==0x8993A1&&mcxnodeage::color(now+1,now)==0x8993A1);
 assert(!mcxnodeage::expired_at(now-7*D,now,7));
 assert(mcxnodeage::expired_at(now-7*D-1,now,7));
 assert(!mcxnodeage::expired_at(0,now,7)&&!mcxnodeage::expired_at(now+1,now,7));
 assert(!mcxnodeage::expired_at(now-100*D,now,0)&&!mcxnodeage::expired_at(now-100*D,0,7));
 mcxnodeage::epoch.store(now);mcxnodeage::retention.store(7);
 auto mt=std::make_unique<meshmt::Client>();mt->data.my_num=1;
 for(unsigned id=1;id<=6;++id){auto *n=mt->node(id,true);n->last_heard=now-20*D;mt->node_roles[id-1]=uint8_t(id);}
 mt->data.favorites[1]=1;mt->data.messages[0].seq=1;mt->data.messages[0].from=3;
 mt->data.drafts[0].used=true;mt->data.drafts[0].target.node=4;mt->data.nodes[4].last_heard=0;
 assert(mt->prune_nodes()==1&&mt->data.node_count==5&&!mt->node(6));
 assert(mt->node(1)&&mt->node(2)&&mt->node(3)&&mt->node(4)&&mt->node(5));
 meshmt::Frame f;meshmt::Writer w(f);w.scalar(1,7);w.fixed(5,now-20*D);
 assert(mt->parse_node({f.data,f.len})&&!mt->node(7)); // successful sync, no stale resurrection
 meshmt::Frame fav;meshmt::Writer fw(fav);fw.scalar(1,8);fw.fixed(5,now-20*D);fw.scalar(10,1);
 assert(mt->parse_node({fav.data,fav.len})&&mt->node(8));
 mt->data.messages[0].seq=0;mt->data.drafts[0].used=false;
 assert(mt->prune_nodes()==2&&mt->node(8));
 for(unsigned i=0;i<mt->data.node_count;++i)assert(mt->node_roles[i]<=6);
 // v1 prefix migration: checksum old bytes BEFORE adding unknown-favorite protection.
 const std::string file=std::string(argv[1])+"/legacy.bin";auto saved=std::make_unique<meshmt::Archive>();
 saved->node_count=1;saved->nodes[0].num=42;saved->nodes[0].last_heard=now-100*D;
 meshmtstore::Header h;h.version=1;h.bytes=offsetof(meshmt::Archive,favorites);h.checksum=meshmtstore::checksum(*saved,h.bytes);
 FILE *fp=fopen(file.c_str(),"wb");assert(fp);assert(fwrite(&h,1,sizeof(h),fp)==sizeof(h));assert(fwrite(saved.get(),1,h.bytes,fp)==h.bytes);assert(!fclose(fp));
 auto loaded=std::make_unique<meshmt::Archive>();assert(meshmtstore::read(file.c_str(),saved->radio,*loaded)==1&&loaded->favorites[0]==2);
 auto live=std::make_unique<meshmt::Client>();live->connected=live->initialized=true;meshmtstore::merge(*live,*loaded);assert(!live->node(42));
 auto offline=std::make_unique<meshmt::Client>();meshmtstore::merge(*offline,*loaded);assert(offline->node(42)&&!offline->prune_nodes());
 char legacy_path[256];meshmtstore::path(legacy_path,sizeof(legacy_path),argv[1],saved->radio,"bin",true);
 assert(!rename(file.c_str(),legacy_path));assert(meshmtstore::load(argv[1],saved->radio,*loaded)==1&&loaded->favorites[0]==2);
 assert(meshmtstore::save(argv[1],*loaded));
 auto legacy_again=std::make_unique<meshmt::Archive>();assert(meshmtstore::read(legacy_path,saved->radio,*legacy_again)==1);auto again=std::make_unique<meshmt::Archive>();assert(meshmtstore::load(argv[1],saved->radio,*again)==1&&again->favorites[0]==2);
 auto mc=std::make_unique<meshmc::Client>();mc->contact_count=6;mc->public_key[0]=1;mc->history_ready=true;
 for(unsigned i=0;i<6;++i){mc->contacts[i].key[0]=uint8_t(i+1);mc->contacts[i].advert=now-20*D;}
 mc->contacts[1].flags=1;mc->messages[0].id=1;mc->messages[0].key[0]=3;
 mcxnodeage::pin_lookup()=pinned;mc->contacts[4].advert=0;
 uint8_t removed[32]={6};assert(mc->prune_contacts()==1&&mc->contact_count==5&&mc->find_contact(removed)<0);
 uint8_t p[152]={};p[0]=3;p[1]=7;p[33]=2;uint32_t old=now-20*D;memcpy(p+132,&old,4);
 assert(mc->parse_contact(p,sizeof(p))&&mc->contact_count==5); // no stale resync
 assert(mc->parse_contact(p,sizeof(p),true)&&mc->contact_count==6&&mc->contacts[5].advert==now);
 mcxnodeage::retention.store(0);assert(mc->prune_contacts()==0);
 puts("PASS: node age boundaries, expiry, clock safety, protected identities/favorites/messages/drafts, import filtering, fresh MC adverts, MT v1/v2 SD migration");
}
