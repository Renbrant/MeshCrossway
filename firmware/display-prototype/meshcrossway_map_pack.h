#pragma once
#include "meshcrossway_pmtiles.h"
#include <dirent.h>
namespace mcxpack {
static void *allocate(size_t n) {
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t v = ~0u;
  while (n--) {
    v ^= *p++;
    for (unsigned j = 0; j < 8; ++j)
      v = (v >> 1) ^ (0xedb88320u & uint32_t(-int32_t(v & 1)));
  }
  return ~v;
}
static bool gunzip(const uint8_t *p, size_t n, uint8_t *out, size_t cap,
                   size_t &size) {
  size = 0;
  if (n < 18 || p[0] != 31 || p[1] != 139 || p[2] != 8 || (p[3] & 0xe0))
    return false;
  const uint8_t flags = p[3];
  size_t at = 10;
  if (flags & 4) {
    if (at + 2 > n - 8)
      return false;
    const size_t len = p[at] | size_t(p[at + 1]) << 8;
    at += 2;
    if (len > n - 8 - at)
      return false;
    at += len;
  }
  for (unsigned flag : {8u, 16u})
    if (flags & flag) {
      while (at < n - 8 && p[at])
        ++at;
      if (at >= n - 8)
        return false;
      ++at;
    }
  if (flags & 2) {
    if (at + 2 > n - 8 ||
        uint16_t(crc(p, at)) != uint16_t(p[at] | unsigned(p[at + 1]) << 8))
      return false;
    at += 2;
  }
  if (at >= n - 8 || mcxpm::le32(p + n - 4) > cap)
    return false;
  LodePNGDecompressSettings settings;
  lodepng_decompress_settings_init(&settings);
  settings.max_output_size = cap;
  uint8_t *decoded = nullptr;
  size_t got = 0;
  const unsigned err =
      lodepng_inflate(&decoded, &got, p + at, n - 8 - at, &settings);
  const bool ok = !err && got <= cap && got == mcxpm::le32(p + n - 4) &&
                  crc(decoded, got) == mcxpm::le32(p + n - 8);
  if (ok) {
    memcpy(out, decoded, got);
    size = got;
  }
  lv_mem_free(decoded);
  return ok;
}
static bool safe_name(const char *name) {
  const size_t n = strlen(name);
  if (n < 9 || n > 63 || strcmp(name + n - 8, ".pmtiles"))
    return false;
  for (size_t i = 0; i < n; ++i)
    if (!((name[i] >= 'a' && name[i] <= 'z') ||
          (name[i] >= 'A' && name[i] <= 'Z') ||
          (name[i] >= '0' && name[i] <= '9') || name[i] == '_' ||
          name[i] == '-' || name[i] == '.'))
      return false;
  return name[0] != '.';
}
struct Catalog {
  unsigned count = 0;
  char names[8][64] = {};
};
struct Status {
  char selected[64] = {}, attribution[256] = {};
  mcxpm::Header coverage;
  mcxpm::Error error = mcxpm::OK;
};
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Catalog catalog;
static Status status;
static std::atomic<bool> scan_requested{true};
static Catalog snapshot_catalog() {
  portENTER_CRITICAL(&mux);
  auto c = catalog;
  portEXIT_CRITICAL(&mux);
  return c;
}
static Status snapshot() {
  portENTER_CRITICAL(&mux);
  auto s = status;
  portEXIT_CRITICAL(&mux);
  return s;
}
static void publish(const Status &s) {
  portENTER_CRITICAL(&mux);
  status = s;
  portEXIT_CRITICAL(&mux);
}
static void scan(const char *root) {
  Catalog c;
  char path[160];
  snprintf(path, sizeof(path), "%s/map-packs", root);
  auto *dir = opendir(path);
  if (dir) {
    while (auto *e = readdir(dir)) {
      if (c.count == 8)
        break;
      if (safe_name(e->d_name))
        snprintf(c.names[c.count++], 64, "%.63s", e->d_name);
    }
    closedir(dir);
  }
  portENTER_CRITICAL(&mux);
  catalog = c;
  portEXIT_CRITICAL(&mux);
}
static FILE *file = nullptr;
static mcxpm::Reader reader;
static char opened[64] = {};
static uint32_t retry_at = 0;
static bool read(void *context, uint64_t at, uint8_t *out, size_t n) {
  // ESP-IDF/FATFS uses signed 32-bit fseek in this build. Reject larger packs
  // explicitly.
  return at <= 0x7fffffffULL &&
         fseek((FILE *)context, long(at), SEEK_SET) == 0 &&
         fread(out, 1, n, (FILE *)context) == n && !ferror((FILE *)context);
}
static void close() {
  reader.close();
  if (file)
    fclose(file);
  file = nullptr;
  opened[0] = 0;
}
static bool select(const char *root, const char *name, uint32_t now) {
  if (!name[0]) {
    close();
    publish(Status{});
    return false;
  }
  if (!strcmp(opened, name) && file)
    return true;
  if (!strcmp(opened, name) && int32_t(now - retry_at) < 0)
    return false;
  close();
  Status s;
  snprintf(s.selected, sizeof(s.selected), "%s", name);
  snprintf(opened, sizeof(opened), "%s", name);
  retry_at = now + 5000;
  char path[240];
  snprintf(path, sizeof(path), "%s/map-packs/%s", root, name);
  struct stat st = {};
  if (!safe_name(name) || stat(path, &st) || st.st_size < 127) {
    s.error = mcxpm::IO;
    publish(s);
    return false;
  }
  if (uint64_t(st.st_size) > 0x7fffffffULL) {
    s.error = mcxpm::LIMIT;
    publish(s);
    return false;
  }
  file = fopen(path, "rb");
  if (!file) {
    s.error = mcxpm::IO;
    publish(s);
    return false;
  }
  mcxpm::IOContext io;
  io.context = file;
  io.size = uint64_t(st.st_size);
  io.read = read;
  io.allocate = allocate;
  io.release = heap_caps_free;
  io.gunzip = gunzip;
  s.error = reader.open(io);
  if (s.error == mcxpm::OK) {
    size_t n = 0;
    auto *p =
        reader.section(reader.info.meta, size_t(reader.info.meta_size), n);
    if (p) {
      mcxpm::Metadata metadata{p, n};
      if (metadata.parse())
        snprintf(s.attribution, sizeof(s.attribution), "%s",
                 metadata.attribution);
      else
        s.error = mcxpm::METADATA;
      heap_caps_free(p);
    } else
      s.error = reader.error;
  }
  s.coverage = reader.info;
  if (s.error != mcxpm::OK) {
    reader.close();
    fclose(file);
    file = nullptr;
  }
  publish(s);
  return file != nullptr;
}
static uint8_t *tile(const char *root, const char *name, unsigned z, unsigned x,
                     unsigned y, size_t &size, uint32_t now) {
  if (!select(root, name, now))
    return nullptr;
  uint64_t at = 0;
  const auto error = reader.locate(z, x, y, at, size);
  if (error != mcxpm::OK) {
    if (error != mcxpm::NOT_FOUND) {
      auto s = snapshot();
      s.error = error;
      publish(s);
    }
    return nullptr;
  }
  auto *p = (uint8_t *)allocate(size);
  if (!p)
    return nullptr;
  if (!read(file, at, p, size)) {
    heap_caps_free(p);
    auto s = snapshot();
    s.error = mcxpm::IO;
    publish(s);
    return nullptr;
  }
  return p;
}
} // namespace mcxpack
