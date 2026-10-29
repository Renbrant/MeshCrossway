#pragma once
// MeshCrossway ESP32-S3 device-key audit, phase A (READ-ONLY).
// No eFuse programming, write-protection changes, secrets or HMAC operations.
// Deliberately has no provisioning function or runtime write dependency.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#if defined(ESP_PLATFORM)
#include "esp_efuse.h"
#endif

namespace mcxkey {

static constexpr uint8_t kKeySlots = 6;
static constexpr uint8_t kHmacUpPurpose = 8;

struct Block {
  uint8_t purpose;
  bool unused;
  bool read_protected;
  bool write_protected;
  bool purpose_write_protected;
};

enum class State : uint8_t {
  UNPROVISIONED,           // candidate free blocks; no trusted enrollment
  HMAC_PRESENT_UNVERIFIED, // existing HMAC_UP never implies MeshCrossway ownership
  UNSAFE_HMAC,             // HMAC_UP present without appropriate read/write protection
  NO_FREE_BLOCKS,         // no HMAC_UP and no free slots
  INCONSISTENT,           // metadata invalid or contradictory
};

struct Audit {
  State state;
  uint8_t available;
  uint8_t hmac_candidates;
  uint8_t unsafe_hmac;
  bool ready_for_storage; // Always false until enrollment and vault are implemented.
};

inline Audit classify(const Block (&blocks)[kKeySlots]) {
  Audit a = {State::UNPROVISIONED, 0, 0, 0, false};
  bool inconsistent = false;
  for (unsigned i = 0; i < kKeySlots; ++i) {
    const Block &b = blocks[i];
    // The purpose value is a numeric, nonsecret eFuse metadata field.
    if (b.purpose > 12) inconsistent = true;
    if (b.unused) {
      if (b.purpose || b.read_protected || b.write_protected ||
          b.purpose_write_protected) inconsistent = true;
      else ++a.available;
    }
    if (b.purpose == kHmacUpPurpose) {
      if (b.unused) inconsistent = true;
      if (b.read_protected && b.write_protected && b.purpose_write_protected)
        ++a.hmac_candidates;
      else ++a.unsafe_hmac;
    }
  }
  a.state = inconsistent ? State::INCONSISTENT
          : a.unsafe_hmac ? State::UNSAFE_HMAC
          : a.hmac_candidates ? State::HMAC_PRESENT_UNVERIFIED
          : a.available ? State::UNPROVISIONED
                        : State::NO_FREE_BLOCKS;
  return a;
}

inline const char *state_name(State s) {
  switch (s) {
    case State::UNPROVISIONED: return "NOT PROVISIONED";
    case State::HMAC_PRESENT_UNVERIFIED: return "KEY PRESENT / NOT ENROLLED";
    case State::UNSAFE_HMAC: return "HMAC PROTECTION INCOMPLETE";
    case State::NO_FREE_BLOCKS: return "NO FREE KEY BLOCKS";
    case State::INCONSISTENT: return "EFUSE METADATA ERROR";
  }
  return "EFUSE STATUS ERROR";
}

inline void describe(const Audit &a, char *dst, size_t cap) {
  if (!dst || !cap) return;
  snprintf(dst, cap,
           "Secure SD: %s\nFree key blocks: %u / %u\n"
           "Storage vault: NOT ACTIVE\n"
           "No eFuse writes or messages saved by this feature.",
           state_name(a.state), (unsigned)a.available, (unsigned)kKeySlots);
}

#if defined(ESP_PLATFORM)

// Evaluates only public metadata; never reads the contents of key blocks.
// Existing HMAC_UP keys must NOT be adopted until authenticated ownership and
// an authenticated storage header are verified in a separate implementation.
inline Audit device_audit() {
  Block blocks[kKeySlots] = {};
  for (unsigned i = 0; i < kKeySlots; ++i) {
    const esp_efuse_block_t block =
        static_cast<esp_efuse_block_t>(EFUSE_BLK_KEY0 + i);
    blocks[i].purpose = static_cast<uint8_t>(esp_efuse_get_key_purpose(block));
    blocks[i].unused = esp_efuse_key_block_unused(block);
    blocks[i].read_protected = esp_efuse_get_key_dis_read(block);
    blocks[i].write_protected = esp_efuse_get_key_dis_write(block);
    blocks[i].purpose_write_protected =
        esp_efuse_get_keypurpose_dis_write(block);
  }
  return classify(blocks);
}
#endif

} // namespace mcxkey
