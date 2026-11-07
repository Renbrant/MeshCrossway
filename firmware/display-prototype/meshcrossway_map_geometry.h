#pragma once
#include "meshcrossway_map_projection.h"
#include <algorithm>
namespace mcxmap {
struct ParentRegion {
  unsigned z = 0, x = 0, y = 0, left = 0, top = 0, size = 256, scale = 1;
};
static bool parent_region(unsigned z, unsigned x, unsigned y, unsigned levels,
                          ParentRegion &p) {
  if (!levels || levels > 3 || levels > z || z > 30 || x >= (1u << z) ||
      y >= (1u << z))
    return false;
  p.z = z - levels;
  p.x = x >> levels;
  p.y = y >> levels;
  p.scale = 1u << levels;
  p.size = 256 / p.scale;
  p.left = (x & (p.scale - 1)) * p.size;
  p.top = (y & (p.scale - 1)) * p.size;
  return true;
}
// Largest-gap longitude fit: a group around +180/-180 stays together.
static bool fit_points(Point *points, size_t count, unsigned width,
                       unsigned height, unsigned max_zoom, bool magnified,
                       Point &center, unsigned &zoom) {
  if (!count || width <= 48 || height <= 48)
    return false;
  double top = 1, bottom = 0;
  for (size_t i = 0; i < count; ++i) {
    if (!isfinite(points[i].x) || !isfinite(points[i].y) ||
        fabs(points[i].x) > 90 || fabs(points[i].y) > 180)
      return false;
    const auto p = project(points[i].x, points[i].y, 0);
    points[i] = {p.x / 256, p.y / 256};
    top = fmin(top, points[i].y);
    bottom = fmax(bottom, points[i].y);
  }
  std::sort(points, points + count,
            [](const Point &a, const Point &b) { return a.x < b.x; });
  double gap = -1, start = 0;
  for (size_t i = 0; i < count; ++i) {
    const double next = points[(i + 1) % count].x + (i + 1 == count ? 1 : 0),
                 d = next - points[i].x;
    if (d > gap) {
      gap = d;
      start = next;
    }
  }
  const double span = 1 - gap, mid = wrap(start + span / 2, 1);
  zoom = 0;
  const double scale = magnified ? 1.25 : 1;
  for (unsigned z = 0; z <= max_zoom; ++z) {
    const double world = 256.0 * (1u << z) * scale;
    if (span * world > width - 48 || (bottom - top) * world > height - 48)
      break;
    zoom = z;
  }
  center = unproject(mid * 256, (top + bottom) * 128, 0);
  return true;
}
struct TileRange {
  unsigned z = 0, x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  uint32_t width = 0, height = 0;
  uint64_t count = 0;
};
static TileRange tile_range(double south, double west, double north,
                            double east, unsigned z) {
  TileRange r;
  r.z = z;
  if (z > 19 || !isfinite(south) || !isfinite(north) || !isfinite(west) ||
      !isfinite(east) || south > north || south < -90 || north > 90 ||
      west < -180 || west > 180 || east < -180 || east > 180)
    return r;
  const unsigned n = 1u << z;
  const auto a = project(north, west, z), b = project(south, east, z);
  r.x0 = unsigned(a.x / 256);
  r.x1 = east == 180 ? n - 1 : unsigned(b.x / 256);
  r.y0 = unsigned(fmax(0, fmin(n - 1, floor(a.y / 256))));
  r.y1 = unsigned(fmax(0, fmin(n - 1, floor(b.y / 256))));
  r.width = (r.x1 + n - r.x0) % n + 1;
  r.height = r.y1 - r.y0 + 1;
  r.count = uint64_t(r.width) * r.height;
  return r;
}
} // namespace mcxmap
