#pragma once
// Independent PMTiles v3 raster reader, implemented from the public format
// spec. No filesystem, platform allocator or third-party reader implementation
// required.
#include <initializer_list>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace mcxpm {
static constexpr size_t HEADER_SIZE = 127, MAX_SECTION = 65536,
                        MAX_ENTRIES = 4096, MAX_TILE = 262144;
static constexpr uint64_t MAX_ID =
    ((uint64_t(1) << 62) - 1) / 3; // supported z <= 30
static uint64_t le64(const uint8_t *p) {
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i)
    v |= uint64_t(p[i]) << (8 * i);
  return v;
}
static uint32_t le32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
static bool range(uint64_t at, uint64_t n, uint64_t end) {
  return at <= end && n <= end - at;
}
static uint64_t tile_id(unsigned z, uint32_t x, uint32_t y) {
  if (z > 30 || x >= (uint32_t(1) << z) || y >= (uint32_t(1) << z))
    return UINT64_MAX;
  const uint32_t n = uint32_t(1) << z;
  uint64_t d = 0;
  for (uint32_t s = n / 2; s; s /= 2) {
    const uint32_t rx = (x & s) ? 1 : 0, ry = (y & s) ? 1 : 0;
    d += uint64_t(s) * s * ((3 * rx) ^ ry);
    if (!ry) {
      if (rx) {
        x = n - 1 - x;
        y = n - 1 - y;
      }
      const auto t = x;
      x = y;
      y = t;
    }
  }
  return ((uint64_t(1) << (2 * z)) - 1) / 3 + d;
}
enum Error {
  OK = 0,
  IO,
  HEADER,
  UNSUPPORTED,
  LIMIT,
  DIRECTORY,
  NOT_FOUND,
  MEMORY,
  COMPRESSION,
  METADATA
};
static const char *message(Error e) {
  switch (e) {
  case OK:
    return "Ready";
  case IO:
    return "Truncated / SD read error";
  case HEADER:
    return "Invalid PMTiles v3 header";
  case UNSUPPORTED:
    return "Only PNG / none or gzip directories supported";
  case LIMIT:
    return "Pack exceeds bounded reader limits";
  case DIRECTORY:
    return "Invalid PMTiles directory";
  case NOT_FOUND:
    return "Tile absent";
  case MEMORY:
    return "PSRAM allocation failed";
  case COMPRESSION:
    return "Invalid / oversized gzip section";
  default:
    return "Pack needs valid attribution metadata";
  }
}
struct Header {
  uint64_t root = 0, root_size = 0, meta = 0, meta_size = 0, leaf = 0,
           leaf_size = 0, data = 0, data_size = 0;
  uint8_t compression = 0, min_zoom = 0, max_zoom = 0, center_zoom = 0;
  double west = 0, south = 0, east = 0, north = 0, latitude = 0, longitude = 0;
};
static Error header(const uint8_t *p, size_t n, uint64_t file_size, Header &h) {
  if (n < HEADER_SIZE || memcmp(p, "PMTiles\3", 8))
    return HEADER;
  h.root = le64(p + 8);
  h.root_size = le64(p + 16);
  h.meta = le64(p + 24);
  h.meta_size = le64(p + 32);
  h.leaf = le64(p + 40);
  h.leaf_size = le64(p + 48);
  h.data = le64(p + 56);
  h.data_size = le64(p + 64);
  h.compression = p[97];
  h.min_zoom = p[100];
  h.max_zoom = p[101];
  h.center_zoom = p[118];
  h.west = int32_t(le32(p + 102)) / 1e7;
  h.south = int32_t(le32(p + 106)) / 1e7;
  h.east = int32_t(le32(p + 110)) / 1e7;
  h.north = int32_t(le32(p + 114)) / 1e7;
  h.longitude = int32_t(le32(p + 119)) / 1e7;
  h.latitude = int32_t(le32(p + 123)) / 1e7;
  if (p[99] != 2 || p[98] != 1 || (h.compression != 1 && h.compression != 2))
    return UNSUPPORTED;
  if (p[96] > 1 || h.min_zoom > h.max_zoom || h.max_zoom > 30 ||
      h.center_zoom > 30 || h.west < -180 || h.west > 180 || h.east < -180 ||
      h.east > 180 || h.south < -90 || h.north > 90 || h.south > h.north ||
      h.longitude < -180 || h.longitude > 180 || h.latitude < -90 ||
      h.latitude > 90)
    return HEADER;
  const uint64_t offsets[] = {h.root, h.meta, h.leaf, h.data},
                 sizes[] = {h.root_size, h.meta_size, h.leaf_size, h.data_size};
  if (!h.root_size || !h.meta_size || !range(h.root, h.root_size, 16384))
    return HEADER;
  for (unsigned i = 0; i < 4; ++i) {
    if ((sizes[i] && offsets[i] < HEADER_SIZE) ||
        !range(offsets[i], sizes[i], file_size))
      return HEADER;
    for (unsigned j = 0; j < i; ++j)
      if (sizes[i] && sizes[j] && offsets[i] < offsets[j] + sizes[j] &&
          offsets[j] < offsets[i] + sizes[i])
        return HEADER;
  }
  if (h.meta_size > MAX_SECTION)
    return LIMIT;
  return OK;
}
struct Cursor {
  const uint8_t *p;
  size_t n, at = 0;
  bool var(uint64_t &v) {
    v = 0;
    for (unsigned i = 0; i < 10; ++i) {
      if (at == n)
        return false;
      const uint8_t b = p[at++];
      if (i == 9 && (b & 254))
        return false;
      v |= uint64_t(b & 127) << (i * 7);
      if (!(b & 128))
        return true;
    }
    return false;
  }
};
struct Entry {
  uint64_t id = 0, offset = 0, run = 0;
  uint32_t length = 0;
};
static Error directory(const uint8_t *p, size_t n, const Header &h,
                       Entry *entries, size_t &count) {
  Cursor c{p, n};
  uint64_t v = 0, id = 0;
  if (!c.var(v) || !v || v > MAX_ENTRIES) {
    count = 0;
    return LIMIT;
  }
  count = size_t(v);
  for (size_t i = 0; i < count; ++i) {
    if (!c.var(v) || (i && !v) || v > MAX_ID - id)
      return DIRECTORY;
    id += v;
    entries[i].id = id;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!c.var(v) || v > MAX_ID - entries[i].id)
      return DIRECTORY;
    entries[i].run = v;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!c.var(v) || !v || v > UINT32_MAX)
      return DIRECTORY;
    entries[i].length = uint32_t(v);
  }
  for (size_t i = 0; i < count; ++i) {
    auto &e = entries[i];
    if (!c.var(v) || (!v && !i))
      return DIRECTORY;
    e.offset = v ? v - 1 : entries[i - 1].offset + entries[i - 1].length;
    if (!range(e.offset, e.length, e.run ? h.data_size : h.leaf_size) ||
        e.length > (e.run ? MAX_TILE : MAX_SECTION))
      return DIRECTORY;
    if (i + 1 < count && e.run > entries[i + 1].id - e.id)
      return DIRECTORY;
  }
  return c.at == n ? OK : DIRECTORY;
}
// Small bounded JSON validator. Only root attribution is retained, as literal
// UTF-8.
struct Metadata {
  const uint8_t *p;
  size_t n, at = 0;
  char attribution[256] = {};
  void space() {
    while (at < n &&
           (p[at] == ' ' || p[at] == '\r' || p[at] == '\n' || p[at] == '\t'))
      ++at;
  }
  bool take(char c) {
    space();
    if (at < n && p[at] == c) {
      ++at;
      return true;
    }
    return false;
  }
  bool hex(uint32_t &v) {
    v = 0;
    for (unsigned i = 0; i < 4; ++i) {
      if (at == n)
        return false;
      const auto c = p[at++];
      const int d = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                           : -1;
      if (d < 0)
        return false;
      v = (v << 4) | unsigned(d);
    }
    return true;
  }
  bool string(char *out = nullptr, size_t cap = 0) {
    if (!take('"'))
      return false;
    size_t used = 0;
    auto emit = [&](uint8_t c) {
      if (out) {
        if (used + 1 >= cap)
          return false;
        out[used] = char(c);
      }
      ++used;
      return true;
    };
    while (at < n) {
      uint8_t c = p[at++];
      if (c == '"') {
        if (out)
          out[used] = 0;
        return true;
      }
      if (c < 32)
        return false;
      if (c == '\\') {
        if (at == n)
          return false;
        c = p[at++];
        if (c == 'u') {
          uint32_t u;
          if (!hex(u))
            return false;
          if (u >= 0xd800 && u <= 0xdbff) {
            if (at + 2 > n || p[at++] != '\\' || p[at++] != 'u')
              return false;
            uint32_t low;
            if (!hex(low) || low < 0xdc00 || low > 0xdfff)
              return false;
            u = 0x10000 + ((u - 0xd800) << 10) + (low - 0xdc00);
          } else if (u >= 0xdc00 && u <= 0xdfff)
            return false;
          if (u < 32 && out)
            return false;
          if (u < 128) {
            if (!emit(uint8_t(u)))
              return false;
          } else if (u < 2048) {
            if (!emit(0xc0 | (u >> 6)) || !emit(0x80 | (u & 63)))
              return false;
          } else if (u < 65536) {
            if (!emit(0xe0 | (u >> 12)) || !emit(0x80 | ((u >> 6) & 63)) ||
                !emit(0x80 | (u & 63)))
              return false;
          } else if (!emit(0xf0 | (u >> 18)) ||
                     !emit(0x80 | ((u >> 12) & 63)) ||
                     !emit(0x80 | ((u >> 6) & 63)) || !emit(0x80 | (u & 63)))
            return false;
          continue;
        }
        if (c != '"' && c != '\\' && c != '/') {
          if (out || !strchr("bfnrt", c))
            return false;
          c = ' ';
        }
      }
      if (c >= 128) {
        const unsigned bytes = c >= 0xc2 && c <= 0xdf   ? 1
                               : c >= 0xe0 && c <= 0xef ? 2
                               : c >= 0xf0 && c <= 0xf4 ? 3
                                                        : 0;
        if (!bytes || at + bytes > n)
          return false;
        uint32_t u = c & ((1u << (6 - bytes)) - 1);
        if (!emit(c))
          return false;
        for (unsigned i = 0; i < bytes; ++i) {
          const auto b = p[at++];
          if ((b & 0xc0) != 0x80 || !emit(b))
            return false;
          u = (u << 6) | (b & 63);
        }
        if ((bytes == 1 && u < 128) || (bytes == 2 && u < 2048) ||
            (bytes == 3 && u < 65536) || u > 0x10ffff ||
            (u >= 0xd800 && u <= 0xdfff))
          return false;
      } else if (!emit(c))
        return false;
    }
    return false;
  }
  bool value(unsigned depth = 0) {
    if (depth > 8)
      return false;
    space();
    if (at == n)
      return false;
    if (p[at] == '"')
      return string();
    if (take('{')) {
      if (take('}'))
        return true;
      do {
        char key[256];
        if (!string(key, sizeof(key)) || !take(':'))
          return false;
        if (!depth && !strcmp(key, "attribution")) {
          if (attribution[0] || !string(attribution, sizeof(attribution)))
            return false;
        } else if (!value(depth + 1))
          return false;
      } while (take(','));
      return take('}');
    }
    if (take('[')) {
      if (take(']'))
        return true;
      do {
        if (!value(depth + 1))
          return false;
      } while (take(','));
      return take(']');
    }
    for (const char *word : {"true", "false", "null"}) {
      const size_t len = strlen(word);
      if (at + len <= n && !memcmp(p + at, word, len)) {
        at += len;
        return true;
      }
    }
    if (p[at] == '-')
      ++at;
    if (at == n)
      return false;
    if (p[at] == '0')
      ++at;
    else {
      if (p[at] < '1' || p[at] > '9')
        return false;
      while (at < n && p[at] >= '0' && p[at] <= '9')
        ++at;
    }
    if (at < n && p[at] == '.') {
      ++at;
      const auto start = at;
      while (at < n && p[at] >= '0' && p[at] <= '9')
        ++at;
      if (at == start)
        return false;
    }
    if (at < n && (p[at] == 'e' || p[at] == 'E')) {
      ++at;
      if (at < n && (p[at] == '+' || p[at] == '-'))
        ++at;
      const auto start = at;
      while (at < n && p[at] >= '0' && p[at] <= '9')
        ++at;
      if (at == start)
        return false;
    }
    return true;
  }
  bool parse() {
    space();
    if (at == n || p[at] != '{' || !value())
      return false;
    space();
    return at == n && attribution[0];
  }
};
struct IOContext {
  void *context = nullptr;
  uint64_t size = 0;
  bool (*read)(void *, uint64_t, uint8_t *, size_t) = nullptr;
  void *(*allocate)(size_t) = nullptr;
  void (*release)(void *) = nullptr;
  // Decompress one complete gzip stream; must enforce output cap during
  // inflation.
  bool (*gunzip)(const uint8_t *, size_t, uint8_t *, size_t,
                 size_t &) = nullptr;
};
struct Reader {
  IOContext io;
  Header info;
  Error error = HEADER;
  Entry *root = nullptr, *leaf = nullptr;
  size_t root_count = 0, leaf_count = 0;
  uint64_t leaf_at = UINT64_MAX;
  uint32_t leaf_length = 0;
  uint32_t index_hits = 0, range_reads = 0;
  Reader() = default;
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  void close() {
    if (io.release) {
      io.release(root);
      io.release(leaf);
    }
    root = leaf = nullptr;
    root_count = leaf_count = 0;
    leaf_at = UINT64_MAX;
  }
  ~Reader() { close(); }
  uint8_t *section(uint64_t at, size_t n, size_t &size) {
    size = 0;
    if (!n || n > MAX_SECTION || !range(at, n, io.size)) {
      error = LIMIT;
      return nullptr;
    }
    auto *raw = (uint8_t *)io.allocate(n);
    if (!raw) {
      error = MEMORY;
      return nullptr;
    }
    ++range_reads;
    if (!io.read(io.context, at, raw, n)) {
      io.release(raw);
      error = IO;
      return nullptr;
    }
    if (info.compression == 1) {
      size = n;
      return raw;
    }
    auto *out = (uint8_t *)io.allocate(MAX_SECTION + 1);
    if (!out) {
      io.release(raw);
      error = MEMORY;
      return nullptr;
    }
    const bool ok = io.gunzip && io.gunzip(raw, n, out, MAX_SECTION, size);
    io.release(raw);
    if (!ok || !size || size > MAX_SECTION) {
      io.release(out);
      error = COMPRESSION;
      return nullptr;
    }
    return out;
  }
  Error open(const IOContext &ctx) {
    close();
    io = ctx;
    if (!io.read || !io.allocate || !io.release)
      return error = IO;
    uint8_t bytes[HEADER_SIZE];
    if (!io.read(io.context, 0, bytes, sizeof(bytes)))
      return error = IO;
    error = header(bytes, sizeof(bytes), io.size, info);
    if (error != OK)
      return error;
    root = (Entry *)io.allocate(MAX_ENTRIES * sizeof(Entry));
    if (!root)
      return error = MEMORY;
    size_t n = 0;
    auto *p = section(info.root, size_t(info.root_size), n);
    if (!p)
      return error;
    error = directory(p, n, info, root, root_count);
    io.release(p);
    return error;
  }
  Error locate(unsigned z, unsigned x, unsigned y, uint64_t &at, size_t &size) {
    if (!root || !root_count)
      return error = HEADER;
    const uint64_t id = tile_id(z, x, y);
    if (id == UINT64_MAX || z < info.min_zoom || z > info.max_zoom)
      return NOT_FOUND;
    const Entry *entries = root;
    size_t count = root_count;
    for (unsigned depth = 0; depth < 4; ++depth) {
      size_t lo = 0, hi = count;
      while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (entries[mid].id <= id)
          lo = mid + 1;
        else
          hi = mid;
      }
      if (!lo)
        return NOT_FOUND;
      const Entry e = entries[lo - 1];
      if (e.run) {
        if (id - e.id >= e.run)
          return NOT_FOUND;
        at = info.data + e.offset;
        size = e.length;
        return error = OK;
      }
      const uint64_t next = info.leaf + e.offset;
      if (leaf && leaf_at == next && leaf_length == e.length) {
        ++index_hits;
        entries = leaf;
        count = leaf_count;
        continue;
      }
      if (!leaf)
        leaf = (Entry *)io.allocate(MAX_ENTRIES * sizeof(Entry));
      if (!leaf)
        return error = MEMORY;
      size_t n = 0;
      auto *p = section(next, e.length, n);
      if (!p)
        return error;
      leaf_at = UINT64_MAX;
      error = directory(p, n, info, leaf, leaf_count);
      io.release(p);
      if (error != OK)
        return error;
      leaf_at = next;
      leaf_length = e.length;
      entries = leaf;
      count = leaf_count;
    }
    return error = DIRECTORY; // Cycles / excessive directory depth.
  }
};
} // namespace mcxpm
