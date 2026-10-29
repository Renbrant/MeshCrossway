#define MCX_VAULT_HOST_TEST 1
#include "../firmware/display-prototype/meshcrossway_storage_crypto.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdint.h>

static bool is_zero(const uint8_t *p,size_t n) {
  for(size_t i=0;i<n;++i) if(p[i]) return false;
  return true;
}
static void hkdf_check() {
  const uint8_t ikm[22] = {0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
                           0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b};
  uint8_t salt[16] = {};
  for(int i=0;i<13;++i) salt[i]=uint8_t(i);
  const uint8_t info[] = {0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9};
  const uint8_t expected[32] = {0x3c,0xb2,0x5f,0x25,0xfa,0xac,0xd5,0x7a,
    0x90,0x43,0x4f,0x64,0xd0,0x36,0x2f,0x2a,0x2d,0x2d,0x0a,0x90,0xcf,0x1a,
    0x5a,0x4c,0x5d,0xb0,0x2d,0x56,0xec,0xc4,0xc5,0xbf};
  // RFC5869 case 1: zero-padding 13-byte salt to 16 changes no HMAC input.
  uint8_t a[32],b[32];
  assert(mcxvault::hkdf_sha256(ikm,sizeof ikm,salt,info,sizeof info,a));
  assert(mcxvault::hkdf_sha256(ikm,sizeof ikm,salt,info,sizeof info,b));
  assert(!memcmp(a,b,32) && !memcmp(a,expected,32));
  salt[13]=1;
  assert(mcxvault::hkdf_sha256(ikm,sizeof ikm,salt,info,sizeof info,b));
  assert(memcmp(a,b,32));
}
static void derive_separation() {
  uint8_t mock_secret[32],key_mc[32],key_mt[32],key_radio2[32],key_store2[32];
  for(int i=0;i<32;++i) mock_secret[i]=(uint8_t)(0xa0+i);
  mcxvault::Context a={};
  for(int i=0;i<16;++i){a.salt[i]=(uint8_t)(i+1);a.store_id[i]=(uint8_t)(i+31);}
  for(int i=0;i<32;++i)a.radio_id[i]=(uint8_t)(i+48);
  a.network=mcxvault::Network::MESHCORE;
  assert(mcxvault::derive_key(a,mcxvault::mock_hmac,mock_secret,key_mc));
  a.network=mcxvault::Network::MESHTASTIC;
  assert(mcxvault::derive_key(a,mcxvault::mock_hmac,mock_secret,key_mt));
  assert(memcmp(key_mc,key_mt,32));
  a.network=mcxvault::Network::MESHCORE;
  a.radio_id[5]^=0x11;
  assert(mcxvault::derive_key(a,mcxvault::mock_hmac,mock_secret,key_radio2));
  assert(memcmp(key_mc,key_radio2,32));
  a.radio_id[5]^=0x11;a.store_id[5]^=0x11;
  assert(mcxvault::derive_key(a,mcxvault::mock_hmac,mock_secret,key_store2));
  assert(memcmp(key_mc,key_store2,32));
  a.network=static_cast<mcxvault::Network>(5);
  uint8_t failed[32];memset(failed,0xae,32);
  assert(!mcxvault::derive_key(a,mcxvault::mock_hmac,mock_secret,failed));
  assert(is_zero(failed,32));
  mcxvault::wipe(mock_secret,32);assert(is_zero(mock_secret,32));
}
static void gcm_known_vector() {
  const uint8_t key[32]={},nonce[12]={},pt[16]={};
  const uint8_t expect_ct[16]={0xce,0xa7,0x40,0x3d,0x4d,0x60,0x6b,0x6e,0x07,0x4e,0xc5,0xd3,0xba,0xf3,0x9d,0x18};
  const uint8_t expect_tag[16]={0xd0,0xd1,0xc8,0xa7,0x99,0x99,0x6b,0xf0,0x26,0x5b,0x98,0xb5,0xd4,0x8a,0xb9,0x19};
  uint8_t ct[16],tag[16],dec[16];
  assert(mcxvault::seal(key,nonce,nullptr,0,pt,16,ct,tag));
  assert(!memcmp(ct,expect_ct,16));assert(!memcmp(tag,expect_tag,16));
  assert(mcxvault::open(key,nonce,nullptr,0,ct,16,tag,dec));
  assert(!memcmp(dec,pt,16));
}
static void gcm_tamper_and_bounds() {
  uint8_t key[32],nonce[12],aad[31],pt[160],ct[160],tag[16],dec[160];
  memset(key,0x25,sizeof key);memset(nonce,0x66,sizeof nonce);
  memset(aad,0x19,sizeof aad);memset(pt,0x7c,sizeof pt);
  assert(mcxvault::seal(key,nonce,aad,sizeof aad,pt,sizeof pt,ct,tag));
  assert(mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  assert(!memcmp(pt,dec,sizeof pt));
  ct[3]^=1;
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  assert(is_zero(dec,sizeof dec));ct[3]^=1;
  tag[6]^=1;
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  assert(is_zero(dec,sizeof dec));tag[6]^=1;
  aad[0]^=1;
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  aad[0]^=1;nonce[1]^=1;
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  nonce[1]^=1;key[1]^=1;
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
  key[1]^=1;
  assert(!mcxvault::seal(key,nonce,aad,sizeof aad,pt,513,ct,tag));
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,513,tag,dec));
  assert(!mcxvault::seal(key,nonce,aad,sizeof aad,pt,160,pt,tag));
  assert(!mcxvault::open(key,nonce,aad,sizeof aad,ct,160,tag,ct));
  assert(!mcxvault::seal(key,nonce,aad,sizeof aad,pt,160,ct,nullptr));
  assert(!mcxvault::seal(key,nonce,nullptr,10,pt,160,ct,tag));
  assert(!mcxvault::open(key,nonce,nullptr,10,ct,160,tag,dec));
}
static void stress_and_measure() {
  uint8_t key[32]={},nonce[12]={},aad[48]={},pt[160]={},ct[160],tag[16],dec[160];
  constexpr unsigned loops=2000;
  auto t0=std::chrono::steady_clock::now();
  for(unsigned n=0;n<loops;++n){
    nonce[8]=static_cast<uint8_t>(n>>24);nonce[9]=static_cast<uint8_t>(n>>16);
    nonce[10]=static_cast<uint8_t>(n>>8);nonce[11]=static_cast<uint8_t>(n);
    assert(mcxvault::seal(key,nonce,aad,sizeof aad,pt,sizeof pt,ct,tag));
    assert(mcxvault::open(key,nonce,aad,sizeof aad,ct,sizeof ct,tag,dec));
    assert(!memcmp(dec,pt,sizeof pt));
  }
  auto dt=std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now()-t0).count();
  std::printf("PASS: AES-256-GCM 2000 host encrypt+decrypt pairs of 160 B in %lld us (HOST ONLY)\n",(long long)dt);
}
int main(){
  hkdf_check();derive_separation();gcm_known_vector();gcm_tamper_and_bounds();stress_and_measure();
  std::puts("PASS: v1 crypto vectors, network separation, tamper, bounds and wipe");
}
