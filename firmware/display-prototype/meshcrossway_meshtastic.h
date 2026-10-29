#pragma once
#include "meshcrossway_meshtastic_store.h"
#include "meshcrossway_sd_history.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <atomic>
#include <new>
namespace meshmt {
  static Client *client=nullptr;
  static constexpr unsigned MAILBOX=32;
  struct Buffers {Frame frames[MAILBOX];
    Frame reading;
    Archive previous;
  };
  struct Disk {Archive image;
    uint8_t radio[7]={};
    int result=0,error=0;
    uint32_t generation=0;
  };
  static Buffers *buffers=nullptr;
  static Disk *disk=nullptr;
  static portMUX_TYPE receive_mux=portMUX_INITIALIZER_UNLOCKED;
  static unsigned head=0,count=0;
  static std::atomic<unsigned> disk_state{0};
  // 0 idle, 1 load, 2 save, 3 loaded, 4 saved
  static std::atomic<bool> link{false},overflow{false};
  static std::atomic<uint32_t> epoch{0};
  static uint32_t seen_epoch=0;
  static std::atomic<int> write_result{0};
  static std::atomic<bool> offline_request{false};
  static uint8_t peer[7]={};
  static uint16_t link_mtu=0;
  static bool restored=false,load_started=false,write_allowed=false,have_previous=false;
  static uint32_t saved_generation=0,last_capture=0;
  static int storage=0;
  static uint64_t saved_hash=0;
  static uint8_t saved_radio[7]={};
  static bool (*send_frame)(const uint8_t *,size_t)=nullptr;
  static bool (*can_write)()=nullptr;
  static void (*pump_read)()=nullptr;
  static void (*close_link)()=nullptr;
  static const char *history_root="/sdcard/mcx-history";
  static void disk_work(){
    const unsigned state=disk_state.load();
    if(!disk||(state!=1&&state!=2))return;
    const char *root=history_root;
    if(state==1){
      disk->result=meshmtstore::load(root,disk->radio,disk->image);
      disk->error=errno;
      // A recovered backup may be the only valid copy. Keep it intact until the
      // new complete .tmp is flushed; subsequent saves run only with this consent.
      if(disk->result>=0){saved_hash=disk->result==1?meshmtstore::checksum(disk->image):0;
        memcpy(saved_radio,disk->radio,7);
      }
      disk_state.store(3);
    }else{
      uint64_t hash=meshmtstore::checksum(disk->image);
      const bool same=!memcmp(saved_radio,disk->image.radio,7)&&hash==saved_hash;
      disk->result=same||meshmtstore::save(root,disk->image);
      disk->error=errno;
      if(disk->result){saved_hash=hash;
        memcpy(saved_radio,disk->image.radio,7);
      }
      disk_state.store(4);
    }
  }
  template<class T> static T *psram(){
    void *memory=heap_caps_malloc(sizeof(T),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return memory?new(memory) T():nullptr;
  }
  static bool init(){
    if(!client)client=psram<Client>();
    if(!buffers)buffers=psram<Buffers>();
    if(!disk)disk=psram<Disk>();
    if(!client||!buffers||!disk)return false;
    mcxhistory::extra_work.store(disk_work);
    return true;
  }
  static void transport_down(){link.store(false);
    write_result.store(0);
  }
  static void transport_up(const uint8_t identity[7],uint16_t mtu){
    portENTER_CRITICAL(&receive_mux);
    head=count=0;
    memcpy(peer,identity,7);
    link_mtu=mtu;
    portEXIT_CRITICAL(&receive_mux);
    overflow.store(false);
    write_result.store(0);
    epoch.fetch_add(1);
    link.store(true);
  }
  static void request_offline(const uint8_t identity[7]){
    portENTER_CRITICAL(&receive_mux);
    memcpy(peer,identity,7);
    portEXIT_CRITICAL(&receive_mux);
    offline_request.store(true);
  }
  static void on_write(bool ok){write_result.store(ok?1:-1);
  }
  static bool on_frame(const uint8_t *p,size_t n){
    if(!link.load()||!buffers||!n||n>MAX_FRAME)return false;
    bool ok=false;
    portENTER_CRITICAL(&receive_mux);
    if(count<MAILBOX){auto &f=buffers->frames[(head+count)%MAILBOX];
      f.len=n;
      memcpy(f.data,p,n);
      ++count;
      ok=true;
    }
    portEXIT_CRITICAL(&receive_mux);
    if(!ok)overflow.store(true);
    return ok;
  }
  static const char *history_status(){
    if(storage==2)return "MT SD error / unsaved RAM";
    if(disk_state.load()==2)return "MT SD saving...";
    if(!restored)return "MT SD restoring...";
    return storage==1?"MT SD saved":"MT SD no saved history";
  }
  static void poll(){
    if(!client||!buffers||!disk)return;
    if(mcxhistory::ble_startup.load()&&!mcxhistory::worker)return;
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    mcxhistory::begin();
    if(!link.load()&&offline_request.exchange(false)&&!client->data.message_count&&disk_state.load()==0){
      memcpy(client->data.radio,peer,7);
      memcpy(disk->radio,peer,7);
      load_started=true;
      disk_state.store(1);
    }
    // The existing low-priority worker owns both protocol stores.
    if(disk_state.load()==3){
      if(disk->result==1)meshmtstore::merge(*client,disk->image);
      if(have_previous&&!memcmp(buffers->previous.radio,client->data.radio,7)){
        // Newer same-radio RAM wins over disk. No pending frame/queue is restored.
        memcpy(client->data.messages,buffers->previous.messages,sizeof(client->data.messages));
        memcpy(client->data.drafts,buffers->previous.drafts,sizeof(client->data.drafts));
        client->data.message_count=buffers->previous.message_count;
        client->data.next_seq=buffers->previous.next_seq;
        for(auto &m:client->data.messages)if(m.seq&&m.outgoing&&(m.delivery==QUEUED||m.delivery==SUBMITTED||m.delivery==RADIO_ACCEPTED))m.delivery=UNCERTAIN;
      }
      have_previous=false;
      restored=true;
      write_allowed=disk->result>=0;
      storage=disk->result<0?2:disk->result==1?1:0;
      client->history_ready=true;
      if(client->connected&&client->initialized&&!client->fault)
        client->status_text(storage==2?"Meshtastic ready / SD recovery failed":"Meshtastic ready / history restored");
      else ++client->generation;
      disk_state.store(0);
    }
    if(disk_state.load()==4){storage=disk->result?1:2;
      if(disk->result)saved_generation=disk->generation;
      last_capture=now;
      disk_state.store(0);
    }
    if(!link.load()){if(client->connected)client->detach();
    }
    else if(seen_epoch!=epoch.load()){
      if(disk_state.load()==1||disk_state.load()==2)return;
      if(client->data.my_num||client->data.message_count){
        // Keep unsaved same-radio RAM across reconnection; do not overwrite it
        // with an older SD image or silently discard it for another radio.
        if(memcmp(client->data.radio,peer,7)&&saved_generation!=client->generation){
          if(storage==2||!write_allowed){
            client->fail("Unsaved Meshtastic history. Restore SD before switching radios.");
            if(close_link)close_link();
            return;
          }
          disk->image=client->data;
          disk->generation=client->generation;
          disk_state.store(2);
          return;
        }
        buffers->previous=client->data;
        have_previous=true;
      }
      client->attach(peer,link_mtu,esp_random(),now);
      seen_epoch=epoch.load();
      restored=load_started=write_allowed=false;
      saved_generation=UINT32_MAX;
      last_capture=now;
      storage=0;
      disk_state.store(0);
    }
    if(overflow.exchange(false)){client->fail("Meshtastic receive overflow. Reconnect to synchronize.");
      if(close_link)close_link();
      return;
    }
    const int written=write_result.exchange(0);
    if(written)client->write_done(written==1);
    for(unsigned k=0;k<16;++k){
      if(client->initialized&&!restored)break;
      // Restore before radio backlog.
      Frame f;
      bool got=false;
      portENTER_CRITICAL(&receive_mux);
      if(count){f=buffers->frames[head];
        head=(head+1)%MAILBOX;
        --count;
        got=true;
      }
      portEXIT_CRITICAL(&receive_mux);
      if(!got)break;
      if(!client->receive(f.data,f.len,now)){if(!client->fault)client->fail("Invalid Meshtastic frame. Check firmware / reconnect.");
        break;
      }
    }
    if(client->initialized&&!load_started&&disk_state.load()==0){
      memcpy(disk->radio,client->data.radio,7);
      load_started=true;
      disk_state.store(1);
    }
    if(restored&&write_allowed&&client->generation!=saved_generation&&disk_state.load()==0&&(uint32_t)(now-last_capture)>3000){
      disk->image=client->data;
      disk->generation=client->generation;
      last_capture=now;
      disk_state.store(2);
    }
    if(client->fault){if(close_link&&link.load())close_link();
      return;
    }
    if(!link.load())return;
    if(can_write&&can_write()){
      Frame frame;
      if(client->next(now,frame)){
        if(!send_frame||!send_frame(frame.data,frame.len))client->write_done(false);
      }else if((!client->initialized||restored)&&pump_read)pump_read();
    }
  }
} // namespace meshmt
