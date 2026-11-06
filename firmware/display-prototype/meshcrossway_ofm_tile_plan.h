// SPDX-License-Identifier: MIT
// Standalone planning prototype: NOT included by firmware or build system.
// No networking, SD access, LVGL calls, or production integration.
#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace mcx_ofm_plan {
constexpr double kMaxLat = 85.0511287798066;
constexpr uint32_t kMaxTilesPerDownload = 3000;
constexpr uint64_t kMaxBytesPerDownload = 50000000ULL;
constexpr uint32_t kMaxParallelRequests = 4;
constexpr unsigned kMaxSourceZoom = 14; // verify against TileJSON at runtime

struct TileRect { uint32_t left=0, top=0, right=0, bottom=0; unsigned zoom=0; bool valid=false; };
struct Budget { uint64_t tiles=0, bytes=0; bool accepted=false; };
inline double clamp_lat(double lat) {
  if (!std::isfinite(lat)) return 0;
  return lat < -kMaxLat ? -kMaxLat : (lat > kMaxLat ? kMaxLat : lat);
}
inline double wrap_lon(double lon) {
  if (!std::isfinite(lon)) return 0;
  double w = std::fmod(lon + 180.0, 360.0);
  if (w < 0) w += 360.0;
  return w - 180.0;
}
inline uint32_t tile_x(double lon, unsigned zoom) {
  if (zoom > 22) return 0;
  const double n = double(uint32_t(1) << zoom);
  const double x = (wrap_lon(lon) + 180.0) / 360.0 * n;
  return uint32_t(x >= n ? n-1 : (x < 0 ? 0 : x));
}
inline uint32_t tile_y(double lat, unsigned zoom) {
  if (zoom > 22) return 0;
  const double n = double(uint32_t(1) << zoom);
  const double r = clamp_lat(lat) * (3.14159265358979323846 / 180.0);
  const double v = (1.0 - std::asinh(std::tan(r)) / 3.14159265358979323846) * 0.5 * n;
  return uint32_t(v < 0 ? 0 : (v >= n ? n-1 : v));
}
// Non-antimeridian rectangular selection. Reject crossings rather than silently
// selecting most of the planet; caller must split view in a future implementation.
inline TileRect rect(double south, double west, double north, double east, unsigned z) {
  TileRect r;
  if (z > 22 || !std::isfinite(south) || !std::isfinite(north) ||
      !std::isfinite(west) || !std::isfinite(east) ||
      south > north || west > east || west < -180 || east > 180) return r;
  r.zoom=z;
  r.left=tile_x(west,z);
  // East is exclusive to avoid including an extra column at exact tile edges.
  const double east_inclusive = std::nextafter(east, -std::numeric_limits<double>::infinity());
  r.right=(east==180.0) ? ((uint32_t(1)<<z)-1) : tile_x(east_inclusive,z);
  r.top=tile_y(north,z);
  r.bottom=tile_y(south,z);
  r.valid=r.left<=r.right && r.top<=r.bottom;
  return r;
}
inline uint64_t count(const TileRect &r) {
  return r.valid ? uint64_t(r.right-r.left+1)*uint64_t(r.bottom-r.top+1) : 0;
}
inline Budget evaluate(uint64_t tiles, uint64_t estimated_bytes) {
  return {tiles,estimated_bytes,tiles>0 && tiles<=kMaxTilesPerDownload &&
    estimated_bytes<=kMaxBytesPerDownload};
}
// Aggregate counts before scheduling requests; byte limit must ALSO be checked
// during streaming against actual transferred bytes.
inline Budget estimate(double south,double west,double north,double east,
                       unsigned min_zoom,unsigned max_zoom,uint64_t estimated_bytes) {
  if (min_zoom>max_zoom || max_zoom>kMaxSourceZoom) return {};
  uint64_t total=0;
  for(unsigned z=min_zoom;z<=max_zoom;++z) {
    auto r=rect(south,west,north,east,z);
    if(!r.valid) return {};
    total+=count(r);
    if(total>kMaxTilesPerDownload) return {total,estimated_bytes,false};
  }
  return evaluate(total,estimated_bytes);
}
} // namespace mcx_ofm_plan
