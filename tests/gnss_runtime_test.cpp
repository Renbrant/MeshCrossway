#include "meshcrossway_gnss.h"
#include <cassert>
#include <cstring>
#include <cstdio>
struct Done {};
static unsigned phase=0;
static const char *gga="$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
static int incoming(void *p,unsigned cap){
  using namespace mcxgnss;
  assert(phase<6);const unsigned step=phase++;
  if(step==0||step==4){assert(strlen(gga)<=cap);memcpy(p,gga,strlen(gga));return int(strlen(gga));}
  auto c=configuration();
  if(step==1){assert(snapshot().valid&&fake_baud_calls==1);c.imperial=1;configure(c);}
  if(step==2){assert(snapshot().valid&&fake_baud_calls==1&&fake_flush_calls==1);c.timeout_ms=10000;configure(c);}
  if(step==3){assert(snapshot().valid&&fake_baud_calls==1&&fake_flush_calls==1);c.baud=38400;configure(c);}
  if(step==5){assert(snapshot().valid&&fake_baud_calls==2&&fake_uart_baud==38400);c.enabled=0;configure(c);}
  return 0;
}
static void delayed(unsigned){if(phase==6&&!mcxgnss::configuration().enabled){assert(!mcxgnss::snapshot().valid);throw Done{};}}
int main(){
  mcxgnss::start();assert(mcxgnss::task&&fake_uart_rx==44&&fake_uart_tx==43);
  fake_uart_read=incoming;fake_delay_hook=delayed;
  try{mcxgnss::worker(nullptr);}catch(Done&){}
  assert(phase==6);puts("PASS: production GPS worker reads NMEA, preserves UART on unit/expiry changes, applies manual baud and disables fixes immediately");
}
