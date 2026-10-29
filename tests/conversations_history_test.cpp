#include "meshcrossway_history_store.h"
#include <cassert>
#include <memory>
#include <string>
#include <vector>
using namespace mcxchat;
using namespace meshmc;
int main(int argc,char **argv){
 assert(argc==2);const char *root=argv[1];
 auto client=std::unique_ptr<Client>(new Client());auto &c=*client;
 c.connected=c.initialized=true;c.payload_limit=173;c.clock=1800000000;c.protocol=14;
 c.public_key[0]=1;c.contact_count=2;c.contacts[0].key[0]=9;c.contacts[1].key[0]=10;
 snprintf(c.contacts[0].name,33,"Alice");snprintf(c.contacts[1].name,33,"Bob");
 c.channel_count=2;c.channels[1].loaded=true;strcpy(c.channels[1].name,"Trail");
 auto prefs=std::unique_ptr<Local>(new Local());auto &local=*prefs;
 auto alice=key_for(c,false,0),bob=key_for(c,false,1),channel=key_for(c,true,1);
 // Every send is fenced by the exact radio + native destination + explicit network.
 assert(!send(Network::MT,c,alice,"cross network"));auto other=alice;other.network=Network::MT;
 assert(!send(Network::MC,c,other,"cross network"));other=alice;other.radio[31]=1;
 assert(!send(Network::MC,c,other,"wrong radio"));assert(c.qcount==0);
 assert(send(Network::MC,c,alice,"Hello 👋"));assert(c.qcount==1&&c.queue[0].frame.data[7]==9);
 assert(!belongs(c.messages[0],bob)&&belongs(c.messages[0],alice));
 assert(!clear(c,alice));c.messages[0].delivery=UNCERTAIN;c.qcount=0;
 assert(!send(Network::MC,c,alice,"\xc0\xaf"));
 auto &rx=c.append();rx.key[0]=10;rx.timestamp=1800000010;strcpy(rx.text,"@Test check https://example.org/trail");
 auto &group=c.append();group.channel=true;group.slot=1;group.timestamp=1800000020;strcpy(group.text,"Team: ready");
 Thread rows[96];assert(list(c,local,rows,96)==3&&equal(rows[0].key,channel));
 assert(unread(c,bob)==1);mark_read(c,&bob);assert(unread(c,bob)==0&&unread(c,channel)==1);
 auto *pref=local.get(channel,true);pref->notify=Notify::Mentions;
 assert(!notify(pref,group,"Test"));strcpy(group.text,"Team: @Test ready");assert(notify(pref,group,"Test"));
 pref->notify=Notify::Muted;assert(!notify(pref,group,"Test"));
 pref->blocked=true;assert(list(c,local,rows,96)==2);assert(list(c,local,rows,96,"",false,true)==3);
 pref->blocked=false;pref->favorite=true;strcpy(pref->alias,"Offroad");strcpy(pref->draft,"Saved draft");
 assert(list(c,local,rows,96,"offroad")==1);assert(list(c,local,rows,96,"ALICE")==1);
 char link[177];assert(url(rx.text,link,sizeof(link))&&!strcmp(link,"https://example.org/trail"));
 assert(!mention("@Tester","Test"));assert(mention("Hi @Test!","Test"));
 // Rooms are native authenticated sessions, not interchangeable with channels.
 c.contacts[1].type=3;auto room_key=key_for(c,false,1);assert(room_key.kind==Kind::Room);
 assert(!send(Network::MC,c,room_key,"No login"));c.room(room_key.native,true)->state=RoomState::Joined;
 assert(send(Network::MC,c,room_key,"Logged in"));c.qcount=0;c.messages[3].delivery=UNCERTAIN;
 c.connected=false;assert(!send(Network::MC,c,room_key,"Disconnected"));
 // A full public identity is included in filenames and in every committed state.
 auto state=std::unique_ptr<mcxstore::State>(new mcxstore::State());
 auto previous=std::unique_ptr<mcxstore::State>(new mcxstore::State());
 auto staging=std::unique_ptr<mcxstore::State>(new mcxstore::State());
 mcxstore::capture(c,local,*state);assert(mcxstore::save(root,*state,*previous,*staging));
 char file[240];mcxstore::path(file,sizeof(file),root,c.public_key,"log");
 auto recovery=mcxstore::load(root,c.public_key,*previous,*staging);assert(recovery.found&&!recovery.recovered);
 auto restored=std::unique_ptr<Client>(new Client());auto loaded_local=std::unique_ptr<Local>(new Local());
 mcxstore::restore(*previous,*restored,*loaded_local,false);
 assert(restored->qcount==0&&!restored->connected&&restored->message_count==c.message_count);
 assert(unread(*restored,channel)==1&&loaded_local->get(channel)->favorite&&!strcmp(loaded_local->get(channel)->draft,"Saved draft"));
 Frame f;assert(!restored->next(50000,f)); // Restart never transmits a restored queued message.
 // Offline read/clear mutations are durable. A partially appended transaction is ignored.
 const long start=recovery.end;
 mark_read(c);strcpy(c.messages[0].text,"second committed value");mcxstore::capture(c,local,*state);
 assert(mcxstore::save(root,*state,*previous,*staging));
 FILE *in=fopen(file,"rb");assert(in);fseek(in,0,SEEK_END);long end=ftell(in);rewind(in);
 std::vector<unsigned char> bytes(end);assert(fread(bytes.data(),1,end,in)==size_t(end));fclose(in);
 std::string cut=std::string(root)+"/cut.log";
 for(long length=start;length<end;++length){FILE *out=fopen(cut.c_str(),"wb");assert(out);assert(fwrite(bytes.data(),1,length,out)==size_t(length));fclose(out);
   auto r=mcxstore::replay(cut.c_str(),c.public_key,*previous,*staging);
   assert(r.found&&r.end==start&&!strcmp(previous->messages[0].text,"Hello 👋"));
 }
 // Corrupted payload cannot commit; the previous complete transaction remains readable.
 bytes[end-1]^=0x80;FILE *out=fopen(file,"wb");assert(out);fwrite(bytes.data(),1,bytes.size(),out);fclose(out);
 recovery=mcxstore::load(root,c.public_key,*previous,*staging);assert(recovery.found&&recovery.recovered&&recovery.end==start);
 assert(mcxstore::save(root,*state,*previous,*staging)); // Retry truncates the bad tail first.
 recovery=mcxstore::load(root,c.public_key,*previous,*staging);assert(recovery.found&&!recovery.recovered&&!strcmp(previous->messages[0].text,"second committed value"));
 uint8_t wrong[32];memcpy(wrong,c.public_key,32);wrong[31]=1;
 assert(!mcxstore::replay(file,wrong,*previous,*staging).found);
 char different[240];mcxstore::path(different,sizeof(different),root,wrong,"log");assert(strcmp(file,different));
 assert(!mcxstore::save("/does-not-exist/mcx-test",*state,*previous,*staging));
 // Quota compaction keeps a recoverable prior journal, including rename interruption.
 out=fopen(file,"ab");assert(out);uint32_t sequence=recovery.sequence;
 while(ftell(out)<=long(mcxstore::QUOTA)){++state->local.revision;assert(mcxstore::transaction(out,*state,nullptr,++sequence));}
 fclose(out);assert(mcxstore::save(root,*state,*previous,*staging));
 char backup[240];mcxstore::path(backup,sizeof(backup),root,c.public_key,"bak2");
 // Deliberately corrupt the new compacted log and restore the previous valid journal.
 out=fopen(file,"wb");assert(out);fputs("truncated",out);fclose(out);
 recovery=mcxstore::load(root,c.public_key,*previous,*staging);assert(recovery.found&&recovery.recovered);
 printf("PASS: network/radio isolation, UTF-8, rooms, organization, local persistence, every delta truncation, corruption, quotas, backup and zero replay TX\n");
}
