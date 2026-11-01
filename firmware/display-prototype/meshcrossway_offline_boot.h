#pragma once
#include "meshcrossway_ble.h"
namespace mcxoffline {
// LVGL lock held, after the SD mount attempt. No controller/host startup,
// transport callback or transmission. Existing workers restore separate stores.
static void begin(){
  const bool mc=meshmc::init(),mt=meshmt::init();
  if(!mc||!mt)printf("[MCX-HISTORY] offline model allocation failed: MC=%d MT=%d\n",mc,mt);
  if(mt)meshble::restore_offline();
}
}
