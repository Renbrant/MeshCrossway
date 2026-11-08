#pragma once
// Included after the raster backend. This worker owns all job/file operations;
// LVGL exchanges bounded commands and snapshots only.
namespace mcxoffline_map {
static constexpr unsigned MAX_AREAS = 4, MAX_TILES = 4096;
static constexpr uint64_t RESERVE = 8ULL * 1024 * 1024;
enum Mode : uint8_t {
  EMPTY,
  PAUSED,
  RUNNING,
  COMPLETE,
  CANCELLED,
  ERROR,
  TOMBSTONE,
  DELETING
};
struct Area {
  uint32_t magic = 0x31414d43, version = 1, sequence = 0;
  double south = 0, west = 0, north = 0, east = 0;
  mcxmap::Config source;
  uint32_t cursor = 0, total = 0, failed = 0;
  uint64_t bytes = 0;
  uint8_t min_zoom = 0, max_zoom = 0, mode = EMPTY, reserved = 0;
  char name[32] = {};
  uint32_t checksum = 0;
};
struct Snapshot {
  Area areas[MAX_AREAS];
  uint64_t free_bytes = 0;
  char status[128] = "Loading offline areas...";
  bool ready = false;
};
enum Action { NONE, CREATE, RESUME, PAUSE, CANCEL, REMOVE };
struct Command {
  Action action = NONE;
  unsigned slot = 0;
  Area area;
};
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Storage {
  Snapshot published, worker_state;
  Command pending;
};
static Storage *storage = nullptr;
static bool queued = false;
static bool initialize() {
  if (storage)
    return true;
  auto *p =
      heap_caps_malloc(sizeof(Storage), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p)
    return false;
  auto *created = new (p) Storage();
  portENTER_CRITICAL(&mux);
  storage = created;
  portEXIT_CRITICAL(&mux);
  return true;
}
static Snapshot &state() { return storage->worker_state; }
static bool loaded = false;
static uint32_t retry_at = 0, last_space_check = 0;
static unsigned attempts = 0;
// UI owns this PSRAM copy; callers must not retain it across another
// snapshot().
static const Snapshot &snapshot() {
  static Snapshot *copy = nullptr;
  static const Snapshot unavailable{};
  if (!copy) {
    auto *p =
        heap_caps_malloc(sizeof(Snapshot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p)
      copy = new (p) Snapshot();
  }
  if (!copy)
    return unavailable;
  portENTER_CRITICAL(&mux);
  if (storage)
    *copy = storage->published;
  portEXIT_CRITICAL(&mux);
  return *copy;
}
static void publish() {
  if (!storage)
    return;
  portENTER_CRITICAL(&mux);
  storage->published = state();
  portEXIT_CRITICAL(&mux);
}
static bool submit(const Command &command) {
  if (command.slot >= MAX_AREAS)
    return false;
  portENTER_CRITICAL(&mux);
  const bool ok = storage && !queued;
  if (ok) {
    storage->pending = command;
    queued = true;
  }
  portEXIT_CRITICAL(&mux);
  return ok;
}
static uint32_t checksum(const Area &a) {
  const auto *p = (const uint8_t *)&a;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(Area, checksum); ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
static uint32_t count(const Area &a) {
  if (a.max_zoom > 19 || a.min_zoom > a.max_zoom)
    return 0;
  uint64_t total = 0;
  for (unsigned z = a.min_zoom; z <= a.max_zoom; ++z) {
    total += mcxmap::tile_range(a.south, a.west, a.north, a.east, z).count;
    if (total > MAX_TILES)
      return 0;
  }
  return uint32_t(total);
}
static bool tile_at(const Area &a, uint32_t index, unsigned &z, unsigned &x,
                    unsigned &y) {
  for (z = a.min_zoom; z <= a.max_zoom; ++z) {
    const auto r = mcxmap::tile_range(a.south, a.west, a.north, a.east, z);
    if (index < r.count) {
      x = (r.x0 + index % r.width) % (1u << z);
      y = r.y0 + index / r.width;
      return true;
    }
    index -= uint32_t(r.count);
  }
  return false;
}
static bool valid(const Area &a) {
  return a.magic == 0x31414d43 && a.version == 1 && a.mode > EMPTY &&
         a.mode <= DELETING && mcxmap::valid_config(a.source) &&
         !a.source.pack[0] && a.source.online &&
         mcxmap::bulk_allowed(a.source) && memchr(a.name, 0, sizeof(a.name)) &&
         a.name[0] && count(a) == a.total && a.total && a.cursor <= a.total &&
         a.checksum == checksum(a);
}
static void path(char *p, size_t n, unsigned slot, unsigned bank) {
  snprintf(p, n, "%s/mcx-map-areas/area-%u.%u", mcxmap::SD_ROOT, slot, bank);
}
static bool save(unsigned slot, Area &a) {
  Area next = a;
  ++next.sequence;
  next.checksum = checksum(next);
  char file[192];
  path(file, sizeof(file), slot, next.sequence & 1);
  FILE *f = fopen(file, "wb");
  if (!f)
    return false;
  const bool ok = fwrite(&next, 1, sizeof(next), f) == sizeof(next) &&
                  fflush(f) == 0 && fsync(fileno(f)) == 0;
  const bool closed = fclose(f) == 0;
  if (ok && closed) {
    a = next;
    return true;
  }
  return false; // Other bank still contains the last commit.
}
static void load() {
  if (!initialize())
    return;
  if (!esp_sdcard_port_get_size())
    return;
  char root[192];
  snprintf(root, sizeof(root), "%s/mcx-map-areas", mcxmap::SD_ROOT);
  if (mkdir(root, 0755) && errno != EEXIST)
    return;
  state() = Snapshot{};
  for (unsigned slot = 0; slot < MAX_AREAS; ++slot) {
    Area chosen;
    bool found = false;
    for (unsigned bank = 0; bank < 2; ++bank) {
      char file[192];
      path(file, sizeof(file), slot, bank);
      FILE *f = fopen(file, "rb");
      if (!f)
        continue;
      Area a;
      const bool ok = fread(&a, 1, sizeof(a), f) == sizeof(a) &&
                      fgetc(f) == EOF && !ferror(f);
      fclose(f);
      if (ok && valid(a) &&
          (!found || int32_t(a.sequence - chosen.sequence) > 0)) {
        chosen = a;
        found = true;
      }
    }
    if (found) {
      if (chosen.mode == TOMBSTONE) {
        const auto sequence = chosen.sequence;
        chosen = Area{};
        chosen.sequence = sequence;
      }
      if (chosen.mode == RUNNING)
        chosen.mode = PAUSED;
      state().areas[slot] = chosen;
    }
  }
  loaded = true;
  last_space_check = 0;
  state().ready = true;
  snprintf(state().status, sizeof(state().status),
           "Ready / recovered jobs are paused");
  publish();
}
static void handle(const Command &c) {
  auto &a = state().areas[c.slot];
  if (c.action == CREATE) {
    if (a.mode != EMPTY) {
      snprintf(state().status, sizeof(state().status),
               "Remove this area before replacing it");
      return;
    }
    auto next = c.area;
    next.magic = 0x31414d43;
    next.version = 1;
    next.sequence = a.sequence;
    next.cursor = next.failed = 0;
    next.bytes = 0;
    next.total = count(next);
    next.mode = RUNNING;
    next.source.pack[0] = 0;
    next.source.online = 1;
    next.checksum = checksum(next);
    bool active = false;
    for (const auto &other : state().areas)
      active |= other.mode == RUNNING;
    if (active || !valid(next) ||
        uint64_t(next.total) * mcxmap::MAX_PNG + RESERVE > state().free_bytes) {
      snprintf(state().status, sizeof(state().status),
               active ? "Pause the active area first"
                      : "Invalid area / max 4096 tiles / insufficient "
                        "worst-case space");
      return;
    }
    if (!save(c.slot, next)) {
      snprintf(state().status, sizeof(state().status),
               "Area journal write failed");
      return;
    }
    a = next;
    attempts = 0;
    retry_at = 0;
  } else if (c.action == RESUME && a.mode != EMPTY && a.mode != CANCELLED &&
             a.mode != COMPLETE && a.mode != DELETING) {
    for (const auto &other : state().areas)
      if (other.mode == RUNNING) {
        snprintf(state().status, sizeof(state().status),
                 "Pause the active area first");
        return;
      }
    a.mode = RUNNING;
    attempts = 0;
    retry_at = 0;
  } else if (c.action == PAUSE && a.mode == RUNNING)
    a.mode = PAUSED;
  else if (c.action == CANCEL && a.mode != EMPTY && a.mode != DELETING)
    a.mode = CANCELLED;
  else if (c.action == REMOVE) {
    if (a.mode == EMPTY || a.mode == DELETING)
      return;
    const auto previous = a.mode;
    const auto previous_cursor = a.cursor;
    a.mode = DELETING;
    a.cursor = 0;
    if (!save(c.slot, a)) {
      a.mode = previous;
      a.cursor = previous_cursor;
      snprintf(state().status, sizeof(state().status),
               "Could not start deletion / check SD");
    } else
      snprintf(state().status, sizeof(state().status),
               "Deleting area / preserving shared tiles");
    return;
  } else
    return;
  if (!save(c.slot, a)) {
    a.mode = ERROR;
    snprintf(state().status, sizeof(state().status),
             "Journal write failed / resume after checking SD");
  } else
    snprintf(state().status, sizeof(state().status), "Area %u / %s", c.slot + 1,
             a.mode == RUNNING  ? "downloading"
             : a.mode == PAUSED ? "paused"
                                : "cancelled / shared tiles retained");
}
static bool contains_tile(const Area &a, unsigned z, unsigned x, unsigned y) {
  if (a.mode == EMPTY || a.mode == TOMBSTONE || z < a.min_zoom ||
      z > a.max_zoom)
    return false;
  const auto range = mcxmap::tile_range(a.south, a.west, a.north, a.east, z);
  return range.count && y >= range.y0 && y <= range.y1 &&
         (x + (1u << z) - range.x0) % (1u << z) < range.width;
}
static void delete_batch(unsigned slot) {
  auto &a = state().areas[slot];
  if (!esp_sdcard_port_get_size()) {
    snprintf(state().status, sizeof(state().status), "Deletion waiting for SD");
    return;
  }
  const uint32_t before = a.cursor;
  uint64_t freed = 0;
  for (unsigned step = 0; step < 8 && a.cursor < a.total; ++step) {
    unsigned z, x, y;
    if (!tile_at(a, a.cursor, z, x, y)) {
      snprintf(state().status, sizeof(state().status),
               "Invalid deletion bounds");
      return;
    }
    bool shared = false;
    for (unsigned other = 0; other < MAX_AREAS; ++other)
      if (other != slot &&
          mcxmap::source_hash(a.source) ==
              mcxmap::source_hash(state().areas[other].source) &&
          contains_tile(state().areas[other], z, x, y)) {
        shared = true;
        break;
      }
    if (!shared) {
      char file[192];
      mcxmap::paths(file, sizeof(file), a.source, z, x, y);
      struct stat st = {};
      const bool exists = stat(file, &st) == 0;
      if (remove(file) && errno != ENOENT) {
        snprintf(state().status, sizeof(state().status),
                 "Deletion paused / SD write error");
        a.cursor = before;
        return;
      }
      if (exists && st.st_size > 0)
        freed += uint64_t(st.st_size);
    }
    ++a.cursor;
  }
  const bool done = a.cursor == a.total;
  a.mode = done ? TOMBSTONE : DELETING;
  if (!save(slot, a)) {
    a.mode = DELETING;
    a.cursor = before;
    snprintf(state().status, sizeof(state().status),
             "Deletion checkpoint failed / retrying safely");
    return;
  }
  if (done) {
    const auto sequence = a.sequence;
    a = Area{};
    a.sequence = sequence;
    mcxmap::clear_tiles();
    snprintf(state().status, sizeof(state().status),
             "Area deleted / unshared tiles removed");
  } else
    snprintf(state().status, sizeof(state().status),
             "Deleting area / %lu of %lu tiles checked",
             (unsigned long)a.cursor, (unsigned long)a.total);
  (void)freed;
  last_space_check = 0;
}
static void tick() {
  if (!loaded) {
    load();
    if (!loaded)
      return;
  }
  const uint32_t now = mcxmap::milliseconds();
  if (!last_space_check || uint32_t(now - last_space_check) > 2000) {
    uint64_t total = 0;
    state().free_bytes = 0;
    if (esp_sdcard_port_get_size())
      esp_vfs_fat_info(mcxmap::SD_ROOT, &total, &state().free_bytes);
    last_space_check = now;
  }
  Command command;
  bool take = false;
  portENTER_CRITICAL(&mux);
  if (queued) {
    command = storage->pending;
    queued = false;
    take = true;
  }
  portEXIT_CRITICAL(&mux);
  if (take) {
    handle(command);
    publish();
    return;
  }
  for (unsigned slot = 0; slot < MAX_AREAS; ++slot)
    if (state().areas[slot].mode == DELETING) {
      delete_batch(slot);
      publish();
      return;
    }
  for (unsigned slot = 0; slot < MAX_AREAS; ++slot) {
    auto &a = state().areas[slot];
    if (a.mode != RUNNING)
      continue;
    if (!esp_sdcard_port_get_size() ||
        state().free_bytes < RESERVE + mcxmap::MAX_PNG) {
      a.mode = ERROR;
      snprintf(state().status, sizeof(state().status),
               "SD unavailable / full / resume after checking card");
      save(slot, a);
      break;
    }
    if (!mcxmap::network_allowed()) {
      snprintf(state().status, sizeof(state().status),
               "Waiting for Wi-Fi / progress saved");
      break;
    }
    if (int32_t(now - retry_at) < 0)
      break;
    unsigned z = 0, x = 0, y = 0;
    if (!tile_at(a, a.cursor, z, x, y)) {
      a.mode = COMPLETE;
      save(slot, a);
      break;
    }
    // Bulk source cannot inherit an archive or another provider's loose XYZ
    // tiles.
    mcxmap::Config c = a.source;
    size_t size = 0;
    const auto generation = mcxmap::wanted.load();
    auto *p = mcxmap::obtain(c, z, x, y, size, nullptr, generation);
    const bool success = p != nullptr;
    heap_caps_free(p);
    if (mcxpriority::ble_initializing.load() ||
        mcxmap::wanted.load() != generation)
      break;
    if (success) {
      ++a.cursor;
      a.bytes += size;
      attempts = 0;
      retry_at = 0;
      if (a.cursor == a.total)
        a.mode = COMPLETE;
      if (!save(slot, a)) {
        a.mode = ERROR;
        snprintf(state().status, sizeof(state().status),
                 "Checkpoint failed / resume will verify the tile again");
        break;
      }
      snprintf(state().status, sizeof(state().status),
               "%s / %lu of %lu verified tiles",
               a.mode == COMPLETE ? "Complete" : "Downloading",
               (unsigned long)a.cursor, (unsigned long)a.total);
    } else {
      ++a.failed;
      ++attempts;
      retry_at = now + uint32_t(5000u << (attempts > 4 ? 4 : attempts - 1));
      if (attempts >= 5)
        a.mode = ERROR;
      snprintf(state().status, sizeof(state().status),
               "Tile %u/%u/%u failed / %s", z, x, y,
               a.mode == ERROR ? "paused; use resume to retry"
                               : "retrying with backoff");
      if (!save(slot, a))
        a.mode = ERROR;
    }
    break; // One request per worker iteration, rendering always takes priority.
  }
  publish();
}
} // namespace mcxoffline_map
