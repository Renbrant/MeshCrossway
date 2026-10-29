#include "meshcrossway_meshtastic_store.h"
#include <cassert>
#include <memory>
#include <vector>
#include <string>

using namespace meshmt;
static std::vector<uint8_t> hex(const char *s){
  std::vector<uint8_t> out;unsigned v;
  while(*s){assert(sscanf(s,"%2x",&v)==1);out.push_back(v);s+=2;}return out;
}
static void receive(Client &c,const char *wire,uint32_t now=100){
  auto v=hex(wire);assert(c.receive(v.data(),v.size(),now));
}
static Frame envelope(unsigned tag,const Frame &body){Frame out;Writer w(out);w.message(tag,body);assert(w.ok);return out;}
static void inject(Client &c,unsigned tag,const Frame &body,uint32_t now=100){auto f=envelope(tag,body);assert(c.receive(f.data,f.len,now));}
static void synchronize(Client &c){
  const uint8_t peer[7]={0,1,2,3,4,5,6};c.attach(peer,517,1234,0);
  Frame f;assert(c.next(1,f));assert(f.len==3&&!memcmp(f.data,"\x18\xd2\x09",3));c.write_done(true);
  receive(c,"1a0508f8acd111");assert(c.data.my_num==0x02345678);
  // FromRadio.metadata: firmware_version "2.7.15", hw_model 7, hasPKC true.
  receive(c,"6a0c0a06322e372e313548075801");
  receive(c,"2a083206380140034801"); // config.lora: region US (1), hops 3, tx true
  receive(c,"520d12091201011a04746573741801"); // channel zero, primary, PSK shorthand 1, "test"
  receive(c,"38d109");assert(!c.initialized); // wrong config completion must not release sends
  receive(c,"38d209");assert(c.initialized&&!c.history_ready);c.history_ready=true;
}
static Frame text_packet(uint32_t from,uint32_t to,uint32_t id,const char *text,unsigned channel=0){
  Frame data,packet;Writer d(data);d.scalar(1,1);d.bytes(2,text,strlen(text));
  Writer p(packet);p.fixed(1,from);p.fixed(2,to);p.scalar(3,channel);p.message(4,data);p.fixed(6,id);return packet;
}
static void ack(Client &c,uint32_t from,uint32_t request,unsigned reason,uint32_t to=0){
  Frame route,data,packet;Writer r(route);r.scalar(3,reason);
  Writer d(data);d.scalar(1,5);d.message(2,route);d.fixed(6,request);
  Writer p(packet);p.fixed(1,from);p.fixed(2,to?to:c.data.my_num);p.message(4,data);
  inject(c,2,packet);
}
int main(int argc,char **argv){
  assert(argc==2);auto ptr=std::make_unique<Client>();auto &c=*ptr;synchronize(c);
  assert(c.has_pki&&c.hardware==7&&c.tx_enabled&&c.region==1&&c.hop_limit==3);
  auto *n=c.node(0x12345678,true);strcpy(n->name,"T-Deck peer");n->has_key=true;memset(n->public_key,0x55,32);
  Target target=c.target(n->num,0);assert(!c.send_problem(target,"hello"));
  auto wrong=target;wrong.radio[1]^=1;assert(c.send_problem(wrong,"hello"));
  wrong=target;wrong.channel_identity++;assert(c.send_problem(wrong,"hello"));
  assert(c.send_text(target,"hello",0x10203040,200));
  assert(c.message(0x10203040)->delivery==QUEUED);Frame outgoing;assert(c.next(201,outgoing));
  // Golden full ToRadio serialization: no 0x94c3 stream framing, fixed32 dest/id,
  // channel index zero, TEXT_MESSAGE_APP, reliable direct, public key and PKI.
  auto expected=hex("0a401578563412180022090801120568656c6c6f3540302010480350015846820120");
  expected.insert(expected.end(),32,0x55);expected.push_back(0x88);expected.push_back(0x01);expected.push_back(0x01);
  // Outer size is payload length; bytes above are schema facts, not SDK objects.
  expected[1]=uint8_t(expected.size()-2);
  assert(outgoing.len==expected.size()&&!memcmp(outgoing.data,expected.data(),expected.size()));
  c.write_done(true);assert(c.message(0x10203040)->delivery==SUBMITTED);
  ack(c,0x99887766,0x10203040,0);assert(c.message(0x10203040)->delivery==SUBMITTED);
  ack(c,c.data.my_num,0x10203040,0);assert(c.message(0x10203040)->delivery==RADIO_ACCEPTED);
  ack(c,n->num,0x10203040,0);assert(c.message(0x10203040)->delivery==REMOTE_ACK);
  auto incoming=text_packet(n->num,c.data.my_num,999,"Olá 😀");inject(c,2,incoming);
  const auto messages=c.data.message_count;inject(c,2,incoming);assert(c.data.message_count==messages);
  auto alien=text_packet(n->num,123,1000,"not addressed to us");inject(c,2,alien);assert(c.data.message_count==messages);
  auto broadcast=c.target(BROADCAST,0);assert(c.send_text(broadcast,"channel",500,300));assert(c.next(301,outgoing));c.write_done(true);
  ack(c,c.data.my_num,500,0);assert(c.message(500)->delivery==RADIO_ACCEPTED);
  ack(c,n->num,500,0);assert(c.message(500)->delivery!=REMOTE_ACK);
  assert(!c.next(121000,outgoing)||c.writing);assert(c.message(500)->delivery==UNCERTAIN);if(c.writing)c.write_done(true);
  assert(c.send_text(target,"rejected",501,122000));assert(c.next(122001,outgoing));c.write_done(true);
  ack(c,n->num,501,3);assert(c.message(501)->delivery==REJECTED&&c.message(501)->error==3);
  std::string oversized(238,'x');assert(!c.send_text(target,oversized.c_str(),502,130000));
  assert(!utf8("\xc0\xaf",2)&&!utf8("\xed\xa0\x80",3)&&!utf8("\xf4\x90\x80\x80",4));
  assert(c.send_text(target,std::string(237,'x').c_str(),502,130000));assert(c.next(130001,outgoing));c.write_done(true);
  c.tx_enabled=false;assert(c.send_problem(target,"off"));c.tx_enabled=true;
  c.data.channels[0].identity++;assert(c.send_problem(target,"changed"));c.data.channels[0].identity--;
  // Zero coordinates are valid. Unknown fields do not reject future extensions.
  Frame position,packet;Writer pos(position);pos.fixed(1,0);pos.fixed(2,0);
  Frame data;Writer d(data);d.scalar(1,3);d.message(2,position);
  Writer p(packet);p.fixed(1,n->num);p.fixed(2,BROADCAST);p.message(4,data);inject(c,2,packet);
  assert(n->position&&n->latitude==0&&n->longitude==0);
  receive(c,"a20603616263"); // unknown future field 100
  // Persistence includes drafts, separates radio identity, survives corrupt active
  // file using the previous complete image, and never rebuilds a TX queue.
  auto *draft=c.draft(target,true);strcpy(draft->text,"preserve draft");
  assert(meshmtstore::save(argv[1],c.data));assert(meshmtstore::save(argv[1],c.data));
  auto restored=std::make_unique<Archive>();assert(meshmtstore::load(argv[1],c.data.radio,*restored)==1);
  auto restored_client=std::make_unique<Client>();synchronize(*restored_client);meshmtstore::merge(*restored_client,*restored);
  assert(restored_client->data.message_count==c.data.message_count&&!restored_client->pending_ready&&!restored_client->writing);
  assert(restored_client->message(502)->delivery==UNCERTAIN);
  assert(!strcmp(restored_client->draft(target)->text,"preserve draft"));
  char path[240];meshmtstore::path(path,sizeof(path),argv[1],c.data.radio,"bin");FILE *f=fopen(path,"wb");assert(f);fputs("torn",f);fclose(f);
  assert(meshmtstore::load(argv[1],c.data.radio,*restored)==1);
  uint8_t other[7]={1};assert(meshmtstore::load(argv[1],other,*restored)==0);
  c.detach();assert(!c.send_text(target,"offline",900,140000));
  synchronize(c);auto locked=hex("9201020802");assert(!c.receive(locked.data(),locked.size(),200));assert(c.fault); // locked API
  // Bounded random malformed input under ASan/UBSan; no heap allocation by parser.
  uint32_t random=0x65a71;uint8_t bytes[512];
  for(unsigned iteration=0;iteration<20000;++iteration){
    size_t len=iteration%sizeof(bytes);for(size_t i=0;i<len;++i){random=random*1664525+1013904223;bytes[i]=random>>24;}
    c.connected=true;c.fault=false;c.receive(bytes,len,150000);
  }
  printf("PASS: Meshtastic golden wire, handshake, independent destinations, PKI, ACK/NAK, bounds, UTF-8, position, dedup, timeout, SD recovery and 20000 malformed frames; client=%zu archive=%zu bytes\n",sizeof(Client),sizeof(Archive));
}
