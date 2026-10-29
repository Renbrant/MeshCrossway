#define MCX_VAULT_HOST_TEST 1
#include "../firmware/display-prototype/meshcrossway_storage_journal.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

struct FakeEntropy { uint8_t ctr; bool fail; };
static bool random_fake(uint8_t *p, size_t n, void *state) {
  FakeEntropy *f = static_cast<FakeEntropy *>(state);
  if (f->fail) return false;
  for (size_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(f->ctr + i * 3);
  ++f->ctr;
  return true;
}
static bool random_fixed(uint8_t *p, size_t n, void *) {
  memset(p, 0x5a, n); return true;
}
struct Messages {
  int count;
  bool seen[6];
  Messages() : count(0), seen{} {}
};
static bool receive(uint8_t type, const uint8_t *bytes, size_t len,
                    uint64_t seq, void *user) {
  Messages *m=static_cast<Messages *>(user);
  assert(seq > 0 && seq < 65536);
  assert(type == 1 || type == 2);
  assert(len > 0 && len < 100);
  if (type == 1) {
    assert(!memcmp(bytes,"Ola MeshCore",len) || !memcmp(bytes,"MeshCore B",len));
    m->seen[0] = true;
  } else {
    assert(!memcmp(bytes,"hello Meshtastic",len));
    m->seen[1] = true;
  }
  ++m->count;
  return true;
}
static bool count_only(uint8_t, const uint8_t *, size_t, uint64_t, void *user) {
  ++*static_cast<int *>(user);
  return true;
}
static void remove_directory(const char *dir) {
  DIR *d=opendir(dir);
  assert(d);
  struct dirent *e;
  char path[256];
  while ((e=readdir(d))) {
    if (strcmp(e->d_name,".") == 0 || strcmp(e->d_name,"..") == 0) continue;
    snprintf(path,sizeof path,"%s/%s",dir,e->d_name);
    assert(unlink(path) == 0);
  }
  closedir(d);
  assert(rmdir(dir) == 0);
}
static void alter_byte(const char *path, long offset) {
  FILE *f=fopen(path,"rb+");assert(f);
  assert(fseek(f,offset,SEEK_SET) == 0);
  const int v=fgetc(f);assert(v>=0);
  assert(fseek(f,offset,SEEK_SET) == 0);
  assert(fputc(v ^ 0x01,f)!=EOF);
  assert(fflush(f)==0);assert(fsync(fileno(f))==0);assert(fclose(f)==0);
}
int main() {
  char dir[]="/tmp/mcx-vault-journal-XXXXXX";
  assert(mkdtemp(dir));
  mcxvault::Context ctx = {};
  ctx.network=mcxvault::Network::MESHCORE;
  for(unsigned i=0;i<16;++i){ctx.salt[i]=uint8_t(i+9);ctx.store_id[i]=uint8_t(i+17);}
  for(unsigned i=0;i<32;++i)ctx.radio_id[i]=uint8_t(i+25);
  uint8_t master[32];
  for(unsigned i=0;i<32;++i)master[i]=uint8_t(i+33);
  uint8_t wrong[32];memcpy(wrong,master,32);wrong[5]^=1;
  FakeEntropy rng={1,false};
  mcxjournal::Writer a;
  assert(mcxjournal::begin(a,dir,master,ctx,random_fake,&rng)==mcxjournal::Result::OK);
  const char msg[]="Ola MeshCore";
  const char next[]="MeshCore B";
  assert(mcxjournal::append(a,1,(const uint8_t *)msg,strlen(msg))==mcxjournal::Result::OK);
  assert(mcxjournal::append(a,1,(const uint8_t *)next,strlen(next))==mcxjournal::Result::OK);
  char filename[mcxjournal::MAX_PATH_BYTES];
  snprintf(filename,sizeof filename,"%s",a.path);
  assert(a.sequence==2);
  uint8_t too_long[mcxvault::MAX_RECORD_BYTES+1] = {};
  assert(mcxjournal::append(a,1,too_long,sizeof too_long)==mcxjournal::Result::INVALID);
  assert(a.sequence==2); // rejected before I/O
  mcxjournal::close_writer(a);
  assert(!a.active && mcxjournal::nonzero(a.key,32)==false);
  FILE *raw=fopen(filename,"rb");assert(raw);
  assert(fseek(raw,0,SEEK_END)==0);long file_len=ftell(raw);assert(file_len>100);
  assert(fseek(raw,0,SEEK_SET)==0);
  uint8_t raw_bytes[1024]={};
  assert((size_t)file_len<sizeof raw_bytes);
  assert(fread(raw_bytes,1,(size_t)file_len,raw)==(size_t)file_len);
  assert(fclose(raw)==0);
  for(size_t k=0;k+strlen(msg)<(size_t)file_len;++k) {
    assert(memcmp(raw_bytes+k,msg,strlen(msg))!=0); // no plaintext record
  }
  Messages got;
  uint32_t count=0;
  assert(mcxjournal::replay_segment(filename,master,ctx,receive,&got,count)==mcxjournal::Result::OK);
  assert(count==2&&got.count==2);
  count=0;
  assert(mcxjournal::replay_segment(filename,wrong,ctx,count_only,&got,count)==mcxjournal::Result::AUTH_FAILED);
  ctx.network=mcxvault::Network::MESHTASTIC;
  assert(mcxjournal::replay_segment(filename,master,ctx,count_only,&got,count)==mcxjournal::Result::AUTH_FAILED);
  ctx.network=mcxvault::Network::MESHCORE;
  // A power cut mid-tail leaves previous authenticated records readable.
  FILE *tail=fopen(filename,"ab");assert(tail);
  const uint8_t partial[]={'M','R',1,0};
  assert(fwrite(partial,1,sizeof partial,tail)==sizeof partial);
  assert(fflush(tail)==0);assert(fclose(tail)==0);
  count=0;
  assert(mcxjournal::replay_segment(filename,master,ctx,count_only,&count,count)==mcxjournal::Result::TAIL_TRUNCATED);
  assert(count==2);
  mcxjournal::Writer b;
  assert(mcxjournal::begin(b,dir,master,ctx,random_fake,&rng)==mcxjournal::Result::OK);
  assert(strcmp(filename,b.path)!=0); // fresh per-boot segment; never re-open broken tail
  assert(mcxjournal::append(b,1,(const uint8_t*)next,strlen(next))==mcxjournal::Result::OK);
  mcxjournal::close_writer(b);
  mcxjournal::Stats stats={};
  int all=0;
  assert(mcxjournal::replay_directory(dir,master,ctx,count_only,&all,stats)==mcxjournal::Result::OK);
  assert(stats.recovered==3 && all==3 && stats.truncated_segments==1 && stats.checked_segments==2);
  // Tampering an existing authenticated record must fail closed.
  alter_byte(filename,mcxjournal::HEADER_BYTES+mcxjournal::RECORD_HEADER_BYTES+2);
  count=0;
  assert(mcxjournal::replay_segment(filename,master,ctx,count_only,&count,count)==mcxjournal::Result::AUTH_FAILED);
  assert(count==0);
  // Forged header, wrong radio and wrong store must never decrypt.
  alter_byte(filename,10);
  count=0;
  assert(mcxjournal::replay_segment(filename,master,ctx,count_only,&count,count)==mcxjournal::Result::AUTH_FAILED);
  mcxjournal::Writer c;
  rng.fail=true;
  assert(mcxjournal::begin(c,dir,master,ctx,random_fake,&rng)==mcxjournal::Result::NO_ENTROPY);
  rng.fail=false;
  uint8_t zero[32]={};
  assert(mcxjournal::begin(c,dir,zero,ctx,random_fake,&rng)==mcxjournal::Result::NO_KEY);
  assert(mcxjournal::begin(c,"../not-absolute",master,ctx,random_fake,&rng)==mcxjournal::Result::INVALID);
  assert(mcxjournal::begin(c,"/tmp/../invalid",master,ctx,random_fake,&rng)==mcxjournal::Result::INVALID);
  // Deterministic collision: existing filename must never be opened for append.
  assert(mcxjournal::begin(c,dir,master,ctx,random_fixed,nullptr)==mcxjournal::Result::OK);
  const char *x=c.path;
  assert(mcxjournal::append(c,1,(const uint8_t*)next,strlen(next))==mcxjournal::Result::OK);
  mcxjournal::close_writer(c);
  assert(mcxjournal::begin(c,dir,master,ctx,random_fixed,nullptr)==mcxjournal::Result::NO_ENTROPY);
  assert(!c.active);
  (void)x;
  remove_directory(dir);
  mcxvault::wipe(master,sizeof master);
  std::puts("PASS: append-only authenticated journal, power-loss recovery, tamper, key isolation, collisions");
}
