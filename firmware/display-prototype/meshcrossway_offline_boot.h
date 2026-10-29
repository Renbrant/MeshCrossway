#pragma once
#include "meshcrossway_ble.h"
namespace mcxoffline {
// LVGL lock held, before its protocol timers start. No controller/host startup,
// transport callback or transmission. Existing workers restore separate stores.
static void begin(){
  const bool mc=meshmc::init(),mt=meshmt::init();
  if(!mc||!mt)printf("[MCX-HISTORY] offline model allocation failed: MC=%d MT=%d\n",mc,mt);
  if(mt&&meshble::read_remembered()){
    meshble::RememberedPeer peer;
    portENTER_CRITICAL(&meshble::mux);peer=meshble::remembered;portEXIT_CRITICAL(&meshble::mux);
    if(peer.radio==meshble::MESHTASTIC){uint8_t id[7];id[0]=peer.addr_type&1;
      memcpy(id+1,peer.addr,6);meshmt::request_offline(id);
    }
  }
}
}
