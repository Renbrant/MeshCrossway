#pragma once
// MeshCrossway Stage B: cryptographic primitive ONLY, not a persistent store.
// No eFuse reading/writing, SD, NVS, BLE, network or LVGL access.
// AES-256-GCM + SHA-256 HMAC/HKDF, constant memory, no heap allocations here.
// Never store the HMAC source key, derived key or mock key in firmware/SD.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#elif defined(MCX_VAULT_HOST_TEST)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#else
#error "Stage-B crypto may only be built for ESP-IDF or MCX_VAULT_HOST_TEST"
#endif

namespace mcxvault {
constexpr size_t KEY_BYTES = 32;
constexpr size_t SALT_BYTES = 16;
constexpr size_t STORE_ID_BYTES = 16;
constexpr size_t RADIO_ID_BYTES = 32;
constexpr size_t NONCE_BYTES = 12;
constexpr size_t TAG_BYTES = 16;
constexpr size_t MAX_RECORD_BYTES = 512;
constexpr uint8_t FORMAT_VERSION = 1;

enum class Network : uint8_t { MESHCORE = 1, MESHTASTIC = 2 };

struct Context {
  uint8_t store_id[STORE_ID_BYTES]; // random persisted ID, NOT a secret
  uint8_t salt[SALT_BYTES];         // random, persisted, NOT a secret
  uint8_t radio_id[RADIO_ID_BYTES]; // full-length hashed radio identity
  Network network;
};

// One random source-challenge per vault. Must not change for a given store.
// In production, response comes ONLY from validated HMAC_UP slot through a
// future provisioner; stage B uses a host-only fake HMAC response. Never use
// a public MAC address or serial number as source key.
inline size_t challenge(const Context &ctx, uint8_t out[48]) {
  static const uint8_t label[] = {'M','C','X','-','V','A','U','L','T','-','V','1'};
  memcpy(out, label, sizeof(label));
  memcpy(out + sizeof(label), ctx.store_id, STORE_ID_BYTES);
  memcpy(out + sizeof(label) + STORE_ID_BYTES, ctx.salt, SALT_BYTES);
  return sizeof(label) + STORE_ID_BYTES + SALT_BYTES;
}

inline void wipe(void *memory, size_t length) {
  volatile uint8_t *p = static_cast<volatile uint8_t *>(memory);
  while (length--) *p++ = 0;
}

inline bool valid_network(Network n) {
  return n == Network::MESHCORE || n == Network::MESHTASTIC;
}

// HMAC source callback. A future vetted provider may call esp_hmac_calculate
// only after authenticated enrolment. NEVER log secrets/HMAC output.
// This cryptographic module is never called by the UI or boot in Stage B.
typedef bool (*HmacProvider)(const uint8_t *challenge_bytes, size_t len,
                             uint8_t out[KEY_BYTES], void *user);

inline bool hkdf_sha256(const uint8_t *ikm, size_t ikm_len,
                        const uint8_t salt[SALT_BYTES],
                        const uint8_t *info, size_t info_len,
                        uint8_t out[KEY_BYTES]) {
  if (!ikm || !ikm_len || !salt || !info || !out || !info_len) return false;
#if defined(ESP_PLATFORM)
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return md && mbedtls_hkdf(md, salt, SALT_BYTES, ikm, ikm_len,
                            info, info_len, out, KEY_BYTES) == 0;
#else
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
  if (!pctx) return false;
  size_t out_len = KEY_BYTES;
  const bool ok = EVP_PKEY_derive_init(pctx) > 0 &&
      EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) > 0 &&
      EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, SALT_BYTES) > 0 &&
      EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm, ikm_len) > 0 &&
      EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) > 0 &&
      EVP_PKEY_derive(pctx, out, &out_len) > 0 && out_len == KEY_BYTES;
  EVP_PKEY_CTX_free(pctx);
  return ok;
#endif
}

inline bool derive_key(const Context &ctx, HmacProvider provider, void *user,
                       uint8_t out[KEY_BYTES]) {
  if (!out) return false;
  memset(out, 0, KEY_BYTES);
  if (!provider || !valid_network(ctx.network)) return false;
  uint8_t input[48] = {};
  uint8_t upstream[KEY_BYTES] = {};
  const size_t n = challenge(ctx, input);
  bool ok = provider(input, n, upstream, user);
  if (ok) {
    // Domain label, protocol and radio identity bound into derived key.
    uint8_t info[12 + 1 + RADIO_ID_BYTES] = {};
    static const uint8_t label[] = {'M','C','X','-','R','A','D','I','O','-','1','!'};
    memcpy(info, label, sizeof(label));
    info[sizeof(label)] = static_cast<uint8_t>(ctx.network);
    memcpy(info + sizeof(label) + 1, ctx.radio_id, RADIO_ID_BYTES);
    ok = hkdf_sha256(upstream, KEY_BYTES, ctx.salt, info, sizeof(info), out);
    wipe(info, sizeof(info));
  }
  wipe(upstream, sizeof(upstream));
  wipe(input, sizeof(input));
  if (!ok) memset(out, 0, KEY_BYTES);
  return ok;
}

// Mock exists ONLY in host test builds; never in production firmware.
#if defined(MCX_VAULT_HOST_TEST)
inline bool mock_hmac(const uint8_t *data, size_t n, uint8_t out[KEY_BYTES], void *user) {
  if (!data || !out || !user) return false;
  unsigned len = 0;
  return HMAC(EVP_sha256(), user, KEY_BYTES, data, n, out, &len) != nullptr &&
         len == KEY_BYTES;
}
#endif

// Per-record AEAD with caller-authenticated AAD. AAD MUST bind schema version,
// protocol, vault/radio identity, record type and sequence. NONCE MUST BE
// UNIQUE per key even after crashes. Stage C journal will enforce uniqueness.
// Distinct input/output buffers only; no dynamic memory allocated here.
inline bool seal(const uint8_t key[KEY_BYTES], const uint8_t nonce[NONCE_BYTES],
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *plaintext, size_t len,
                 uint8_t *ciphertext, uint8_t tag[TAG_BYTES]) {
  if (!key || !nonce || !tag || !ciphertext || len > MAX_RECORD_BYTES ||
      (len && !plaintext) || (aad_len && !aad) || aad_len > 256 ||
      (len && plaintext == ciphertext)) return false;
  memset(tag, 0, TAG_BYTES);
#if defined(ESP_PLATFORM)
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int status = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (status == 0) status = mbedtls_gcm_crypt_and_tag(
      &ctx, MBEDTLS_GCM_ENCRYPT, len, nonce, NONCE_BYTES,
      aad, aad_len, plaintext, ciphertext, TAG_BYTES, tag);
  mbedtls_gcm_free(&ctx);
  if (status != 0) { wipe(ciphertext, len); wipe(tag, TAG_BYTES); }
  return status == 0;
#else
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return false;
  int count = 0, tail = 0;
  const bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) > 0 &&
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_BYTES, nullptr) > 0 &&
      EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) > 0 &&
      (!aad_len || EVP_EncryptUpdate(ctx, nullptr, &count, aad, (int)aad_len) > 0) &&
      (!len || EVP_EncryptUpdate(ctx, ciphertext, &count, plaintext, (int)len) > 0) &&
      EVP_EncryptFinal_ex(ctx, ciphertext + len, &tail) > 0 &&
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_BYTES, tag) > 0;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) { wipe(ciphertext, len); wipe(tag, TAG_BYTES); }
  return ok;
#endif
}

inline bool open(const uint8_t key[KEY_BYTES], const uint8_t nonce[NONCE_BYTES],
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *ciphertext, size_t len,
                 const uint8_t tag[TAG_BYTES], uint8_t *plaintext) {
  if (!key || !nonce || !tag || !plaintext || len > MAX_RECORD_BYTES ||
      (len && !ciphertext) || (aad_len && !aad) || aad_len > 256 ||
      (len && ciphertext == plaintext)) return false;
  // Never return unauthenticated bytes to caller on any failure.
  memset(plaintext, 0, len);
#if defined(ESP_PLATFORM)
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int status = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (status == 0) status = mbedtls_gcm_auth_decrypt(
      &ctx, len, nonce, NONCE_BYTES, aad, aad_len,
      tag, TAG_BYTES, ciphertext, plaintext);
  mbedtls_gcm_free(&ctx);
  if (status != 0) wipe(plaintext, len);
  return status == 0;
#else
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return false;
  int count = 0, tail = 0;
  const bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) > 0 &&
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_BYTES, nullptr) > 0 &&
      EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) > 0 &&
      (!aad_len || EVP_DecryptUpdate(ctx, nullptr, &count, aad, (int)aad_len) > 0) &&
      (!len || EVP_DecryptUpdate(ctx, plaintext, &count, ciphertext, (int)len) > 0) &&
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_BYTES, const_cast<uint8_t *>(tag)) > 0 &&
      EVP_DecryptFinal_ex(ctx, plaintext + len, &tail) > 0;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) wipe(plaintext, len);
  return ok;
#endif
}
} // namespace mcxvault
