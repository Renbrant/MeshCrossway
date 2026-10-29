#include "meshcrossway_meshcore_protocol.h"
#include <cassert>
#include <memory>
#include <random>
using namespace meshmc;
static std::unique_ptr<Client> make_client(){return std::unique_ptr<Client>(new Client());}
static Frame response(uint8_t code,size_t n){Frame f;f.len=n;f.data[0]=code;return f;}
static void recv(Client &c,const Frame &f,uint32_t now){assert(c.receive(f.data,f.len,now));}
static Frame start(Client &c,uint32_t now){Frame f;assert(c.next(now,f));return f;}
static void bootstrap(Client &c){
 c.attach(173);uint32_t t=100;
 auto f=start(c,t);assert(f.data[0]==1&&f.len==20);
 auto r=response(5,62);r.data[2]=20;r.data[3]=22;memcpy(r.data+58,"Test",4);put32(r.data+48,910525);put32(r.data+52,62500);r.data[56]=7;r.data[57]=5;
 // Radio notification may arrive before ATT write completion; do not send again yet.
 recv(c,r,t);assert(c.busy);assert(!c.next(t+100,f));c.write_done(true);assert(!c.busy);
 t+=200;f=start(c,t);assert(f.data[0]==22&&f.data[1]==14);c.write_done(true);r=response(13,82);r.data[1]=14;r.data[2]=32;r.data[3]=2;put32(r.data+4,123456);memcpy(r.data+60,"1.18",4);recv(c,r,t);assert(c.channel_count==2);assert(u32(c.last_frame.data+4)==0);assert(!strstr(c.result,"123456"));
 t+=100;f=start(c,t);assert(f.data[0]==5);c.write_done(true);r=response(9,5);put32(r.data+1,1800000000);recv(c,r,t);
 t+=100;f=start(c,t);assert(f.data[0]==4);c.write_done(true);r=response(2,5);put32(r.data+1,1);recv(c,r,t);
 r=response(3,152);r.data[1]=0xab;r.data[33]=1;r.data[35]=255;memcpy(r.data+100,"Heltec",6);put32(r.data+132,1800000000);recv(c,r,t);assert(c.busy);
 r=response(4,5);recv(c,r,t);assert(!c.busy&&c.contact_count==1);
 for(int i=0;i<2;i++){t+=100;f=start(c,t);assert(f.data[0]==31&&f.data[1]==i);c.write_done(true);r=response(18,50);r.data[1]=i;memcpy(r.data+2,"Public",6);recv(c,r,t);}
 t+=100;f=start(c,t);assert(f.data[0]==20);c.write_done(true);r=response(12,11);r.data[1]=0x68;r.data[2]=0x10;recv(c,r,t);
 t+=100;f=start(c,t);assert(c.initialized&&f.data[0]==10);c.write_done(true);r=response(10,1);recv(c,r,t);
 assert(!strcmp(c.name,"Test")&&!strcmp(c.firmware,"1.18"));
}
int main(){
 auto ptr=make_client();Client &c=*ptr;bootstrap(c);Frame f,r;
 // V3 and legacy offline messages, signed-text offset, asynchronous waiting and deduplication.
 r=response(17,16);r.data[1]=(uint8_t)-16;r.data[4]=1;r.data[5]=3;put32(r.data+7,1800000010);memcpy(r.data+11,"hello",5);recv(c,r,2000);
 assert(c.message_count==1&&c.messages[0].channel&&c.messages[0].slot==1&&!strcmp(c.messages[0].text,"hello")&&c.messages[0].snr==-16);
 recv(c,r,2000);assert(c.message_count==1);
 r=response(7,22);r.data[1]=0xab;r.data[8]=2;put32(r.data+9,1800000011);memcpy(r.data+13,"sign",4);memcpy(r.data+17,"world",5);recv(c,r,2000);
 assert(c.message_count==2&&!strcmp(c.messages[1].text,"world")&&!memcmp(c.messages[1].signer,"sign",4));
 c.waiting_messages=false;
 assert(c.send_text(false,0,"direct"));f=start(c,2200);assert(f.data[0]==2&&f.len==19&&f.data[7]==0xab);
 c.write_done(true);
 r=response(131,1);recv(c,r,2201);assert(c.busy); // push cannot complete outbound command.
 r=response(6,10);put32(r.data+2,0x1234abcd);put32(r.data+6,5000);recv(c,r,2202);
 auto *m=c.by_id(3);assert(m&&m->delivery==ACCEPTED&&m->ack==0x1234abcd);
 r=response(130,9);put32(r.data+1,0x1234abcd);recv(c,r,2203);assert(m->delivery==DELIVERED);
 c.waiting_messages=false;
 assert(c.send_text(true,1,"group"));f=start(c,2400);assert(f.data[0]==3&&f.data[2]==1&&f.len==12);c.write_done(true);
 r=response(0,1);recv(c,r,2401);assert(c.by_id(4)->delivery==ACCEPTED); // group has no recipient delivery ACK.
 assert(c.send_text(false,0,"uncertain"));f=start(c,2600);c.write_done(true);assert(!c.next(13000,f));assert(c.fault&&c.by_id(5)->delivery==UNCERTAIN);
 c.detach();assert(c.by_id(3)->delivery==DELIVERED); // confirmed history retained until a new session.
 bootstrap(c);
 // Full key identity, prefix collision rejection, queue limits and version gating.
 c.contacts[1]=c.contacts[0];c.contacts[1].key[31]=1;c.contact_count=2;assert(!c.send_text(false,0,"ambiguous"));
 c.contact_count=1;c.protocol=7;assert(!c.simple(56,0));c.protocol=14;
 for(int i=0;i<12;i++)assert(c.simple(20));assert(!c.simple(20));c.qcount=0;
 // T-Deck / other Companion firmware may return documented optional fields.
 // Do not classify these packets as corrupted or disconnect the radio.
 auto variants=make_client();bootstrap(*variants);
 r=response(0,5);put32(r.data+1,0x10203040);recv(*variants,r,2900);
 assert(!variants->fault);
 r=response(1,1);recv(*variants,r,2910);
 assert(!variants->fault);
 r=response(27,8);recv(*variants,r,2920); // empty channel datagram
 assert(!variants->fault && variants->waiting_messages);
 // Malformed frames must still fail safely and report a code/length only.
 r=response(0,3);assert(!variants->receive(r.data,r.len,2930));
 assert(variants->fault && strstr(variants->status,"0x00") &&
        strstr(variants->status,"len 3"));
 auto older=make_client();older->attach(173);
 r=response(13,4);r.data[1]=2;
 assert(!older->receive(r.data,r.len,2931) && older->fault);
 assert(strstr(older->status,"protocol v2 unsupported"));
 auto short_info=make_client();short_info->attach(173);
 r=response(13,6);r.data[1]=14;
 assert(!short_info->receive(r.data,r.len,2932) && short_info->fault);
 assert(strstr(short_info->status,"0x0D") && strstr(short_info->status,"len 6"));
 // No transport payload is exposed in diagnostics.
 assert(!strstr(older->status,"private") && !strstr(older->status,"secret"));
 // Wrong channel response does not release GET_CHANNEL; malformed responses fail the session.
 assert(c.simple(31,1));f=start(c,3000);c.write_done(true);r=response(18,50);r.data[1]=0;recv(c,r,3001);assert(c.busy);r.data[1]=1;recv(c,r,3002);assert(!c.busy);
 r=response(5,57);assert(!c.receive(r.data,r.len,3003)&&c.fault);
 // A remote request is not complete merely because the local radio accepted it.
 bootstrap(c);c.waiting_messages=false;
 Frame request;request.len=33;request.data[0]=27;memcpy(request.data+1,c.contacts[0].key,32);assert(c.enqueue(request));
 f=start(c,3000);c.write_done(true);r=response(6,10);put32(r.data+2,77);put32(r.data+6,10000);recv(c,r,3001);assert(c.remote_pending==27);
 assert(c.enqueue(request));assert(!c.next(3200,f));
 r=response(135,8);r.data[2]=0xcd;recv(c,r,3201);assert(c.remote_pending==27);
 r.data[2]=0xab;recv(c,r,3202);assert(c.remote_pending==0);f=start(c,3300);assert(f.data[0]==27);
 char telem[300];const uint8_t lpp[]={1,116,1,0x9a,2,103,0xff,0x9c,3,104,100};
 telemetry_text(lpp,sizeof(lpp),telem,sizeof(telem));assert(strstr(telem,"4.100 V")&&strstr(telem,"-10.000 C")&&strstr(telem,"50.000 %"));
 telemetry_text(lpp,3,telem,sizeof(telem));assert(strstr(telem,"Truncated"));
 // Typed encoders use official fixed layouts, units, padding and bounds.
 char error[128];const char *ch[]={"1","#trail","00112233445566778899aabbccddeeff"};assert(build(*command(32),ch,3,f,error,sizeof(error)));assert(f.len==50&&f.data[1]==1&&f.data[34]==0&&f.data[49]==255);
 const char *radio[]={"910525","62500","7","5","0"};assert(build(*command(11),radio,5,f,error,sizeof(error)));assert(f.len==12&&u32(f.data+1)==910525&&u32(f.data+5)==62500);
 const char *negative[]={"-10"};assert(!build(*command(12),negative,1,f,error,sizeof(error)));
 const char *overflow[]={"4294967296"};assert(!build(*command(6),overflow,1,f,error,sizeof(error)));
 const char *pin[]={"99999"};assert(!build(*command(37),pin,1,f,error,sizeof(error)));
 const char *data[]={"1","255","","65535","a1b2"};assert(build(*command(62),data,5,f,error,sizeof(error))&&f.len==7&&f.data[3]==255);
 const char *badkey[]={"1","Public","0"};assert(!build(*command(32),badkey,3,f,error,sizeof(error)));
 assert(build(*command(19),nullptr,0,f,error,sizeof(error))&&f.len==7&&!memcmp(f.data+1,"reboot",6));
 assert(build(*command(51),nullptr,0,f,error,sizeof(error))&&f.len==6&&!memcmp(f.data+1,"reset",5));
 // Every catalog entry has a unique opcode. All official command families must remain reachable.
 assert(COMMAND_COUNT==59);for(size_t i=0;i<COMMAND_COUNT;i++)for(size_t j=i+1;j<COMMAND_COUNT;j++)assert(COMMANDS[i].code!=COMMANDS[j].code);
 // Group text reserves sender prefix space; consecutive sends never reuse timestamps.
 auto budget=make_client();bootstrap(*budget);memset(budget->name,'N',31);budget->name[31]=0;
 assert(budget->text_limit(true)==127);char longtext[151];memset(longtext,'x',150);longtext[150]=0;assert(!budget->send_text(true,0,longtext));
 assert(budget->send_text(false,0,"one")&&budget->send_text(false,0,"two"));assert(budget->by_id(2)->timestamp>budget->by_id(1)->timestamp);
 // Automatic inbox maintenance must not erase the last tool response.
 auto view=make_client();bootstrap(*view);assert(view->simple(20));f=start(*view,3000);view->write_done(true);r=response(12,11);r.data[1]=0x68;r.data[2]=0x10;recv(*view,r,3001);assert(strstr(view->result,"Battery"));
 char saved[800];strcpy(saved,view->result);view->waiting_messages=true;f=start(*view,3200);assert(f.data[0]==10);view->write_done(true);r=response(10,1);recv(*view,r,3201);assert(!strcmp(saved,view->result));
 r=response(139,12);r.data[8]=1;r.data[9]=116;r.data[10]=1;r.data[11]=0x9a;recv(*view,r,3202);assert(view->last_frame.data[0]==139&&strstr(view->result,"4.100 V"));
 // Sanitizer corpus: all packet types and truncated lengths, plus random oversized/truncated frames.
 std::mt19937 random(7);auto fuzz=make_client();uint8_t bytes[MAX_FRAME+1];
 for(int code=0;code<256;code++)for(size_t n=0;n<=MAX_FRAME+1;n++){
  for(auto &b:bytes)b=random();bytes[0]=code;fuzz->connected=true;fuzz->channel_count=40;fuzz->busy=false;fuzz->fault=false;
  fuzz->receive(bytes,n,4000);
 }
 puts("PASS: MeshCore session bootstrap, wire fixtures, notifications, ACKs, timeouts, typed commands, bounds and sanitizer corpus");
}
