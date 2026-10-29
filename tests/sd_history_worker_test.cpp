// Execute the real production worker with deterministic FreeRTOS timing and
// an injected sync error. No private radio archive or message text is embedded.
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
static bool sync_error=false;
static int simulated_sync(int fd){if(sync_error){errno=EIO;return -1;}return fsync(fd);}
#define fsync simulated_sync
#include "meshcrossway_sd_history.h"
#undef fsync
#include <cassert>
#include <memory>
struct Done{};
static meshmc::Client *client=nullptr;
static uint32_t failed_at=0;
static int phase=0;
static bool loaded_retry=false;
static void drive(uint32_t){
  using namespace mcxhistory;
  assert(fake_ticks<20000);
  if(phase==0){
    assert(commits==1&&committed==submitted&&count==0);
    sync_error=true;strcpy(client->messages[0].text,"new unsaved message");++client->generation;
    capture(*client,fake_ticks,true);phase=1;return;
  }
  if(phase==1){
    assert(failures==1&&error_step==mcxstore::SYNC&&error_number==EIO);
    failed_at=fake_ticks;phase=2;
  }
  if(phase==2){
    // Every 100 ms supplies a newer draft, without bypassing the 5 s backoff.
    ++preferences().revision;capture(*client,fake_ticks,true);
    if(fake_ticks-failed_at<4800)assert(failures==1);
    if(!loaded_retry&&fake_ticks-failed_at>=1000){
      Job job;job.load=true;job.epoch=2;memcpy(job.radio,client->public_key,32);assert(submit(job));
      loaded_retry=true;
    }
    if(complete){
      assert(load_ok&&!load_unavailable&&loaded&&storage==2);
      assert(!strcmp(loaded->messages[0].text,"new unsaved message"));
      auto restored=std::unique_ptr<meshmc::Client>(new meshmc::Client());
      auto local=std::unique_ptr<mcxchat::Local>(new mcxchat::Local());
      mcxstore::restore(*loaded,*restored,*local,false);assert(restored->qcount==0);
      heap_caps_free(loaded);loaded=nullptr;complete.store(false);
    }
    if(failures==2){assert(fake_ticks-failed_at>=4800);sync_error=false;phase=3;}
  }
  if(phase==3){
    ++preferences().revision;capture(*client,fake_ticks,true);
    if(commits==2){
      // Last capture here may still be queued. The last flushed ticket must
      // already cover all work preceding it, including superseded snapshots.
      assert(committed>1&&error_step==0&&storage==1&&loaded_retry);
      throw Done{};
    }
  }
}
int main(int argc,char **argv){
  assert(argc==2);using namespace mcxhistory;DIR=argv[1];assert(init_workspace());
  client=new meshmc::Client();client->public_key[0]=7;
  auto &m=client->append();strcpy(m.text,"saved message");++client->generation;
  for(int i=0;i<10;++i){++preferences().revision;capture(*client,0,true);}
  assert(count==1&&submitted==10);idle_hook=drive;
  try{worker_main(nullptr);}catch(Done&){}
  assert(phase==3&&failures==2);
  auto out=std::unique_ptr<mcxstore::State>(new mcxstore::State());
  auto staging=std::unique_ptr<mcxstore::State>(new mcxstore::State());
  auto r=mcxstore::load(DIR,client->public_key,*out,*staging);
  assert(r.found&&!r.error&&!strcmp(out->messages[0].text,"new unsaved message"));
  printf("PASS: real worker coalesces snapshots, backs off under continuous traffic, preserves unsaved RAM on reconnect, recovers from sync failure and never enqueues restored TX\n");
}
