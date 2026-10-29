#pragma once
// MeshCrossway Stage C: encrypted append-only journal (NOT connected to chat).
// VFS/FatFs/POSIX file API; one caller-owned storage worker only, never LVGL/BLE.
// No eFuse programming, NVS, network, automatic plaintext fallback or key files.
// Does not create storage when device-bound authenticated key is unavailable.
#include "meshcrossway_storage_crypto.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

namespace mcxjournal {

enum class Result : uint8_t {
  OK, INVALID, NO_KEY, NO_ENTROPY, NO_DIRECTORY, IO_ERROR,
  AUTH_FAILED, TAIL_TRUNCATED, MALFORMED, CALLBACK_STOP
};

constexpr size_t HEADER_BYTES = 41;
constexpr size_t RECORD_HEADER_BYTES = 13;
constexpr size_t SEGMENT_ID_BYTES = 16;
constexpr size_t MAX_PATH_BYTES = 256;
constexpr uint64_t MAX_PER_SEGMENT = 65536;

struct Stats {
  uint32_t recovered;
  uint32_t truncated_segments;
  uint32_t checked_segments;
};

typedef bool (*RandomProvider)(uint8_t *bytes, size_t len, void *user);
typedef bool (*RecordCallback)(uint8_t type, const uint8_t *cleartext,
                                size_t len, uint64_t segment_seq, void *user);

struct Writer {
  int fd;
  bool active;
  uint64_t sequence;
  uint8_t key[mcxvault::KEY_BYTES];
  uint8_t salt[SEGMENT_ID_BYTES];
  mcxvault::Context context;
  char path[MAX_PATH_BYTES];
  Writer() : fd(-1), active(false), sequence(0), key{}, salt{}, context{}, path{} {}
  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
};

inline bool nonzero(const uint8_t *b, size_t n) {
  if (!b) return false;
  uint8_t value = 0;
  for (size_t i = 0; i < n; ++i) value |= b[i];
  return value != 0;
}
inline void store64(uint8_t *dst, uint64_t value) {
  for (int i = 7; i >= 0; --i) { dst[i] = uint8_t(value); value >>= 8; }
}
inline uint64_t load64(const uint8_t *src) {
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i) v = (v << 8) | src[i];
  return v;
}
inline void make_nonce(const uint8_t salt[SEGMENT_ID_BYTES], uint64_t sequence,
                       uint8_t nonce[mcxvault::NONCE_BYTES]) {
  memcpy(nonce, salt, 4);
  store64(nonce + 4, sequence);
}
inline void make_context_aad(const mcxvault::Context &ctx, uint8_t out[49]) {
  memcpy(out, ctx.store_id, 16);
  out[16] = static_cast<uint8_t>(ctx.network);
  memcpy(out + 17, ctx.radio_id, 32);
}
inline bool derive_segment_key(const uint8_t master[32],
                               const uint8_t salt[SEGMENT_ID_BYTES],
                               const mcxvault::Context &ctx,
                               uint8_t out[32]) {
  if (!nonzero(master, 32) || !mcxvault::valid_network(ctx.network)) return false;
  uint8_t info[65] = {};
  static const uint8_t label[] = {'M','C','X','-','S','E','G','M','E','N','T','-','V','1','!'};
  memcpy(info, label, sizeof(label));
  make_context_aad(ctx, info + sizeof(label));
  const bool ok = mcxvault::hkdf_sha256(master, 32, salt, info, sizeof(info), out);
  mcxvault::wipe(info, sizeof(info));
  return ok;
}
inline bool valid_directory(const char *directory) {
  if (!directory || directory[0] != '/') return false;
  const size_t len = strnlen(directory, MAX_PATH_BYTES);
  if (len < 2 || len > MAX_PATH_BYTES - 40 || directory[len - 1] == '/') return false;
  // Directory comes from trusted app configuration, never radio text. Reject
  // traversal and control characters even if a caller accidentally passes them.
  for (size_t i = 0; i < len; ++i) {
    if (directory[i] < 32 || directory[i] == '\\') return false;
    if (directory[i] == '.' && (i == 0 || directory[i - 1] == '/') &&
        (directory[i + 1] == '.' || directory[i + 1] == '/')) return false;
  }
  return true;
}
inline void hex_id(const uint8_t salt[SEGMENT_ID_BYTES], char out[33]) {
  static const char hexdigit[] = "0123456789abcdef";
  for (unsigned i = 0; i < SEGMENT_ID_BYTES; ++i) {
    out[2*i] = hexdigit[salt[i] >> 4];
    out[2*i+1] = hexdigit[salt[i] & 15];
  }
  out[32] = '\0';
}
inline bool segment_name(const char *name) {
  if (!name || strlen(name) != 36 || strcmp(name + 32, ".mcx") != 0) return false;
  for (unsigned i = 0; i < 32; ++i)
    if (!((name[i] >= '0' && name[i] <= '9') ||
          (name[i] >= 'a' && name[i] <= 'f'))) return false;
  return true;
}
inline bool write_all(int fd, const uint8_t *data, size_t len) {
  while (len) {
    const ssize_t n = write(fd, data, len);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    data += (size_t)n;
    len -= (size_t)n;
  }
  return true;
}
enum class ReadResult { FULL, END, SHORT, ERROR };
inline ReadResult read_full(int fd, uint8_t *data, size_t len) {
  const size_t original = len;
  while (len) {
    const ssize_t n = read(fd, data, len);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) return ReadResult::ERROR;
    if (n == 0) return len == original ? ReadResult::END : ReadResult::SHORT;
    data += (size_t)n;
    len -= (size_t)n;
  }
  return ReadResult::FULL;
}
inline bool authenticated_header(const uint8_t master[32],
                                 const mcxvault::Context &ctx,
                                 const uint8_t header[HEADER_BYTES],
                                 uint8_t out_key[32]) {
  static const uint8_t magic[8] = {'M','C','X','J','N','L','0','1'};
  if (memcmp(header, magic, 8) != 0 ||
      header[8] != mcxvault::FORMAT_VERSION) return false;
  const uint8_t *salt = header + 9;
  if (!derive_segment_key(master, salt, ctx, out_key)) return false;
  uint8_t aad[25 + 49] = {};
  memcpy(aad, header, 25);
  make_context_aad(ctx, aad + 25);
  uint8_t nonce[12], scratch[1] = {};
  make_nonce(salt, 0, nonce);
  const bool ok = mcxvault::open(out_key, nonce, aad, sizeof(aad), scratch, 0,
                                 header + 25, scratch);
  mcxvault::wipe(aad, sizeof(aad));
  return ok;
}
inline void close_writer(Writer &w) {
  if (w.fd >= 0) { (void)close(w.fd); w.fd = -1; }
  w.active = false;
  w.sequence = 0;
  mcxvault::wipe(w.key, sizeof(w.key));
  mcxvault::wipe(w.salt, sizeof(w.salt));
  mcxvault::wipe(&w.context, sizeof(w.context));
  memset(w.path, 0, sizeof(w.path));
}
inline Result begin(Writer &w, const char *directory,
                    const uint8_t master[32], const mcxvault::Context &ctx,
                    RandomProvider random_bytes, void *user) {
  if (w.active) return Result::INVALID;
  close_writer(w);
  if (!valid_directory(directory) || !mcxvault::valid_network(ctx.network))
    return Result::INVALID;
  if (!nonzero(master, 32)) return Result::NO_KEY;
  if (!random_bytes) return Result::NO_ENTROPY;
  struct stat st = {};
  if (stat(directory, &st) != 0 || !S_ISDIR(st.st_mode))
    return Result::NO_DIRECTORY;
  // A new never-reopened file and random per-segment key make record nonces
  // unique even if a previous boot was interrupted mid-write.
  for (unsigned attempt = 0; attempt < 4; ++attempt) {
    if (!random_bytes(w.salt, SEGMENT_ID_BYTES, user)) {
      close_writer(w);
      return Result::NO_ENTROPY;
    }
    char name[33]; hex_id(w.salt, name);
    const int written = snprintf(w.path, sizeof(w.path), "%s/%s.mcx", directory, name);
    if (written < 0 || (size_t)written >= sizeof(w.path)) {
      close_writer(w); return Result::INVALID;
    }
    w.fd = ::open(w.path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (w.fd >= 0) break;
    if (errno != EEXIST) { close_writer(w); return Result::IO_ERROR; }
  }
  if (w.fd < 0) { close_writer(w); return Result::NO_ENTROPY; }
  w.context = ctx;
  if (!derive_segment_key(master, w.salt, ctx, w.key)) {
    (void)unlink(w.path); close_writer(w); return Result::NO_KEY;
  }
  uint8_t header[HEADER_BYTES] = {};
  static const uint8_t magic[8] = {'M','C','X','J','N','L','0','1'};
  memcpy(header, magic, 8);
  header[8] = mcxvault::FORMAT_VERSION;
  memcpy(header + 9, w.salt, 16);
  uint8_t aad[74] = {};
  memcpy(aad, header, 25);
  make_context_aad(ctx, aad + 25);
  uint8_t nonce[12], empty[1] = {};
  make_nonce(w.salt, 0, nonce);
  const bool sealed = mcxvault::seal(w.key, nonce, aad, sizeof(aad),
                                     nullptr, 0, empty, header + 25);
  mcxvault::wipe(aad, sizeof(aad));
  if (!sealed || !write_all(w.fd, header, sizeof(header)) || fsync(w.fd) != 0) {
    (void)unlink(w.path); close_writer(w); return Result::IO_ERROR;
  }
  w.active = true;
  w.sequence = 0;
  return Result::OK;
}
inline Result append(Writer &w, uint8_t record_type,
                     const uint8_t *plaintext, size_t length) {
  if (!w.active || w.fd < 0) return Result::INVALID;
  if (!record_type || record_type > 64 ||
      length > mcxvault::MAX_RECORD_BYTES || (length && !plaintext) ||
      w.sequence >= MAX_PER_SEGMENT) return Result::INVALID;
  uint8_t rec[RECORD_HEADER_BYTES] = {'M','R',record_type};
  const uint64_t sequence = w.sequence + 1;
  store64(rec + 3, sequence);
  rec[11] = uint8_t(length >> 8);
  rec[12] = uint8_t(length);
  uint8_t aad[25 + 49 + RECORD_HEADER_BYTES] = {};
  static const uint8_t magic[8] = {'M','C','X','J','N','L','0','1'};
  memcpy(aad, magic, 8); aad[8] = mcxvault::FORMAT_VERSION;
  memcpy(aad + 9, w.salt, 16);
  make_context_aad(w.context, aad + 25);
  memcpy(aad + 74, rec, sizeof(rec));
  uint8_t nonce[12], ciphertext[mcxvault::MAX_RECORD_BYTES] = {}, tag[16] = {};
  make_nonce(w.salt, sequence, nonce);
  uint8_t empty[1] = {};
  const bool ok = mcxvault::seal(w.key, nonce, aad, sizeof(aad),
                                 plaintext, length, length ? ciphertext : empty, tag);
  mcxvault::wipe(aad, sizeof(aad));
  if (!ok || !write_all(w.fd, rec, sizeof(rec)) ||
      (length && !write_all(w.fd, ciphertext, length)) ||
      !write_all(w.fd, tag, sizeof(tag)) || fsync(w.fd) != 0) {
    mcxvault::wipe(ciphertext, sizeof(ciphertext));
    close_writer(w); // never append to an uncertain nonce sequence again
    return Result::IO_ERROR;
  }
  mcxvault::wipe(ciphertext, sizeof(ciphertext));
  w.sequence = sequence;
  return Result::OK;
}
inline Result replay_segment(const char *path, const uint8_t master[32],
                             const mcxvault::Context &ctx,
                             RecordCallback callback, void *user,
                             uint32_t &recovered) {
  if (!path || !callback || !nonzero(master, 32) ||
      !mcxvault::valid_network(ctx.network)) return Result::INVALID;
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) return Result::IO_ERROR;
  uint8_t header[HEADER_BYTES] = {}, key[32] = {};
  ReadResult rd = read_full(fd, header, sizeof(header));
  if (rd != ReadResult::FULL) {
    (void)close(fd);
    return rd == ReadResult::ERROR ? Result::IO_ERROR : Result::TAIL_TRUNCATED;
  }
  if (!authenticated_header(master, ctx, header, key)) {
    mcxvault::wipe(key, sizeof(key)); (void)close(fd); return Result::AUTH_FAILED;
  }
  Result result = Result::OK;
  uint64_t next_seq = 1;
  for (;;) {
    uint8_t rec[RECORD_HEADER_BYTES] = {};
    rd = read_full(fd, rec, sizeof(rec));
    if (rd == ReadResult::END) break;
    if (rd != ReadResult::FULL) {
      result = rd == ReadResult::ERROR ? Result::IO_ERROR : Result::TAIL_TRUNCATED;
      break;
    }
    const size_t length = (size_t(rec[11]) << 8) | rec[12];
    const uint64_t seq = load64(rec + 3);
    if (rec[0] != 'M' || rec[1] != 'R' || !rec[2] || rec[2] > 64 ||
        seq != next_seq || seq > MAX_PER_SEGMENT ||
        length > mcxvault::MAX_RECORD_BYTES) {
      result = Result::MALFORMED; break;
    }
    uint8_t ciphertext[mcxvault::MAX_RECORD_BYTES] = {};
    uint8_t plaintext[mcxvault::MAX_RECORD_BYTES] = {};
    uint8_t tag[16] = {};
    rd = length ? read_full(fd, ciphertext, length) : ReadResult::FULL;
    if (rd == ReadResult::FULL) rd = read_full(fd, tag, sizeof(tag));
    if (rd != ReadResult::FULL) {
      result = rd == ReadResult::ERROR ? Result::IO_ERROR : Result::TAIL_TRUNCATED;
      break;
    }
    uint8_t aad[25 + 49 + RECORD_HEADER_BYTES] = {};
    memcpy(aad, header, 25);
    make_context_aad(ctx, aad + 25);
    memcpy(aad + 74, rec, sizeof(rec));
    uint8_t nonce[12], empty[1] = {};
    make_nonce(header + 9, seq, nonce);
    const bool valid = mcxvault::open(key, nonce, aad, sizeof(aad),
                                      length ? ciphertext : empty, length, tag,
                                      length ? plaintext : empty);
    mcxvault::wipe(aad, sizeof(aad));
    mcxvault::wipe(ciphertext, sizeof(ciphertext));
    if (!valid) {
      mcxvault::wipe(plaintext, sizeof(plaintext));
      result = Result::AUTH_FAILED; break;
    }
    const bool keep_going = callback(rec[2], plaintext, length, seq, user);
    mcxvault::wipe(plaintext, sizeof(plaintext));
    if (!keep_going) { result = Result::CALLBACK_STOP; break; }
    ++recovered;
    ++next_seq;
  }
  mcxvault::wipe(key, sizeof(key));
  (void)close(fd);
  return result;
}
inline Result replay_directory(const char *directory, const uint8_t master[32],
                               const mcxvault::Context &ctx,
                               RecordCallback callback, void *user, Stats &stats) {
  stats = {};
  if (!valid_directory(directory) || !nonzero(master, 32) ||
      !mcxvault::valid_network(ctx.network) || !callback)
    return Result::INVALID;
  DIR *dir = opendir(directory);
  if (!dir) return Result::NO_DIRECTORY;
  Result result = Result::OK;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (!segment_name(entry->d_name)) continue;
    char path[MAX_PATH_BYTES] = {};
    if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) < 0 ||
        strlen(path) >= MAX_PATH_BYTES - 1) { result = Result::INVALID; break; }
    uint32_t n = 0;
    const Result this_result = replay_segment(path, master, ctx, callback, user, n);
    stats.recovered += n;
    ++stats.checked_segments;
    if (this_result == Result::TAIL_TRUNCATED) {
      ++stats.truncated_segments; // older authenticated records were delivered
    } else if (this_result != Result::OK) {
      result = this_result;
      break; // don't mask cryptographic tampering by continuing
    }
  }
  (void)closedir(dir);
  return result;
}

} // namespace mcxjournal
