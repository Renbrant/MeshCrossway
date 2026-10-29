#include "meshcrossway_meshcore.h"
#include <cassert>
static int closed=0;
static void close_transport(){++closed;}
int main(){
  assert(meshmc::init());meshmc::transport_up(173);
  auto &c=*meshmc::client;c.connected=c.initialized=true;c.public_key[0]=7;
  c.payload_limit=173;c.busy=true;c.deadline=60000;
  meshmc::seen_epoch=meshmc::connection_epoch.load();meshmc::close_link=close_transport;
  mcxhistory::restored=mcxhistory::restore_requested=true;
  mcxhistory::bound_epoch=meshmc::seen_epoch;memcpy(mcxhistory::bound_radio,c.public_key,32);
  const uint8_t waiting=131;
  // A burst exceeding the old 16-frame capacity must not fault the session.
  for(int i=0;i<100;++i)meshmc::on_frame(&waiting,1);
  assert(meshmc::mail_count==100&&meshmc::mailbox_peak==100&&!meshmc::overflow);
  meshmc::poll();assert(meshmc::mail_count==100-meshmc::POLL_FRAMES&&!c.fault);
  for(int i=0;i<6;++i)meshmc::poll();
  assert(!meshmc::mail_count&&!c.fault&&!closed&&meshmc::mailbox_overflows==0);
  // The bound is still strict: overflowing it closes the session explicitly.
  for(unsigned i=0;i<meshmc::MAILBOX_CAPACITY+1;++i)meshmc::on_frame(&waiting,1);
  meshmc::poll();assert(c.fault&&closed==1&&meshmc::mailbox_overflows==1);
  meshmc::transport_down();meshmc::poll();assert(!c.connected);
  meshmc::transport_up(173);assert(!meshmc::mail_count&&!meshmc::overflow);
  printf("PASS: production transport drains a 100-frame burst in bounded batches, enforces capacity, and clears the old session on reconnect\n");
}
