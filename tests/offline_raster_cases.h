#pragma once
// Runs the production worker backend with real PNG decoding and fault-injected
// I/O.
static void run_offline_raster_cases(const char *base) {
  namespace fs = std::filesystem;
  using Bytes = std::vector<uint8_t>;
  const std::string root = std::string(base) + "-offline-v010";
  fs::remove_all(root);
  fs::create_directories(root);
  mcxmap::SD_ROOT = root.c_str();
  mcxmap::clear_tiles();
  mcxpack::close();
  fake_sd_size = 1000000;
  fake_free_space = 1024ULL * 1024 * 1024;
  fake_http_enabled = true;
  fake_http_status = 200;
  fake_read_failure = false;
  mcxmap::backing_off = false;
  mcxmap::wifi_ready = []() { return true; };
  mcxmap::Config c;
  strcpy(c.url, "https://offline.example.test/tiles");
  strcpy(c.attribution, "Test data");
  c.reserved |= 4;
  c.online = 0;
  assert(mcxmap::valid_config(c) && mcxmap::bulk_allowed(c) &&
         mcxmap::provider(c) == mcxmap::CUSTOM);
  for (const char *blocked :
       {"https://tile.openstreetmap.org/", "https://A.TILE.OPENSTREETMAP.ORG",
        "https://a.tile.opentopomap.org/base"}) {
    auto denied = c;
    strcpy(denied.url, blocked);
    assert(!mcxmap::bulk_allowed(denied));
  }
  Bytes rgba(256 * 256 * 4);
  for (unsigned y = 0; y < 256; ++y)
    for (unsigned x = 0; x < 256; ++x) {
      const unsigned i = (y * 256 + x) * 4;
      rgba[i] = x;
      rgba[i + 1] = y;
      rgba[i + 2] = 20;
      rgba[i + 3] = 255;
    }
  uint8_t *encoded = nullptr;
  size_t png_size = 0;
  assert(!lodepng_encode32(&encoded, &png_size, rgba.data(), 256, 256));
  Bytes png(encoded, encoded + png_size);
  lv_mem_free(encoded);
  auto write = [&](const std::string &file, const Bytes &data) {
    fs::create_directories(fs::path(file).parent_path());
    FILE *f = fopen(file.c_str(), "wb");
    assert(f && fwrite(data.data(), 1, data.size(), f) == data.size());
    assert(!fclose(f));
  };
  auto save_tile = [&](const mcxmap::Config &cfg, unsigned z, unsigned x,
                       unsigned y) {
    char file[192];
    assert(mcxmap::directories(cfg, z, x));
    mcxmap::paths(file, sizeof(file), cfg, z, x, y);
    write(file, png);
  };
  auto geo = mcxmap::unproject(513 * 256 + 128, 257 * 256 + 128, 10);
  mcxmap::View v;
  v.latitude = geo.x;
  v.longitude = geo.y;
  v.zoom = 10;
  v.width = v.height = 64;
  v.generation = mcxmap::wanted.fetch_add(1) + 1;
  save_tile(c, 7, 64, 32);
  const auto calls = fake_http_calls;
  auto *f = mcxmap::render(v, c);
  assert(f && f->overzoom == 1 && f->loaded == 1 && !f->missing &&
         fake_http_calls == calls);
  const auto *pixels = (const lv_color_t *)f->image.data;
  assert(pixels[0].full == lv_color_make(44, 44, 20).full);
  assert(pixels[63 * 64 + 63].full == lv_color_make(51, 51, 20).full);
  mcxmap::release(f);
  auto disabled = c;
  mcxmap::set_overzoom(disabled, false);
  f = mcxmap::render(v, disabled);
  assert(f && f->missing == 1 && !f->overzoom);
  mcxmap::release(f);
  const auto decodes = mcxmap::decode_count.load();
  f = mcxmap::render(v, c);
  assert(f && !f->missing && mcxmap::decode_count == decodes);
  mcxmap::release(f);
  c.online = 1;
  fake_http_body = png;
  mcxmap::backing_off = false;
  f = mcxmap::render(v, c);
  assert(f && f->loaded == 1 && !f->overzoom && fake_http_calls == calls + 1);
  mcxmap::release(f);
  // Actual PNG payload in a PMTiles archive, including required literal
  // attribution.
  auto var = [](Bytes &b, uint64_t v) {
    do {
      uint8_t c = v & 127;
      v >>= 7;
      b.push_back(c | (v ? 128 : 0));
    } while (v);
  };
  auto le = [](Bytes &b, size_t at, uint64_t v, unsigned bytes = 8) {
    for (unsigned i = 0; i < bytes; ++i)
      b[at + i] = uint8_t(v >> (8 * i));
  };
  Bytes directory;
  for (auto value : {uint64_t(1), mcxpm::tile_id(10, 513, 257), uint64_t(1),
                     uint64_t(png.size()), uint64_t(1)})
    var(directory, value);
  Bytes pack(127);
  memcpy(pack.data(), "PMTiles\3", 8);
  pack[97] = pack[98] = 1;
  pack[99] = 2;
  pack[100] = 7;
  pack[101] = 10;
  pack[118] = 10;
  le(pack, 102, uint32_t(-1800000000), 4);
  le(pack, 106, uint32_t(-850000000), 4);
  le(pack, 110, 1800000000, 4);
  le(pack, 114, 850000000, 4);
  le(pack, 8, pack.size());
  le(pack, 16, directory.size());
  pack.insert(pack.end(), directory.begin(), directory.end());
  const char meta[] = "{\"attribution\":\"Synthetic © test data\"}";
  le(pack, 24, pack.size());
  le(pack, 32, sizeof(meta) - 1);
  pack.insert(pack.end(), meta, meta + sizeof(meta) - 1);
  le(pack, 40, pack.size());
  le(pack, 56, pack.size());
  le(pack, 64, png.size());
  pack.insert(pack.end(), png.begin(), png.end());
  write(root + "/map-packs/test.pmtiles", pack);
  mcxpack::scan(mcxmap::SD_ROOT);
  assert(mcxpack::snapshot_catalog().count == 1);
  strcpy(c.pack, "test.pmtiles");
  c.online = 0;
  mcxmap::clear_tiles();
  f = mcxmap::render(v, c);
  assert(f && f->loaded == 1 && !f->missing && !f->overzoom);
  mcxmap::release(f);
  assert(strstr(mcxpack::snapshot().attribution, "Synthetic"));
  auto bad = pack;
  bad[99] = 1;
  write(root + "/map-packs/vector.pmtiles", bad);
  assert(!mcxpack::select(mcxmap::SD_ROOT, "vector.pmtiles",
                          mcxmap::milliseconds()) &&
         mcxpack::snapshot().error == mcxpm::UNSUPPORTED);
  bad = pack;
  bad.resize(100);
  write(root + "/map-packs/truncated.pmtiles", bad);
  assert(!mcxpack::select(mcxmap::SD_ROOT, "truncated.pmtiles",
                          mcxmap::milliseconds()));
  // Exercise the production gzip adapter using raw deflate produced by LodePNG.
  uint8_t *zlib = nullptr;
  size_t zsize = 0;
  LodePNGCompressSettings settings;
  lodepng_compress_settings_init(&settings);
  assert(!lodepng_zlib_compress(&zlib, &zsize, directory.data(),
                                directory.size(), &settings));
  Bytes gz = {31, 139, 8, 0, 0, 0, 0, 0, 0, 3};
  gz.insert(gz.end(), zlib + 2, zlib + zsize - 4);
  lv_mem_free(zlib);
  const auto end = gz.size();
  gz.resize(end + 8);
  le(gz, end, mcxpack::crc(directory.data(), directory.size()), 4);
  le(gz, end + 4, directory.size(), 4);
  uint8_t output[128];
  size_t got = 0;
  assert(mcxpack::gunzip(gz.data(), gz.size(), output, sizeof(output), got) &&
         got == directory.size() && !memcmp(output, directory.data(), got));
  gz[end] ^= 1;
  assert(!mcxpack::gunzip(gz.data(), gz.size(), output, sizeof(output), got));
  gz[end] ^= 1;
  assert(!mcxpack::gunzip(gz.data(), gz.size(), output, 1, got));
  // Jobs persist a verified cursor, recover paused and keep a valid older bank.
  using namespace mcxoffline_map;
  assert(initialize());
  loaded = false;
  state() = Snapshot{};
  queued = false;
  last_space_check = 0;
  tick();
  assert(state().ready && state().free_bytes > RESERVE);
  Command command;
  command.action = CREATE;
  command.slot = 0;
  auto &a = command.area;
  a.source = c;
  a.source.pack[0] = 0;
  a.source.online = 1;
  a.south = 40;
  a.north = 40.001;
  a.west = -105;
  a.east = -104.999;
  a.min_zoom = 12;
  a.max_zoom = 13;
  strcpy(a.name, "Test area");
  assert(count(a) > 1);
  assert(submit(command));
  tick();
  assert(state().areas[0].mode == RUNNING && state().areas[0].cursor == 0);
  mcxmap::backing_off = false;
  tick();
  assert(state().areas[0].cursor == 1);
  const auto cursor = state().areas[0].cursor;
  const auto generation = state().areas[0].sequence;
  loaded = false;
  state() = Snapshot{};
  load();
  assert(state().areas[0].mode == PAUSED && state().areas[0].cursor == cursor &&
         state().areas[0].sequence == generation);
  Command resume;
  resume.action = RESUME;
  resume.slot = 0;
  assert(submit(resume));
  tick();
  assert(state().areas[0].mode == RUNNING);
  char journal[192];
  path(journal, sizeof(journal), 0, state().areas[0].sequence & 1);
  write(journal, Bytes{1, 2, 3});
  loaded = false;
  state() = Snapshot{};
  load();
  assert(state().areas[0].cursor == cursor &&
         state().areas[0].mode == PAUSED); // Other bank survives torn write.
  assert(submit(resume));
  tick();
  mcxmap::wifi_ready = []() { return false; };
  tick();
  assert(state().areas[0].cursor == cursor && state().areas[0].mode == RUNNING);
  mcxmap::wifi_ready = []() { return true; };
  fake_free_space = 0;
  last_space_check = 0;
  tick();
  assert(state().areas[0].mode == ERROR && state().areas[0].cursor == cursor);
  fake_free_space = 1024ULL * 1024 * 1024;
  last_space_check = 0;
  assert(submit(resume));
  tick();
  for (unsigned i = 0; i < 20 && state().areas[0].mode == RUNNING; ++i) {
    mcxmap::backing_off = false;
    tick();
  }
  assert(state().areas[0].mode == COMPLETE &&
         state().areas[0].cursor == state().areas[0].total);
  // No download is accepted against the standard OSM tile service.
  command.slot = 1;
  command.area.source = mcxmap::Config{};
  command.area.source.reserved |= 4;
  assert(submit(command));
  tick();
  assert(state().areas[1].mode == EMPTY);
  auto before_files =
      std::distance(fs::recursive_directory_iterator(root + "/mcx-map-cache"),
                    fs::recursive_directory_iterator{});
  Command remove;
  remove.action = REMOVE;
  remove.slot = 0;
  assert(submit(remove));
  tick();
  assert(state().areas[0].mode == EMPTY);
  assert(before_files == std::distance(fs::recursive_directory_iterator(
                                           root + "/mcx-map-cache"),
                                       fs::recursive_directory_iterator{}));
  mcxmap::clear_tiles();
  mcxpack::close();
  mcxmap::SD_ROOT = base;
  puts("PASS: real overzoom pixels, online precedence, pack PNG/metadata/gzip, "
       "offline journal torn-write/reboot/Wi-Fi/full-SD recovery and safe area "
       "cleanup");
}
