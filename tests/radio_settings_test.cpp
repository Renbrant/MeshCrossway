#include "meshcrossway_meshtastic_protocol.h"
#include <cassert>
#include <memory>
#include <string>
#include <cstdio>
using namespace meshmt;
static unsigned section(unsigned family,unsigned tag){for(unsigned i=0;i<settings::SECTION_COUNT;++i)if(settings::sections[i].family==family&&settings::sections[i].tag==tag)return i;assert(false);return 0;}
static unsigned field(unsigned section,const char *name){const auto &s=settings::sections[section];for(unsigned i=0;i<s.count;++i)if(!strcmp(s.fields[i].name,name))return i;assert(false);return 0;}
static View view(const Frame &f){return {f.data,f.len};}
static Frame response(unsigned group,const Frame &config){const auto &s=settings::sections[group];Frame a,wrapper;Writer w(a);uint8_t pass[8]={1,2,3,4,5,6,7,8};w.bytes(101,pass,8);
 if(s.family<2){Writer wr(wrapper);wr.message(s.tag,config);w.message(s.family?8:6,wrapper);}else if(s.family>=5){auto text=settings::child(view(config),1);w.bytes(s.family==5?11:15,text.data,text.len);}else w.message(s.family==2?4:s.family==3?2:45,config);return a;}
static void loaded(Settings &s,unsigned group,const Frame &config){assert(s.read(group));Frame req;assert(s.next(10,100,req));auto a=response(group,config);assert(s.receive(view(a),100));if(!s.loaded||s.stage!=Settings::LOADED)fprintf(stderr,"group=%u family=%u stage=%u status=%s\n",group,settings::sections[group].family,s.stage,s.status);assert(s.loaded&&s.stage==Settings::LOADED);}
static Frame extract(const Frame &f,unsigned tag){auto v=settings::child(view(f),tag);Frame a;assert(v.len<=MAX_FRAME);a.len=v.len;if(v.len)memcpy(a.data,v.data,v.len);return a;}
static void inject(Client &c,unsigned port,const Frame &payload,uint32_t id,uint32_t from){Frame data,packet,out;Writer d(data);d.scalar(1,port);d.message(2,payload);d.fixed(6,id);Writer p(packet);p.fixed(1,from);p.fixed(2,c.data.my_num);p.message(4,data);Writer o(out);o.message(2,packet);assert(c.receive(out.data,out.len,100));}
int main(){
 auto ptr=std::make_unique<Settings>();auto &s=*ptr;const unsigned lora=section(0,6),region=field(lora,"region");const auto &spec=settings::sections[lora].fields[region];
 Frame cfg;Writer w(cfg);w.scalar(7,1);w.scalar(8,3);w.bytes(117,"unknown future setting",22);loaded(s,lora,cfg);
 assert(!s.apply(lora,region,"9999",s.revision));assert(!s.apply(lora,region,"3",s.revision-1));assert(s.apply(lora,region,"3",s.revision));
 Frame request;assert(s.next(20,101,request));auto fresh=response(lora,cfg);assert(s.receive(view(fresh),100)&&s.stage==Settings::FRESH_WAIT);assert(s.receive(view(fresh),101)&&s.stage==Settings::WRITE_SEND);
 assert(s.next(30,102,request));Frame wrapper=extract(request,34),written=extract(wrapper,6);char text[1025];assert(settings::value(view(written),spec,text,sizeof(text))&&!strcmp(text,"3"));auto unknown=settings::child(view(written),117);assert(unknown.len==22&&!memcmp(unknown.data,"unknown future setting",22));assert(settings::child(view(request),101).len==8);
 s.routing(101,0);assert(s.stage==Settings::WRITE_WAIT);s.routing(102,0);assert(s.stage==Settings::VERIFY_SEND);assert(s.next(40,103,request));auto checked=response(lora,written);assert(s.receive(view(checked),103)&&s.stage==Settings::DONE);
 // Stale form cannot overwrite another client's changes; no write retry on timeout.
 assert(s.apply(lora,region,"1",s.revision));assert(s.next(50,104,request));Frame changed;Writer wc(changed);wc.scalar(7,3);wc.scalar(8,4);fresh=response(lora,changed);assert(s.receive(view(fresh),104)&&s.stage==Settings::ERROR);assert(!s.next(60,105,request));
 assert(s.read(lora));assert(s.next(100,106,request));assert(!s.next(15101,107,request)&&s.stage==Settings::ERROR);assert(!s.next(20000,108,request));
 loaded(s,lora,cfg);assert(s.apply(lora,region,"1",s.revision));assert(s.next(10,109,request));s.routing(109,32);assert(s.stage==Settings::ERROR);
 // Nested patch and repeated packed fields retain unrelated bytes exactly.
 settings::Spec nested{{2,5,0,0},2,13,false,false,false,"nested",""};Frame inner,outer;Writer wi(inner);wi.scalar(5,17);wi.scalar(44,99);Writer wo(outer);wo.message(2,inner);wo.bytes(88,"secret",6);assert(settings::patch(view(outer),nested,"18",written,s.work));assert(settings::value(view(written),nested,text,sizeof(text))&&!strcmp(text,"18"));assert(settings::child(view(written),88).len==6);auto inner2=extract(written,2);Reader ri(view(inner2));Field f;bool kept=false;while(ri.next(f))if(f.tag==44&&f.value==99)kept=true;assert(kept);
 settings::Spec repeated{{3,0,0,0},1,13,true,false,false,"repeat",""};Frame packed;Writer wp(packed);const uint8_t values[]={1,2,3};wp.bytes(3,values,3);assert(settings::value(view(packed),repeated,text,sizeof(text))&&!strcmp(text,"1,2,3"));assert(settings::patch(view(packed),repeated,"4,5",written,s.work));assert(settings::value(view(written),repeated,text,sizeof(text))&&!strcmp(text,"4,5"));
 // Reject invalid ranges, malformed hex/UTF-8, readonly writes and oversized payloads.
 settings::Spec number{{1,0,0,0},1,13,false,false,false,"uint",""};assert(!settings::atom(number,"4294967296",request));assert(!settings::atom(number,"-1",request));number.type=5;assert(!settings::atom(number,"2147483648",request));assert(settings::atom(number,"-2147483648",request));assert(settings::value(view(request),number,text,sizeof(text))&&!strcmp(text,"-2147483648"));number.type=12;assert(!settings::atom(number,"a",request)&&!settings::atom(number,"zz",request));number.type=9;assert(!settings::atom(number,"\xc0\xaf",request));number.type=2;assert(!settings::atom(number,"nan",request));number.readonly=true;assert(!settings::patch({},number,"1",request,s.work));
 // Special string settings and Device UI use their own actual admin commands.
 for(auto family:{4u,5u,6u}){unsigned group=section(family,family==4?10:0);Frame value;Writer v(value);if(family>=5)v.bytes(1,"hello",5);loaded(s,group,value);assert(s.loaded);}
 // Real Client outgoing envelope is addressed to self and has no mesh hops.
 auto cp=std::make_unique<Client>();auto &c=*cp;c.connected=c.initialized=c.history_ready=c.config_sent=true;c.data.my_num=1234;c.config_nonce=77;
 assert(c.settings.read(lora));Frame out;assert(c.next(100,out));auto packet=extract(out,1);Reader pr(view(packet));bool self=false,local=false;while(pr.next(f)){if(f.tag==2)self=f.value==1234;if(f.tag==9)local=f.value==0;}assert(self&&local);c.write_done(true);
 const unsigned id=c.settings.request_id;auto reply=response(lora,cfg);inject(c,6,reply,id,9999);assert(!c.settings.loaded);inject(c,6,reply,id,1234);assert(c.settings.loaded&&c.region==1&&c.hop_limit==3);
 c.detach();assert(!c.settings.loaded&&!c.settings.passkey_len&&!c.settings.current.len);
 printf("PASS: %u settings groups, unknown-field preservation, nested/packed edits, numeric/hex bounds, local-only admin envelopes, fresh authorization, stale edit rejection, readback, NAK, timeout and disconnect\n",settings::SECTION_COUNT);
}
