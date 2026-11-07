#include "../firmware/display-prototype/meshcrossway_map_geometry.h"
#include "../firmware/display-prototype/meshcrossway_pmtiles.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <zlib.h>
using Bytes = std::vector<uint8_t>;
static void put64(Bytes &b, size_t at, uint64_t v) {
  for (unsigned i = 0; i < 8; ++i)
    b[at + i] = uint8_t(v >> (i * 8));
}
static void put32(Bytes &b, size_t at, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = uint8_t(v >> (i * 8));
}
static void var(Bytes &b, uint64_t v) {
  do {
    uint8_t c = v & 127;
    v >>= 7;
    b.push_back(c | (v ? 128 : 0));
  } while (v);
}
static Bytes dir(uint64_t id, uint64_t run, unsigned len, unsigned offset = 0) {
  Bytes b;
  for (auto v : {uint64_t(1), id, run, uint64_t(len), uint64_t(offset + 1)})
    var(b, v);
  return b;
}
static Bytes pack(Bytes root, Bytes leaves = {}, bool gzip = false) {
  Bytes b(127);
  memcpy(b.data(), "PMTiles\3", 8);
  b[97] = gzip ? 2 : 1;
  b[98] = 1;
  b[99] = 2;
  b[100] = 0;
  b[101] = 14;
  b[118] = 14;
  put32(b, 102, uint32_t(-1800000000));
  put32(b, 106, uint32_t(-850000000));
  put32(b, 110, 1800000000);
  put32(b, 114, 850000000);
  put64(b, 8, b.size());
  put64(b, 16, root.size());
  b.insert(b.end(), root.begin(), root.end());
  const char meta[] = "{\"attribution\":\"Test data © "
                      "contributors\",\"other\":[null,true,false,1.25e2]}";
  put64(b, 24, b.size());
  put64(b, 32, sizeof(meta) - 1);
  b.insert(b.end(), meta, meta + sizeof(meta) - 1);
  put64(b, 40, b.size());
  put64(b, 48, leaves.size());
  b.insert(b.end(), leaves.begin(), leaves.end());
  put64(b, 56, b.size());
  put64(b, 64, 4);
  b.insert(b.end(), {1, 2, 3, 4});
  return b;
}
static bool read(void *ctx, uint64_t at, uint8_t *out, size_t n) {
  auto &b = *(Bytes *)ctx;
  if (!mcxpm::range(at, n, b.size()))
    return false;
  memcpy(out, b.data() + at, n);
  return true;
}
static bool inflate_gzip(const uint8_t *p, size_t n, uint8_t *out, size_t cap,
                         size_t &got) {
  z_stream s{};
  s.next_in = (Bytef *)p;
  s.avail_in = n;
  s.next_out = out;
  s.avail_out = cap;
  if (inflateInit2(&s, 31) != Z_OK)
    return false;
  const auto rc = inflate(&s, Z_FINISH);
  got = s.total_out;
  inflateEnd(&s);
  return rc == Z_STREAM_END;
}
static mcxpm::IOContext context(Bytes &b) {
  mcxpm::IOContext c;
  c.context = &b;
  c.size = b.size();
  c.read = read;
  c.allocate = malloc;
  c.release = free;
  c.gunzip = inflate_gzip;
  return c;
}
static Bytes gzip(const Bytes &b) {
  z_stream s{};
  assert(deflateInit2(&s, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) == Z_OK);
  Bytes out(compressBound(b.size()) + 32);
  s.next_in = (Bytef *)b.data();
  s.avail_in = b.size();
  s.next_out = out.data();
  s.avail_out = out.size();
  assert(deflate(&s, Z_FINISH) == Z_STREAM_END);
  out.resize(s.total_out);
  deflateEnd(&s);
  return out;
}
int main() {
  assert(mcxpm::tile_id(0, 0, 0) == 0 && mcxpm::tile_id(1, 0, 0) == 1 &&
         mcxpm::tile_id(1, 0, 1) == 2 && mcxpm::tile_id(1, 1, 1) == 3 &&
         mcxpm::tile_id(1, 1, 0) == 4 &&
         mcxpm::tile_id(12, 3423, 1763) == 19078479);
  assert(mcxpm::tile_id(31, 0, 0) == UINT64_MAX &&
         mcxpm::tile_id(1, 2, 0) == UINT64_MAX);
  mcxpm::Reader r;
  uint64_t at = 0;
  size_t n = 0;
  auto b = pack(dir(0, 5, 4));
  assert(r.open(context(b)) == mcxpm::OK);
  for (unsigned x = 0; x < 2; ++x)
    for (unsigned y = 0; y < 2; ++y)
      assert(r.locate(1, x, y, at, n) == mcxpm::OK && n == 4);
  assert(r.locate(2, 0, 0, at, n) == mcxpm::NOT_FOUND);
  auto leaf = dir(0, 5, 4);
  b = pack(dir(0, 0, leaf.size()), leaf);
  assert(r.open(context(b)) == mcxpm::OK &&
         r.locate(1, 0, 0, at, n) == mcxpm::OK);
  auto hits = r.index_hits;
  assert(r.locate(1, 1, 1, at, n) == mcxpm::OK && r.index_hits > hits);
  b = pack(dir(0, 0, 5), dir(0, 0, 5));
  assert(r.open(context(b)) == mcxpm::OK &&
         r.locate(0, 0, 0, at, n) == mcxpm::DIRECTORY);
  auto compressed = gzip(dir(0, 5, 4));
  b = pack(compressed, {}, true);
  assert(r.open(context(b)) == mcxpm::OK &&
         r.locate(1, 1, 0, at, n) == mcxpm::OK);
  Bytes bomb(1000000, 0);
  b = pack(gzip(bomb), {}, true);
  assert(r.open(context(b)) == mcxpm::COMPRESSION);
  b = pack(dir(0, 1, 4));
  auto valid = b;
  b[99] = 1;
  assert(r.open(context(b)) == mcxpm::UNSUPPORTED);
  b = valid;
  b[99] = 3;
  assert(r.open(context(b)) == mcxpm::UNSUPPORTED);
  b = valid;
  b[97] = 4;
  assert(r.open(context(b)) == mcxpm::UNSUPPORTED);
  b = valid;
  put64(b, 8, UINT64_MAX - 4);
  assert(r.open(context(b)) == mcxpm::HEADER);
  b = valid;
  b.pop_back();
  assert(r.open(context(b)) == mcxpm::HEADER);
  b = pack(Bytes{0});
  assert(r.open(context(b)) == mcxpm::LIMIT);
  b = pack(Bytes(32, 255));
  assert(r.open(context(b)) == mcxpm::LIMIT);
  b = pack(dir(0, 1, 5));
  assert(r.open(context(b)) == mcxpm::DIRECTORY);
  // Random malformed inputs must neither allocate without bounds nor read
  // outside file.
  uint32_t seed = 1;
  for (unsigned i = 0; i < 2000; ++i) {
    b = valid;
    seed = seed * 1664525 + 1013904223;
    const unsigned pos = seed % b.size();
    b[pos] ^= uint8_t(seed >> 24);
    r.open(context(b));
    if (r.error == mcxpm::OK)
      r.locate(1, 0, 0, at, n);
  }
  const char metadata[] =
      R"({"attribution":"Test \u00a9 \ud83c\udf0d","nested":{"one":[true,null,-1.25e+3]}})";
  mcxpm::Metadata m{(const uint8_t *)metadata, sizeof(metadata) - 1};
  assert(m.parse() && strstr(m.attribution, "Test ©"));
  for (const char *bad :
       {"{}", "{\"attribution\":\"x\",}", "{\"attribution\":\"x\"} trailing",
        "{\"attribution\":\"\\ud800\"}",
        "{\"attribution\":\"x\",\"attribution\":\"y\"}"}) {
    mcxpm::Metadata md{(const uint8_t *)bad, strlen(bad)};
    assert(!md.parse());
  }
  for (unsigned levels = 1; levels <= 3; ++levels)
    for (unsigned qx = 0; qx < (1u << levels); ++qx)
      for (unsigned qy = 0; qy < (1u << levels); ++qy) {
        mcxmap::ParentRegion p;
        assert(mcxmap::parent_region(10, 512 + qx, 256 + qy, levels, p));
        assert(p.left == qx * p.size && p.top == qy * p.size &&
               p.left + p.size <= 256 && p.top + p.size <= 256);
      }
  mcxmap::Point points[] = {{0, 179.9}, {0, -179.9}};
  mcxmap::Point center;
  unsigned zoom = 0;
  assert(mcxmap::fit_points(points, 2, 400, 286, 19, false, center, zoom) &&
         fabs(fabs(center.y) - 180) < 0.001 && zoom >= 10);
  auto range = mcxmap::tile_range(-1, 179, 1, -179, 8);
  assert(range.width == 2 && range.height == 2 && range.count == 4);
  assert(!mcxmap::tile_range(10, 0, -10, 1, 8).count);
  puts("PASS: PMTiles header, Hilbert, root/leaf/run lookup, cache, gzip/bomb "
       "bounds, corruption, metadata and antimeridian geometry");
}
