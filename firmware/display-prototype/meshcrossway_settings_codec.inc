// Included within meshmt after the bounded protobuf Reader/Writer definitions.
namespace settings {
static uint8_t wire(uint8_t t) {
  return t == 1 || t == 6 || t == 16    ? 1
         : t == 2 || t == 7 || t == 15  ? 5
         : t == 9 || t == 11 || t == 12 ? 2
                                        : 0;
}
static bool append(Frame &to, const uint8_t *p, size_t n) {
  if (n > MAX_FRAME - to.len)
    return false;
  if (n)
    memcpy(to.data + to.len, p, n);
  to.len += n;
  return true;
}
static View child(View v, uint32_t tag) {
  Reader r(v);
  Field f;
  View found;
  while (r.next(f))
    if (f.tag == tag && f.wire == 2)
      found = f.bytes;
  return found;
}
static View parent(View v, const Spec &s) {
  for (unsigned i = 0; i + 1 < s.depth; ++i)
    v = child(v, s.path[i]);
  return v;
}
static bool enum_value(const Spec &s, int64_t n) {
  if (!s.choices[0])
    return true;
  const char *p = s.choices;
  while (*p) {
    char *end = nullptr;
    long v = strtol(p, &end, 10);
    if (end != p && *end == ':' && v == n)
      return true;
    p = strchr(p, '|');
    if (!p)
      break;
    ++p;
  }
  return false;
}
static bool atom(const Spec &s, const char *text, Frame &out) {
  Writer w(out);
  const unsigned tag = s.path[s.depth - 1];
  const uint8_t wt = wire(s.type);
  if (wt == 2) {
    if (s.type == 9) {
      const size_t n = strlen(text);
      if (!utf8(text, n))
        return false;
      w.bytes(tag, text, n);
      return w.ok;
    }
    // Bytes and advanced repeated-message fields use exact hex, never JSON
    // guessed into protobuf.
    const size_t n = strlen(text);
    if (n % 2 || n / 2 > MAX_FRAME - 8)
      return false;
    w.varint((uint64_t(tag) << 3) | 2);
    w.varint(n / 2);
    for (size_t i = 0; i < n; i += 2) {
      auto digit = [](char c) -> int {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                      : -1;
      };
      int a = digit(text[i]), b = digit(text[i + 1]);
      if (a < 0 || b < 0)
        return false;
      w.byte(uint8_t(a * 16 + b));
    }
    if (s.type == 11) {
      Reader r({out.data, out.len});
      Field f;
      if (!r.next(f) || !valid(f.bytes))
        return false;
    }
    return w.ok;
  }
  if (!*text)
    return false;
  char *end = nullptr;
  uint64_t value = 0;
  errno = 0;
  if (s.type == 1 || s.type == 2) {
    double d = strtod(text, &end);
    if (errno || !end || *end || !isfinite(d))
      return false;
    if (s.type == 2) {
      float f = float(d);
      if (!isfinite(f))
        return false;
      uint32_t b;
      memcpy(&b, &f, 4);
      value = b;
    } else
      memcpy(&value, &d, 8);
  } else if (s.type == 4 || s.type == 6 || s.type == 7 || s.type == 13) {
    if (*text == '-')
      return false;
    value = strtoull(text, &end, 10);
    if (errno || !end || *end ||
        ((s.type == 7 || s.type == 13) && value > UINT32_MAX))
      return false;
  } else {
    int64_t n = strtoll(text, &end, 10);
    if (errno || !end || *end)
      return false;
    if ((s.type == 5 || s.type == 14 || s.type == 15 || s.type == 17) &&
        (n < INT32_MIN || n > INT32_MAX))
      return false;
    if (s.type == 8 && (n < 0 || n > 1))
      return false;
    if (s.type == 14 && !enum_value(s, n))
      return false;
    value = (s.type == 17 || s.type == 18)
                ? (uint64_t(n) << 1) ^ uint64_t(n >> 63)
                : uint64_t(n);
  }
  w.varint((uint64_t(tag) << 3) | wt);
  if (wt == 0)
    w.varint(value);
  else
    for (unsigned i = 0; i < (wt == 1 ? 8u : 4u); ++i)
      w.byte(uint8_t(value >> (8 * i)));
  return w.ok;
}
static bool encode(const Spec &s, const char *input, Frame &encoded,
                   Frame &temporary) {
  encoded.len = 0;
  if (!s.repeated)
    return atom(s, input, encoded);
  if (!*input)
    return true;
  char text[1025];
  if (strlen(input) >= sizeof(text))
    return false;
  strcpy(text, input);
  char *next = text;
  while (next) {
    char *p = next;
    next = strchr(p, s.type == 9 ? '\n' : ',');
    if (next)
      *next++ = 0;
    if (!atom(s, p, temporary) ||
        !append(encoded, temporary.data, temporary.len))
      return false;
  }
  return true;
}
static bool replace(View source, unsigned tag, const Frame &replacement,
                    Frame &out) {
  out.len = 0;
  Reader r(source);
  Field f;
  size_t start = 0;
  while (r.next(f)) {
    if (f.tag != tag && !append(out, source.data + start, r.offset - start))
      return false;
    start = r.offset;
  }
  return r.ok && append(out, replacement.data, replacement.len);
}
// Caller supplies PSRAM work frames: no recursive 512-byte stack allocations.
static bool patch(View source, const Spec &s, const char *input, Frame &out,
                  Frame work[6]) {
  if (s.readonly || !s.depth || s.depth > 4)
    return false;
  if (!encode(s, input, work[0], work[1]))
    return false;
  View parents[4];
  parents[0] = source;
  for (unsigned i = 1; i < s.depth; ++i)
    parents[i] = child(parents[i - 1], s.path[i - 1]);
  for (int i = int(s.depth) - 1; i >= 0; --i) {
    if (!replace(parents[i], s.path[i], work[0], work[1]))
      return false;
    if (i) {
      Writer w(work[0]);
      w.message(s.path[i - 1], work[1]);
      if (!w.ok)
        return false;
    }
  }
  out = work[1];
  return true;
}
static bool atom_text(const Spec &s, const Field &f, char *out, size_t cap) {
  if (f.wire != wire(s.type))
    return false;
  int n = 0;
  if (f.wire == 2) {
    if (s.type == 9) {
      if (f.bytes.len >= cap || !utf8((const char *)f.bytes.data, f.bytes.len))
        return false;
      memcpy(out, f.bytes.data, f.bytes.len);
      out[f.bytes.len] = 0;
      return true;
    }
    if (f.bytes.len * 2 >= cap)
      return false;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < f.bytes.len; ++i) {
      out[i * 2] = hex[f.bytes.data[i] >> 4];
      out[i * 2 + 1] = hex[f.bytes.data[i] & 15];
    }
    out[f.bytes.len * 2] = 0;
    return true;
  }
  if (s.type == 1) {
    double v;
    memcpy(&v, &f.value, 8);
    if (!isfinite(v))
      return false;
    n = snprintf(out, cap, "%.17g", v);
  } else if (s.type == 2) {
    float v;
    uint32_t b = uint32_t(f.value);
    memcpy(&v, &b, 4);
    if (!isfinite(v))
      return false;
    n = snprintf(out, cap, "%.9g", double(v));
  } else if (s.type == 17 || s.type == 18)
    n = snprintf(out, cap, "%lld",
                 (long long)((f.value >> 1) ^ uint64_t(-int64_t(f.value & 1))));
  else if (s.type == 5 || s.type == 14 || s.type == 15)
    n = snprintf(out, cap, "%ld", (long)(int32_t)f.value);
  else if (s.type == 3 || s.type == 16)
    n = snprintf(out, cap, "%lld", (long long)(int64_t)f.value);
  else
    n = snprintf(out, cap, "%llu", (unsigned long long)f.value);
  return n >= 0 && size_t(n) < cap;
}
static bool value(View source, const Spec &s, char *out, size_t cap) {
  if (!cap)
    return false;
  out[0] = 0;
  View v = parent(source, s);
  Reader r(v);
  Field f;
  size_t used = 0;
  bool found = false;
  auto add = [&](const Field &a) -> bool {
    if (!s.repeated)
      used = 0;
    else if (found) {
      if (used + 1 >= cap)
        return false;
      out[used++] = s.type == 9 ? '\n' : ',';
    }
    if (!atom_text(s, a, out + used, cap - used))
      return false;
    used += strlen(out + used);
    found = true;
    return true;
  };
  while (r.next(f))
    if (f.tag == s.path[s.depth - 1]) {
      if (s.repeated && f.wire == 2 && wire(s.type) != 2) {
        Reader packed(f.bytes);
        while (packed.offset < packed.v.len) {
          Field a;
          a.wire = wire(s.type);
          if (a.wire == 0) {
            if (!packed.varint(a.value))
              return false;
          } else {
            unsigned n = a.wire == 1 ? 8 : 4;
            if (n > packed.v.len - packed.offset)
              return false;
            for (unsigned i = 0; i < n; ++i)
              a.value |= uint64_t(packed.v.data[packed.offset++]) << (8 * i);
          }
          if (!add(a))
            return false;
        }
      } else if (!add(f))
        return false;
    }
  if (!found && !s.repeated) {
    Field zero;
    zero.wire = wire(s.type);
    return atom_text(s, zero, out, cap);
  }
  return r.ok;
}
} // namespace settings
