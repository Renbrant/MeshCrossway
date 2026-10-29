#include "meshcrossway_meshtastic.h"
#include <cassert>
using namespace meshmt;
static bool available=true;
static unsigned writes=0,reads=0,closes=0;
static bool send(const uint8_t *,size_t){++writes;return true;}
static bool writable(){return available;}
static void read_more(){++reads;}
static void close_more(){++closes;transport_down();}
int main(int argc,char **argv){
  assert(argc==2);history_root=argv[1];assert(init());assert(sizeof(Buffers)<100000);
  send_frame=send;can_write=writable;pump_read=read_more;close_link=close_more;
  mcxhistory::ble_startup.store(true);poll();assert(!mcxhistory::worker);
  mcxhistory::ble_startup.store(false);
  uint8_t identity[7]={0,9,8,7,6,5,4};transport_up(identity,517);poll();
  assert(client->connected&&writes==1);on_write(true);poll();
  assert(!client->writing&&reads>0);
  // Saturation is explicit and does not overwrite earlier frames.
  uint8_t future[]={0xa0,0x06,1};
  for(unsigned i=0;i<MAILBOX;++i)assert(on_frame(future,sizeof(future)));
  assert(!on_frame(future,sizeof(future)));poll();assert(client->fault&&closes==1);
  poll();assert(!client->connected);
  transport_up(identity,517);poll();assert(!client->fault&&count==0);on_write(true);
  // Simulate completed configuration; SD restore gates the read backlog.
  client->initialized=true;client->data.my_num=123;client->have_lora=true;
  poll();assert(disk_state.load()==1&&!client->history_ready);
  disk_work();assert(disk_state.load()==3);poll();assert(client->history_ready&&restored);
  auto &m=client->append();m.id=900;m.from=123;m.to=BROADCAST;m.outgoing=true;m.delivery=QUEUED;strcpy(m.text,"pending before disconnect");
  ++client->generation;fake_us=5000000;poll();assert(disk_state.load()==2);
  disk_work();assert(disk_state.load()==4);poll();assert(storage==1);
  transport_down();poll();assert(client->message(900)->delivery==UNCERTAIN);
  transport_up(identity,517);poll();on_write(true);client->initialized=true;client->data.my_num=123;
  poll();disk_work();poll();assert(client->message(900)&&client->message(900)->delivery==UNCERTAIN);
  assert(!client->pending_ready);assert(!strcmp(client->message(900)->text,"pending before disconnect"));
  puts("PASS: production Meshtastic mailbox, overflow, reconnect, SD worker handoff, backlog gating, RAM precedence and no automatic retransmission");
}
