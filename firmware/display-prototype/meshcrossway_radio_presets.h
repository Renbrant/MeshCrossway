#pragma once
#include <stdint.h>
// MeshCore community LoRa presets fetched 2026-10-31 from api.meshcore.nz/api/v1/config.
// These fill radio parameters only; path-hash recommendations are displayed separately.
namespace meshmc {
struct RadioPreset {const char *name;uint32_t frequency,bandwidth;uint8_t sf,cr,hash_bytes;};
static const RadioPreset RADIO_PRESETS[] = {
  {"Australia",915800,250000,10,5,0},
  {"Australia (Narrow)",916575,62500,7,7,0},
  {"Australia (Mid)",915075,125000,9,5,0},
  {"Australia: SA, WA",923125,62500,8,8,0},
  {"Australia: QLD",923125,62500,8,5,0},
  {"Brazil",923125,62500,8,8,0},
  {"Canada",910525,62500,7,5,3},
  {"Costa Rica",910525,125000,11,5,0},
  {"EU/UK (Narrow)",869618,62500,8,8,0},
  {"Czech Republic (Narrow)",869432,62500,7,5,2},
  {"EU 433MHz (Long Range)",433650,250000,11,5,0},
  {"EU 433MHz (Narrow)",433650,62500,8,8,0},
  {"Hungary",869618,62500,7,5,2},
  {"Netherlands",869618,62500,7,5,0},
  {"Netherlands (Limburg)",869618,62500,8,8,2},
  {"New Zealand (Narrow)",917375,62500,7,5,2},
  {"New Zealand (Gisborne)",917375,250000,11,5,1},
  {"Portugal 433",433375,62500,9,6,0},
  {"Portugal 868",869618,62500,7,6,0},
  {"Slovakia",869618,62500,7,5,2},
  {"Switzerland",869618,62500,8,8,0},
  {"Thailand",921000,62500,7,5,0},
  {"USA",910525,62500,7,5,0},
  {"USA - Southern California",927875,62500,7,5,3},
  {"Vietnam (Narrow)",920250,62500,8,5,0},
};
static constexpr unsigned RADIO_PRESET_COUNT=sizeof(RADIO_PRESETS)/sizeof(RadioPreset);
}
