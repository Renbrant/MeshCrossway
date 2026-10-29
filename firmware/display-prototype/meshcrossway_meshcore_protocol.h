#pragma once
// Independent Companion wire-format implementation. See docs/meshcore-v070.md for pinned sources.
// No upstream firmware/library code is linked or copied. All integers on this wire are little endian.
#include <errno.h>
#include <limits.h>
#include <new>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace meshmc {
static constexpr size_t MAX_FRAME = 176, MAX_TEXT = 150, MAX_CONTACTS = 512, MAX_CHANNELS = 256,
                        MAX_MESSAGES = 96;
struct Frame {
  uint16_t len = 0;
  uint8_t data[MAX_FRAME] = {};
};
static uint16_t u16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
static uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static void put32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; i++)
    p[i] = uint8_t(n >> (8 * i));
}
static void string_copy(char *to, size_t cap, const uint8_t *from, size_t n) {
  if (!cap)
    return;
  if (n >= cap)
    n = cap - 1;
  memcpy(to, from, n);
  to[n] = 0;
}
static int nibble(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
static bool unhex(const char *s, uint8_t *out, size_t cap, size_t &used) {
  used = 0;
  int hi = -1;
  for (; *s; s++) {
    if (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
      continue;
    int v = nibble(*s);
    if (v < 0)
      return false;
    if (hi < 0)
      hi = v;
    else {
      if (used == cap)
        return false;
      out[used++] = uint8_t(hi * 16 + v);
      hi = -1;
    }
  }
  return hi < 0;
}
static void hex(const uint8_t *p, size_t n, char *out, size_t cap) {
  static const char digits[] = "0123456789abcdef";
  size_t j = 0;
  for (size_t i = 0; i < n && j + 2 < cap; i++) {
    out[j++] = digits[p[i] >> 4];
    out[j++] = digits[p[i] & 15];
  }
  if (cap)
    out[j] = 0;
}
// Field schema: byte/word/uint/int/sbyte, key (32B), prefix (6B), secret (16B),
// identity (64B), path (64B), name (32B NUL padded), scope (31B), text, bytes.
// Fixed zeros/literals are inserted by the builder, never typed by the user.
struct Command {
  uint8_t code, response, min_version;
  const char *name, *fields;
  bool confirm;
};
static const Command COMMANDS[] = {
    {1, 5, 1, "Refresh identity", "", false},
    {2, 6, 1, "Send direct text",
     "byte:Text type|byte:Attempt|uint:Unix time|prefix:Recipient prefix|text:Message", false},
    {3, 0, 1, "Send channel text", "byte:Text type|byte:Channel slot|uint:Unix time|text:Message", false},
    {4, 4, 1, "Refresh contacts", "", false},
    {5, 9, 1, "Read clock", "", false},
    {6, 0, 1, "Set clock", "uint:Unix seconds", true},
    {7, 0, 1, "Advertise identity", "byte:Mode (0 nearby / 1 flood)", true},
    {8, 0, 1, "Rename radio", "text:Radio name", true},
    {9, 0, 1, "Add or update contact",
     "key:Public key|byte:Type|byte:Flags|byte:Encoded path length|path:Padded path|name:Contact "
     "name|uint:Advert time|int:Latitude x1000000|int:Longitude x1000000",
     true},
    {10, 10, 1, "Fetch waiting messages", "", false},
    {11, 0, 1, "Radio parameters",
     "uint:Frequency kHz|uint:Bandwidth Hz|byte:Spreading factor (5-12)|byte:Coding rate (5-8)", true},
    {12, 0, 1, "Transmit power", "sbyte:Power dBm", true},
    {13, 0, 1, "Reset contact path", "key:Public key", true},
    {14, 0, 1, "Set advertised location", "int:Latitude x1000000|int:Longitude x1000000", true},
    {15, 0, 1, "Remove contact", "key:Public key", true},
    {16, 0, 1, "Share contact nearby", "key:Public key", true},
    {17, 11, 1, "Export contact", "bytes:Public key (empty for self)", false},
    {18, 0, 1, "Import contact advert", "bytes:Advert data (hex)", true},
    {19, 255, 1, "Restart radio", "", true},
    {20, 12, 1, "Battery and storage", "", false},
    {21, 0, 1, "Set radio tuning", "uint:RX delay x1000|uint:Airtime factor x1000", true},
    {22, 13, 1, "Firmware information", "", false},
    {23, 14, 1, "Export private identity", "", true},
    {24, 0, 1, "Import private identity", "identity:Private identity (hex)", true},
    {25, 0, 1, "Send raw data", "byte:Encoded path length|bytes:Path followed by payload (hex)", true},
    {26, 6, 1, "Repeater or room login", "key:Public key|text:Password (max 15 bytes)", true},
    {27, 6, 1, "Request remote status", "key:Public key", false},
    {28, 0, 1, "Check room connection", "key:Public key", false},
    {29, 0, 1, "Leave room", "key:Public key", true},
    {30, 3, 1, "Find contact by key", "key:Public key", false},
    {31, 18, 1, "Read channel slot", "byte:Channel slot", false},
    {32, 0, 1, "Create or update channel", "byte:Channel slot|name:Channel name|secret:Channel key (hex)",
     true},
    {33, 19, 1, "Begin signature", "", true},
    {34, 0, 1, "Append signature data", "bytes:Data (hex)", true},
    {35, 20, 1, "Finish signature", "", true},
    {36, 6, 1, "Trace route", "uint:Tag|uint:Auth|byte:Path flags|bytes:Path (hex)", true},
    {37, 0, 1, "Change Bluetooth PIN", "uint:New PIN (0 or 100000-999999)", true},
    {38, 0, 1, "Contact and privacy policies",
     "byte:Manual contacts|byte:Telemetry permissions|byte:Advert location policy|byte:Extra ACKs", true},
    {39, 139, 1, "Read telemetry", "bytes:Public key (empty for local)", false},
    {40, 21, 1, "Read sensor settings", "", false},
    {41, 0, 1, "Set sensor setting", "text:Name:value", true},
    {42, 22, 1, "Last advert route", "zero:Reserved|key:Public key", false},
    {43, 23, 1, "Read radio tuning", "", false},
    {50, 6, 1, "Request remote binary data", "key:Public key|bytes:Request and parameters (hex)", true},
    {51, 255, 1, "Factory reset radio", "", true},
    {52, 6, 1, "Discover contact path", "zero:Reserved|key:Public key", true},
    {54, 0, 8, "Override flood scope",
     "byte:Mode (0 key or clear / 1 unscoped)|bytes:Scope key (hex, empty to clear)", true},
    {55, 0, 8, "Send control data nearby", "bytes:Control data (hex, first byte >=80)", true},
    {56, 24, 8, "Read statistics", "byte:Category (0 core / 1 radio / 2 packets)", false},
    {57, 6, 8, "Anonymous remote request", "key:Public key|bytes:Request (hex)", true},
    {58, 0, 8, "Automatic contact policy", "byte:Type mask|byte:Maximum hops (0-64)", true},
    {59, 25, 8, "Read automatic contact policy", "", false},
    {60, 26, 9, "Allowed repeat frequencies", "", false},
    {61, 0, 10, "Path hash size", "zero:Reserved|byte:Mode (0 / 1 / 2)", true},
    {62, 0, 11, "Send channel data",
     "byte:Channel slot|byte:Encoded path length (255 flood)|bytes:Path (empty for flood)|word:Data "
     "type|bytes:Payload (hex)",
     true},
    {63, 0, 12, "Set default flood scope", "scope:Scope name|secret:Scope key (hex)", true},
    {64, 28, 12, "Read default flood scope", "", false},
    {65, 0, 13, "Send raw LoRa packet", "byte:Priority|bytes:Packet (hex)", true},
    {66, 29, 14, "Local radio console", "text:Command", true},
};
static constexpr size_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
static const Command *command(uint8_t code) {
  for (auto &c : COMMANDS)
    if (c.code == code)
      return &c;
  return nullptr;
}
static bool integer(const char *s, int64_t low, int64_t high, int64_t &out) {
  if (!s || !*s)
    return false;
  char *end = nullptr;
  errno = 0;
  long long value = strtoll(s, &end, 10);
  if (errno || !end || *end || value < low || value > high)
    return false;
  out = value;
  return true;
}
// Shared typed form encoder used by display and host tests. Returns a useful validation error.
static bool build(const Command &c, const char *const *values, size_t count, Frame &f, char *error,
                  size_t cap) {
  f = Frame{};
  f.data[f.len++] = c.code;
  char schema[360];
  snprintf(schema, sizeof(schema), "%s", c.fields);
  char *save = nullptr;
  size_t field = 0;
  for (char *token = strtok_r(schema, "|", &save); token; token = strtok_r(nullptr, "|", &save)) {
    char *colon = strchr(token, ':');
    if (!colon)
      return false;
    *colon = 0;
    if (!strcmp(token, "zero")) {
      if (f.len == MAX_FRAME)
        return false;
      f.data[f.len++] = 0;
      continue;
    }
    if (field >= count) {
      snprintf(error, cap, "Missing %s", colon + 1);
      return false;
    }
    const char *s = values[field++];
    size_t n = 0;
    uint8_t bytes[MAX_FRAME] = {};
    int64_t v = 0;
    if (!strcmp(token, "byte") || !strcmp(token, "sbyte") || !strcmp(token, "word") ||
        !strcmp(token, "uint") || !strcmp(token, "int")) {
      const bool sign = !strcmp(token, "sbyte") || !strcmp(token, "int");
      n = !strcmp(token, "byte") || !strcmp(token, "sbyte") ? 1 : !strcmp(token, "word") ? 2 : 4;
      int64_t low = sign ? (n == 1 ? -128 : INT32_MIN) : 0, high = sign ? (n == 1 ? 127 : INT32_MAX)
                                                                        : (n == 1   ? 255
                                                                           : n == 2 ? 65535
                                                                                    : UINT32_MAX);
      if (!integer(s, low, high, v)) {
        snprintf(error, cap, "Invalid %s", colon + 1);
        return false;
      }
      for (size_t i = 0; i < n; i++)
        bytes[i] = uint8_t(uint64_t(v) >> (8 * i));
    } else if (!strcmp(token, "text") || !strcmp(token, "name") || !strcmp(token, "scope")) {
      const size_t len = strlen(s);
      n = !strcmp(token, "name") ? 32 : !strcmp(token, "scope") ? 31 : len;
      if (len >= sizeof(bytes) || (!strcmp(token, "name") && len > 31) ||
          (!strcmp(token, "scope") && len > 30)) {
        snprintf(error, cap, "%s is too long", colon + 1);
        return false;
      }
      memcpy(bytes, s, len);
    } else {
      if (!unhex(s, bytes, sizeof(bytes), n)) {
        snprintf(error, cap, "Invalid hex: %s", colon + 1);
        return false;
      }
      size_t required = !strcmp(token, "key")                                  ? 32
                        : !strcmp(token, "prefix")                             ? 6
                        : !strcmp(token, "secret")                             ? 16
                        : !strcmp(token, "identity") || !strcmp(token, "path") ? 64
                                                                               : 0;
      if (required && n != required) {
        snprintf(error, cap, "%s needs %u bytes", colon + 1, (unsigned)required);
        return false;
      }
    }
    if (f.len + n > MAX_FRAME) {
      snprintf(error, cap, "Command exceeds radio frame limit");
      return false;
    }
    memcpy(f.data + f.len, bytes, n);
    f.len += n;
  }
  if (c.code == 1) {
    f.len = 8;
    const char name[] = "MeshCrossway";
    memcpy(f.data + 8, name, sizeof(name) - 1);
    f.len += sizeof(name) - 1;
  }
  if (c.code == 22) {
    f.data[1] = 14;
    f.len = 2;
  }
  if (c.code == 39) {
    size_t keylen = f.len - 1;
    if (keylen != 0 && keylen != 32) {
      snprintf(error, cap, "Public key needs 32 bytes or empty for local");
      return false;
    }
    memmove(f.data + 4, f.data + 1, keylen);
    memset(f.data + 1, 0, 3);
    f.len = 4 + keylen;
  }
  if (c.code == 63 && f.data[1] == 0)
    f.len = 1;
  if (c.code == 19) {
    memcpy(f.data + 1, "reboot", 6);
    f.len = 7;
  }
  if (c.code == 51) {
    memcpy(f.data + 1, "reset", 5);
    f.len = 6;
  }
  const uint8_t *p = f.data;
  bool valid = true;
  switch (c.code) {
  case 2:
    valid = f.len >= 14 && p[1] <= 3 && p[1] != 2;
    break;
  case 3:
    valid = f.len >= 8 && p[1] == 0;
    break;
  case 7:
    valid = p[1] <= 1;
    break;
  case 8:
    valid = f.len >= 2 && f.len <= 32;
    break;
  case 11:
    valid = u32(p + 1) >= 150000 && u32(p + 1) <= 2500000 && u32(p + 5) >= 7000 && u32(p + 5) <= 500000 &&
            p[9] >= 5 && p[9] <= 12 && p[10] >= 5 && p[10] <= 8;
    break;
  case 12:
    valid = (int8_t)p[1] >= -9;
    break;
  case 14:
    valid = (int32_t)u32(p + 1) >= -90000000 && (int32_t)u32(p + 1) <= 90000000 &&
            (int32_t)u32(p + 5) >= -180000000 && (int32_t)u32(p + 5) <= 180000000;
    break;
  case 17:
    valid = f.len == 1 || f.len == 33;
    break;
  case 18:
    valid = f.len > 98;
    break;
  case 25:
    valid = f.len >= 6 && (p[1] >> 6) != 3 && f.len >= 6 + (p[1] & 63) * ((p[1] >> 6) + 1);
    break;
  case 26:
    valid = f.len >= 33 && f.len <= 48;
    break;
  case 34:
    valid = f.len > 1;
    break;
  case 36:
    valid = f.len > 10 && f.len - 10 <= 64 && (p[9] & 3) <= 2 && ((f.len - 10) % (1u << (p[9] & 3))) == 0;
    break;
  case 37:
    valid = u32(p + 1) == 0 || (u32(p + 1) >= 100000 && u32(p + 1) <= 999999);
    break;
  case 41:
    valid = f.len >= 4 && strchr(values[0], ':');
    break;
  case 50:
  case 57:
    valid = f.len >= 34;
    break;
  case 54:
    valid = p[1] <= 1 && ((p[1] == 1 && f.len == 2) || (p[1] == 0 && (f.len == 2 || f.len == 18)));
    break;
  case 55:
    valid = f.len >= 2 && (p[1] & 128);
    break;
  case 56:
    valid = p[1] <= 2;
    break;
  case 58:
    valid = p[2] <= 64;
    break;
  case 61:
    valid = p[2] <= 2;
    break;
  case 62: {
    size_t path = p[2] == 255 ? 0 : (p[2] & 63) * ((p[2] >> 6) + 1);
    size_t actual = 0;
    uint8_t check[64];
    if (!unhex(values[2], check, sizeof(check), actual) || actual != path) {
      snprintf(error, cap, "Path bytes do not match encoded length");
      return false;
    }
    valid = path <= 64 && f.len >= 5 + path && (p[2] == 255 || (p[2] >> 6) != 3) && u16(p + 3 + path) != 0;
    break;
  }
  case 65:
    valid = f.len >= 4;
    break;
  case 66:
    valid = f.len >= 3;
    break;
  default:
    break;
  }
  if (!valid) {
    snprintf(error, cap, "Parameters outside supported range");
    return false;
  }
  return true;
}
// Cayenne LPP values are big endian, unlike the Companion envelope.
static void telemetry_text(const uint8_t *p, size_t n, char *out, size_t cap) {
  size_t pos = 0, used = 0;
  if (!cap)
    return;
  out[0] = 0;
  while (pos + 2 <= n && used + 80 < cap) {
    uint8_t channel = p[pos++], type = p[pos++];
    size_t size = 0;
    double scale = 1.0;
    bool sign = false;
    const char *unit = "";
    switch (type) {
    case 0:
    case 1:
    case 102:
    case 120:
    case 142:
      size = 1;
      break;
    case 2:
    case 3:
      size = 2;
      scale = .01;
      sign = true;
      break;
    case 100:
    case 118:
    case 133:
      size = 4;
      break;
    case 101:
      size = 2;
      unit = " lux";
      break;
    case 103:
      size = 2;
      scale = .1;
      sign = true;
      unit = " C";
      break;
    case 104:
      size = 1;
      scale = .5;
      unit = " %";
      break;
    case 115:
      size = 2;
      scale = .1;
      unit = " hPa";
      break;
    case 116:
      size = 2;
      scale = .01;
      unit = " V";
      break;
    case 117:
      size = 2;
      scale = .001;
      unit = " A";
      break;
    case 121:
      size = 2;
      sign = true;
      unit = " m";
      break;
    case 125:
      size = 2;
      unit = " ppm";
      break;
    case 128:
      size = 2;
      unit = " W";
      break;
    case 130:
    case 131:
      size = 4;
      scale = .001;
      unit = type == 130 ? " m" : " kWh";
      break;
    case 132:
      size = 2;
      unit = " deg";
      break;
    case 113:
    case 134:
      size = 6;
      break;
    case 135:
      size = 3;
      break;
    case 136:
      size = 9;
      break;
    default:
      snprintf(out + used, cap - used, "Channel %u: unsupported sensor %u (raw data available)", channel,
               type);
      return;
    }
    if (pos + size > n) {
      snprintf(out + used, cap - used, "Truncated telemetry");
      return;
    }
    if (size <= 4) {
      uint32_t raw = 0;
      for (size_t i = 0; i < size; i++)
        raw = (raw << 8) | p[pos + i];
      int64_t value = sign && size == 2 ? (int16_t)raw : (int64_t)raw;
      int count = snprintf(out + used, cap - used, "Channel %u / type %u: %.3f%s\n", channel, type,
                           value * scale, unit);
      if (count > 0)
        used += (size_t)count;
    } else if (type == 136) {
      double coords[3];
      for (int k = 0; k < 3; k++) {
        uint32_t raw =
            (uint32_t(p[pos + k * 3]) << 16) | (uint32_t(p[pos + k * 3 + 1]) << 8) | p[pos + k * 3 + 2];
        int32_t v = raw & 0x800000 ? (int32_t)(raw | 0xff000000) : (int32_t)raw;
        coords[k] = v / (k == 2 ? 100.0 : 10000.0);
      }
      int count = snprintf(out + used, cap - used, "GPS %u: %.5f, %.5f / %.2f m\n", channel, coords[0],
                           coords[1], coords[2]);
      if (count > 0)
        used += (size_t)count;
    } else {
      char bytes[32];
      hex(p + pos, size, bytes, sizeof(bytes));
      int count = snprintf(out + used, cap - used, "Channel %u / type %u: %s\n", channel, type, bytes);
      if (count > 0)
        used += (size_t)count;
    }
    if (used >= cap)
      used = cap - 1;
    pos += size;
  }
}
static bool remote_command(uint8_t code) {
  return code == 26 || code == 27 || code == 36 || code == 39 || code == 50 || code == 52 || code == 57;
}
struct Contact {
  uint8_t key[32] = {}, type = 0, flags = 0, path_len = 255, path[64] = {};
  char name[33] = {};
  uint32_t advert = 0, lastmod = 0;
  int32_t lat = 0, lon = 0;
};
struct Channel {
  bool loaded = false;
  char name[33] = {};
  uint8_t secret[16] = {};
};
enum Delivery : uint8_t { RECEIVED, QUEUED, ACCEPTED, DELIVERED, REJECTED, UNCERTAIN };
struct Message {
  uint32_t id = 0, timestamp = 0, ack = 0, ack_deadline = 0;
  bool channel = false, outgoing = false, read = false;
  uint8_t slot = 0, key[6] = {}, signer[4] = {}, type = 0, path = 255;
  int8_t snr = 0;
  Delivery delivery = RECEIVED;
  char text[MAX_FRAME + 1] = {};
};
struct Pending {
  Frame frame;
  int32_t message = -1;
};
class Client {
public:
  Contact contacts[MAX_CONTACTS];
  Channel channels[MAX_CHANNELS];
  Message messages[MAX_MESSAGES];
  uint16_t contact_count = 0, channel_count = 0, message_count = 0, max_contacts = 0;
  uint8_t protocol = 0;
  bool connected = false, initialized = false, fault = false, busy = false, written = false,
       response_done = false, waiting_messages = false;
  uint16_t payload_limit = 0, battery_mv = 0;
  uint32_t used_kb = 0, total_kb = 0, generation = 0, rx_count = 0, tx_count = 0, invalid = 0;
  char name[33] = {}, firmware[21] = {}, model[41] = {}, status[128] = "Connect a MeshCore Companion radio",
       result[800] = {};
  uint8_t public_key[32] = {}, tx_power = 0, max_power = 0, sf = 0, cr = 0;
  uint32_t frequency = 0, bandwidth = 0;
  int32_t latitude = 0, longitude = 0;
  Frame last_frame;
  uint32_t clock = 0, clock_at = 0;
  Pending queue[12], active;
  uint8_t qhead = 0, qcount = 0;
  uint32_t deadline = 0, last_send = 0, next_id = 1, now_ms = 0, last_timestamp = 0;
  uint8_t remote_pending = 0, remote_key[6] = {};
  uint32_t remote_tag = 0, remote_deadline = 0;
  uint16_t bootstrap = 0, channel_next = 0;
  bool contacts_started = false;
  uint32_t contacts_seen = 0, contacts_expected = 0;
  void change(const char *s) {
    string_copy(status, sizeof(status), (const uint8_t *)s, strlen(s));
    ++generation;
  }
  void fail(const char *s) {
    fault = true;
    initialized = false;
    busy = false;
    qcount = 0;
    for (auto &m : messages)
      if (m.outgoing && m.delivery == QUEUED)
        m.delivery = UNCERTAIN;
    change(s);
  }
  void detach() {
    connected = false;
    remote_pending = 0;
    initialized = false;
    busy = false;
    qcount = 0;
    for (auto &m : messages)
      if (m.outgoing && (m.delivery == QUEUED || m.delivery == ACCEPTED))
        m.delivery = UNCERTAIN;
    if (!fault)
      change("Radio disconnected; history is from this session");
  }
  void attach(uint16_t limit) {
    this->~Client();
    new (this) Client();
    connected = true;
    payload_limit = limit;
    change("Reading MeshCore identity...");
  }
  bool enqueue(const Frame &f, int32_t message = -1) {
    if (!connected || fault || !f.len || f.len > payload_limit || qcount >= 12)
      return false;
    if (f.data[0] == 3 && (f.len < 8 || size_t(f.len - 7) > text_limit(true)))
      return false;
    const Command *c = command(f.data[0]);
    if (!c || (protocol && c->min_version > protocol))
      return false;
    if (f.data[0] == 2 && find_contact(f.data + 7, 6) == -2)
      return false;
    if ((f.data[0] == 31 || f.data[0] == 32) && protocol && f.data[1] >= channel_count)
      return false;
    if (f.data[0] == 12 && protocol && (int8_t)f.data[1] > (int8_t)max_power)
      return false;
    queue[(qhead + qcount) % 12].frame = f;
    queue[(qhead + qcount) % 12].message = message;
    ++qcount;
    return true;
  }
  bool simple(uint8_t code, int arg = -1) {
    Frame f;
    char error[90];
    const Command *c = command(code);
    if (!c)
      return false;
    if (arg >= 0) {
      f.len = 2;
      f.data[0] = code;
      f.data[1] = arg;
    } else if (!build(*c, nullptr, 0, f, error, sizeof(error)))
      return false;
    return enqueue(f);
  }
  uint32_t timestamp() { return clock + (now_ms - clock_at) / 1000; }
  Message *by_id(uint32_t id) {
    for (auto &m : messages)
      if (m.id == id)
        return &m;
    return nullptr;
  }
  Message &append() {
    Message &m = messages[(next_id - 1) % MAX_MESSAGES];
    m = Message{};
    m.id = next_id++;
    if (message_count < MAX_MESSAGES)
      ++message_count;
    return m;
  }
  int find_contact(const uint8_t *key, size_t n = 32) {
    int found = -1;
    for (int i = 0; i < contact_count; i++)
      if (!memcmp(contacts[i].key, key, n)) {
        if (found >= 0)
          return -2;
        found = i;
      }
    return found;
  }
  size_t text_limit(bool group) const {
    size_t prefix = group ? strlen(name) + 2 : 0;
    size_t limit = prefix < 160 ? 160 - prefix : 0;
    if (limit > MAX_TEXT)
      limit = MAX_TEXT;
    size_t offset = group ? 7 : 13;
    size_t wire = payload_limit > offset ? payload_limit - offset : 0;
    return limit < wire ? limit : wire;
  }
  bool send_text(bool group, uint16_t target, const char *text) {
    size_t n = strlen(text);
    if (!initialized || !n || n > text_limit(group) || timestamp() < 1577836800u)
      return false;
    if (group ? (target >= channel_count || !channels[target].loaded || !channels[target].name[0])
              : target >= contact_count)
      return false;
    Frame f;
    f.data[0] = group ? 3 : 2;
    f.data[1] = 0;
    f.data[2] = group ? target : 0;
    uint32_t stamp = timestamp();
    if (stamp <= last_timestamp)
      stamp = last_timestamp + 1;
    put32(f.data + 3, stamp);
    size_t offset = 7;
    if (!group) {
      if (find_contact(contacts[target].key, 6) != target)
        return false;
      memcpy(f.data + 7, contacts[target].key, 6);
      offset = 13;
    }
    memcpy(f.data + offset, text, n);
    f.len = offset + n;
    if (f.len > payload_limit || qcount >= 12)
      return false;
    const uint32_t id = next_id;
    if (!enqueue(f, id))
      return false;
    last_timestamp = stamp;
    Message &m = append();
    m.timestamp = stamp;
    m.outgoing = true;
    m.channel = group;
    m.slot = target;
    m.delivery = QUEUED;
    if (!group)
      memcpy(m.key, contacts[target].key, 6);
    snprintf(m.text, sizeof(m.text), "%s", text);
    ++generation;
    return true;
  }
  // Returns one complete frame. Caller must provide its ATT write completion through write_done().
  bool next(uint32_t now, Frame &f) {
    now_ms = now;
    if (!connected || fault)
      return false;
    for (auto &m : messages)
      if (m.outgoing && m.delivery == ACCEPTED && m.ack && m.ack_deadline &&
          (int32_t)(now - m.ack_deadline) >= 0) {
        m.delivery = UNCERTAIN;
        ++generation;
      }
    if (remote_pending && (int32_t)(now - remote_deadline) >= 0) {
      remote_pending = 0;
      snprintf(result, sizeof(result), "Remote request timed out. No automatic retry.");
      change("Remote reply not received");
    }
    if (busy) {
      if ((int32_t)(now - deadline) >= 0)
        fail("Command timed out; reconnect before retrying (delivery unknown)");
      return false;
    }
    if (now - last_send < 80)
      return false;
    if (!qcount && !initialized) {
      if (bootstrap == 0)
        simple(1);
      else if (bootstrap == 1)
        simple(22);
      else if (bootstrap == 2)
        simple(5);
      else if (bootstrap == 3) {
        contact_count = 0;
        simple(4);
      } else if (channel_next < channel_count) {
        simple(31, channel_next++);
      } else if (bootstrap == 4)
        simple(20);
      else {
        initialized = true;
        waiting_messages = true;
        change("MeshCore ready");
      }
      if (bootstrap < 4)
        ++bootstrap;
      else if (channel_next >= channel_count && qcount && queue[qhead].frame.data[0] == 20)
        ++bootstrap;
    }
    if (!qcount && initialized && waiting_messages) {
      waiting_messages = false;
      simple(10);
    }
    if (!qcount)
      return false;
    if (remote_pending && remote_command(queue[qhead].frame.data[0]))
      return false;
    active = queue[qhead];
    if (active.frame.data[0] == 4) {
      contact_count = 0;
      contacts_started = false;
    }
    qhead = (qhead + 1) % 12;
    --qcount;
    busy = true;
    written = false;
    response_done = false;
    deadline = now + (active.frame.data[0] == 4 ? 60000 : 10000);
    last_send = now;
    ++tx_count;
    f = active.frame;
    const Command *c = command(f.data[0]);
    snprintf(result, sizeof(result), "%s\nWaiting for radio response...", c ? c->name : "Command");
    change(c ? c->name : "Command");
    return true;
  }
  void write_done(bool ok) {
    if (!busy)
      return;
    if (!ok) {
      fail("BLE write failed; reconnect (delivery unknown)");
      return;
    }
    written = true;
    if (response_done)
      busy = false;
  }
  bool parse_contact(const uint8_t *p, size_t n) {
    if (n < 148)
      return false;
    int i = find_contact(p + 1);
    if (i < 0) {
      if (contact_count == MAX_CONTACTS)
        return false;
      i = contact_count++;
    }
    Contact &c = contacts[i];
    memcpy(c.key, p + 1, 32);
    c.type = p[33];
    c.flags = p[34];
    c.path_len = p[35];
    memcpy(c.path, p + 36, 64);
    string_copy(c.name, sizeof(c.name), p + 100, 32);
    c.advert = u32(p + 132);
    c.lat = (int32_t)u32(p + 136);
    c.lon = (int32_t)u32(p + 140);
    c.lastmod = n >= 152 ? u32(p + 148) : c.advert;
    return true;
  }
  bool receive(const uint8_t *p, size_t n, uint32_t now) {
    now_ms = now;
    if (!connected || !n || n > MAX_FRAME) {
      ++invalid;
      return false;
    }
    ++rx_count;
    uint8_t code = p[0];
    bool valid = true;
    char description[600] = {};
    switch (code) {
    case 0:
      valid = n == 1;
      snprintf(description, sizeof(description), "Accepted by radio (not a delivery confirmation)");
      break;
    case 1:
      valid = n >= 2;
      if (valid)
        snprintf(description, sizeof(description), "Radio error %u: %s", p[1],
                 p[1] == 1   ? "unsupported command"
                 : p[1] == 2 ? "not found"
                 : p[1] == 3 ? "table full"
                 : p[1] == 4 ? "bad state"
                 : p[1] == 5 ? "file I/O"
                             : "invalid argument");
      break;
    case 2:
      valid = n >= 5;
      if (valid) {
        contacts_started = true;
        contacts_seen = 0;
        contacts_expected = u32(p + 1);
      }
      break;
    case 3:
    case 138:
      valid = parse_contact(p, n);
      if (valid && code == 3)
        ++contacts_seen;
      break;
    case 4:
      valid = n >= 5 && contacts_started && contacts_seen == contacts_expected;
      if (valid)
        contacts_started = false;
      break;
    case 5:
      valid = n >= 58;
      if (valid) {
        tx_power = p[2];
        max_power = p[3];
        memcpy(public_key, p + 4, 32);
        latitude = (int32_t)u32(p + 36);
        longitude = (int32_t)u32(p + 40);
        frequency = u32(p + 48);
        bandwidth = u32(p + 52);
        sf = p[56];
        cr = p[57];
        string_copy(name, sizeof(name), p + 58, n - 58);
        snprintf(description, sizeof(description),
                 "%s\nFrequency: %lu kHz\nBandwidth: %lu Hz\nSF %u / CR %u / Power %d dBm", name,
                 (unsigned long)frequency, (unsigned long)bandwidth, sf, cr, (int8_t)tx_power);
      }
      break;
    case 6:
      valid = n >= 10;
      if (valid) {
        Message *m = by_id(active.message);
        if (m && busy && active.frame.data[0] == 2) {
          m->delivery = ACCEPTED;
          m->ack = u32(p + 2);
          uint32_t timeout = u32(p + 6);
          if (timeout > 600000)
            timeout = 600000;
          m->ack_deadline = now + timeout;
        }
        snprintf(description, sizeof(description),
                 "Radio queued transmission / %s\nExpected ACK: %08lx\nEstimated wait: %lu ms",
                 p[1] ? "flood" : "direct", (unsigned long)u32(p + 2), (unsigned long)u32(p + 6));
      }
      break;
    case 7:
    case 8:
    case 16:
    case 17: {
      bool group = code == 8 || code == 17, v3 = code == 16 || code == 17;
      size_t start = v3 ? 4 : 1, meta = group ? 3 : 8;
      valid = n >= start + meta + 4;
      if (valid) {
        size_t typepos = start + (group ? 2 : 7);
        uint8_t type = p[typepos];
        size_t offset = start + meta + 4 + (type == 2 ? 4 : 0);
        valid = n >= offset && (!group || p[start] < channel_count);
        if (valid) {
          bool duplicate = false;
          for (auto &m : messages)
            if (m.id && !m.outgoing && m.channel == group && m.timestamp == u32(p + start + meta) &&
                (group ? m.slot == p[start] : !memcmp(m.key, p + start, 6)) &&
                !strncmp(m.text, (const char *)p + offset, n - offset) && strlen(m.text) == n - offset)
              duplicate = true;
          if (!duplicate) {
            Message &m = append();
            m.channel = group;
            m.slot = group ? p[start] : 0;
            if (!group)
              memcpy(m.key, p + start, 6);
            m.type = type;
            if (type == 2)
              memcpy(m.signer, p + start + meta + 4, 4);
            m.path = p[start + (group ? 1 : 6)];
            m.snr = v3 ? (int8_t)p[1] : 0;
            m.timestamp = u32(p + start + meta);
            string_copy(m.text, sizeof(m.text), p + offset, n - offset);
          }
          waiting_messages = true;
        }
      }
      break;
    }
    case 9:
      valid = n >= 5;
      if (valid) {
        clock = u32(p + 1);
        clock_at = now;
        snprintf(description, sizeof(description), "Radio clock: %lu Unix seconds", (unsigned long)clock);
      }
      break;
    case 10:
      valid = n == 1;
      break;
    case 12:
      valid = n >= 3;
      if (valid) {
        battery_mv = u16(p + 1);
        if (n >= 11) {
          used_kb = u32(p + 3);
          total_kb = u32(p + 7);
        }
        snprintf(description, sizeof(description), "Battery: %u mV\nStorage: %lu / %lu KB", battery_mv,
                 (unsigned long)used_kb, (unsigned long)total_kb);
      }
      break;
    case 13:
      valid = n >= 4 && p[1] >= 3;
      if (valid) {
        protocol = p[1];
        max_contacts = uint16_t(p[2]) * 2;
        channel_count = p[3];
        if (n >= 60)
          string_copy(model, sizeof(model), p + 20, 40);
        if (n >= 80)
          string_copy(firmware, sizeof(firmware), p + 60, 20);
        snprintf(description, sizeof(description),
                 "%s\nFirmware: %s\nProtocol: %u\nContacts: %u / Channel slots: %u\nBluetooth PIN: hidden",
                 model, firmware, protocol, max_contacts, channel_count);
      }
      break;
    case 14:
      valid = n >= 65;
      snprintf(description, sizeof(description),
               "Private identity received; use explicit export view. Do not share screenshots.");
      break;
    case 15:
      snprintf(description, sizeof(description), "Disabled in the radio firmware build");
      break;
    case 18:
      valid = n >= 50 && p[1] < channel_count;
      if (valid) {
        Channel &c = channels[p[1]];
        c.loaded = true;
        string_copy(c.name, sizeof(c.name), p + 2, 32);
        memcpy(c.secret, p + 34, 16);
        snprintf(description, sizeof(description), "Channel %u: %s\nKey hidden", p[1], c.name);
      }
      break;
    case 19:
      valid = n >= 6;
      break;
    case 20:
      valid = n >= 65;
      break;
    case 21:
    case 29:
      valid = n >= 1;
      if (valid)
        string_copy(description, sizeof(description), p + 1, n - 1);
      break;
    case 22:
      valid = n >= 6;
      break;
    case 23:
      valid = n >= 9;
      if (valid)
        snprintf(description, sizeof(description), "RX delay x1000: %lu\nAirtime factor x1000: %lu",
                 (unsigned long)u32(p + 1), (unsigned long)u32(p + 5));
      break;
    case 24:
      valid = n >= 2 && (p[1] == 0 ? n >= 11 : p[1] == 1 ? n >= 14 : p[1] == 2 ? n >= 30 : false);
      if (valid) {
        if (p[1] == 0)
          snprintf(description, sizeof(description),
                   "Battery %u mV / uptime %lu s\nError flags %u / TX queue %u", u16(p + 2),
                   (unsigned long)u32(p + 4), u16(p + 8), p[10]);
        else if (p[1] == 1)
          snprintf(description, sizeof(description),
                   "Noise %d dBm / RSSI %d / SNR %.2f\nTX airtime %lu s / RX %lu s", (int16_t)u16(p + 2),
                   (int8_t)p[4], (int8_t)p[5] / 4.0, (unsigned long)u32(p + 6), (unsigned long)u32(p + 10));
        else
          snprintf(
              description, sizeof(description),
              "Packets RX %lu / TX %lu\nTX flood %lu / direct %lu\nRX flood %lu / direct %lu / errors %lu",
              (unsigned long)u32(p + 2), (unsigned long)u32(p + 6), (unsigned long)u32(p + 10),
              (unsigned long)u32(p + 14), (unsigned long)u32(p + 18), (unsigned long)u32(p + 22),
              (unsigned long)u32(p + 26));
      }
      break;
    case 25:
      valid = n >= 2;
      if (valid)
        snprintf(description, sizeof(description), "Auto-add mask %u / max hops %u", p[1], n >= 3 ? p[2] : 0);
      break;
    case 26:
      valid = n >= 1 && ((n - 1) % 8) == 0;
      break;
    case 27:
      valid = n >= 9;
      if (valid)
        waiting_messages = true;
      break;
    case 28:
      valid = n == 1 || n >= 48;
      break;
    case 128:
    case 129:
      valid = n >= 33;
      break;
    case 130:
      valid = n >= 9;
      if (valid)
        for (auto &m : messages)
          if (m.id && m.outgoing && m.ack && m.ack == u32(p + 1)) {
            m.delivery = DELIVERED;
            m.ack_deadline = 0;
          }
      break;
    case 131:
      valid = n == 1;
      if (valid)
        waiting_messages = true;
      break;
    case 139:
      valid = n >= 8;
      if (valid)
        telemetry_text(p + 8, n - 8, description, sizeof(description));
      break;
    case 140:
      valid = n >= 6;
      break;
    case 133:
    case 134:
    case 135:
    case 141:
      valid = n >= 8;
      break;
    case 143:
      valid = n >= 33;
      if (valid) {
        int i = find_contact(p + 1);
        if (i >= 0) {
          contacts[i] = contacts[--contact_count];
        }
      }
      break;
    default:
      valid = code >= 128 || code == 11;
      break;
    }
    if (!valid) {
      ++invalid;
      fail("Malformed or unsupported radio response; reconnect");
      return false;
    }
    ++generation;
    // Async observations never acknowledge a command (except a specifically requested self telemetry
    // response).
    bool matches = false;
    if (busy && !response_done) {
      const uint8_t cmd = active.frame.data[0];
      const Command *c = command(cmd);
      matches = code == 1 || code == 15 || (c && code == c->response);
      if (cmd == 4) {
        matches = code == 1 || code == 15 || code == 4;
        if (code == 2 || code == 3)
          deadline = now + 10000;
      }
      if (cmd == 10)
        matches = code == 1 || code == 15 || code == 7 || code == 8 || code == 10 || code == 16 ||
                  code == 17 || code == 27;
      if (cmd == 31 && code == 18)
        matches = p[1] == active.frame.data[1];
      if (cmd == 56 && code == 24)
        matches = p[1] == active.frame.data[1];
      if (cmd == 39) {
        if (active.frame.len == 4)
          matches = code == 1 || code == 15 || (code == 139 && n >= 8 && !memcmp(p + 2, public_key, 6));
        else
          matches = code == 1 || code == 15 || code == 6;
      }
      if (matches) {
        bool error = code == 1 || code == 15;
        if (!error && code == 6 && remote_command(cmd)) {
          remote_pending = cmd;
          remote_tag = u32(p + 2);
          uint32_t timeout = u32(p + 6);
          if (timeout < 5000)
            timeout = 5000;
          if (timeout > 600000)
            timeout = 600000;
          remote_deadline = now + timeout;
          size_t key_offset = cmd == 39 ? 4 : cmd == 52 ? 2 : 1;
          if (cmd != 36)
            memcpy(remote_key, active.frame.data + key_offset, 6);
        }
        Message *m = by_id(active.message);
        if (m) {
          if (error)
            m->delivery = REJECTED;
          else if (cmd == 3)
            m->delivery = ACCEPTED;
        }
        if (!error && code == 0) {
          if (cmd == 6) {
            clock = u32(active.frame.data + 1);
            clock_at = now;
          }
          if (cmd == 8)
            string_copy(name, sizeof(name), active.frame.data + 1, active.frame.len - 1);
          if (cmd == 32) {
            uint8_t i = active.frame.data[1];
            channels[i].loaded = true;
            string_copy(channels[i].name, 33, active.frame.data + 2, 32);
            memcpy(channels[i].secret, active.frame.data + 34, 16);
          }
        }
        if (error && !initialized && (cmd == 1 || cmd == 22 || cmd == 4)) {
          fail(description);
          return false;
        }
        if (!error && code == 0) {
          if (cmd == 11) {
            frequency = u32(active.frame.data + 1);
            bandwidth = u32(active.frame.data + 5);
            sf = active.frame.data[9];
            cr = active.frame.data[10];
          }
          if (cmd == 12)
            tx_power = active.frame.data[1];
          if (cmd == 14) {
            latitude = (int32_t)u32(active.frame.data + 1);
            longitude = (int32_t)u32(active.frame.data + 5);
          }
          if (cmd == 15) {
            int i = find_contact(active.frame.data + 1);
            if (i >= 0)
              contacts[i] = contacts[--contact_count];
          }
          if (cmd == 9 || cmd == 13 || cmd == 18)
            simple(4);
        }
        response_done = true;
        if (written)
          busy = false;
        last_frame.len = n;
        memcpy(last_frame.data, p, n);
        if (code == 13 && n >= 8)
          memset(last_frame.data + 4, 0, 4);
        if (!description[0]) {
          char data[360];
          hex(p + 1, n - 1, data, sizeof(data));
          snprintf(description, sizeof(description), "Response %u / %u bytes\n%s", code, (unsigned)n, data);
        }
        snprintf(result, sizeof(result), "%s\n%s", command(cmd)->name, description);
        change(error ? description : "Command completed");
      }
    }
    if (code >= 128) {
      const bool remote_match =
          remote_pending && ((code == 140 && n >= 6 && u32(p + 2) == remote_tag) ||
                             (code == 137 && n >= 12 && u32(p + 4) == remote_tag) ||
                             ((code == 133 || code == 134 || code == 135 || code == 139 || code == 141) &&
                              n >= 8 && !memcmp(p + 2, remote_key, 6)));
      if (remote_match)
        remote_pending = 0;
      const char *event = code == 130   ? "Delivery acknowledged"
                          : code == 133 ? "Remote login succeeded"
                          : code == 134 ? "Remote login failed"
                          : code == 135 ? "Remote status received"
                          : code == 137 ? "Trace response received"
                          : code == 139 ? "Telemetry received"
                          : code == 141 ? "Path discovery received"
                          : code == 144 ? "Radio contacts full"
                                        : "Radio event received";
      if (code != 131 && code != 128 && code != 129) {
        char data[360];
        hex(p + 1, n - 1, data, sizeof(data));
        snprintf(result, sizeof(result), "%s\n%s", event, description[0] ? description : data);
      }
      if (code == 128 || code == 129 || code == 138 || code == 143) {
        bool queued = false;
        for (int i = 0; i < qcount; i++)
          if (queue[(qhead + i) % 12].frame.data[0] == 4)
            queued = true;
        if (!queued && initialized)
          simple(4);
      }
    }
    return true;
  }
};
} // namespace meshmc
