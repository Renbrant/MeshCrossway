#pragma once
#include <stdint.h>
#include <stddef.h>
namespace mcxchat {
// Enumerate stored entries rather than deriving presence from the next counter
// or a physical ring slot. Both live and restored/sparse histories use this order.
template<class Message,size_t N,class Identity>
static unsigned ordered_indices(const Message (&messages)[N],uint16_t *out,unsigned capacity,Identity identity){
  unsigned count=0;
  for(unsigned i=0;i<N;++i){const uint32_t id=identity(messages[i]);if(!id||count==capacity)continue;
    unsigned pos=count++;while(pos&&identity(messages[out[pos-1]])>id){out[pos]=out[pos-1];--pos;}out[pos]=uint16_t(i);
  }
  return count;
}
}
