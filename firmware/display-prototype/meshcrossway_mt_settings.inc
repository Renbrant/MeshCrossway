// Local-radio administration only. No admin packet may target a mesh peer.
struct Settings {
  enum Stage {
    IDLE,
    READ_SEND,
    READ_WAIT,
    LOADED,
    FRESH_SEND,
    FRESH_WAIT,
    WRITE_SEND,
    WRITE_WAIT,
    VERIFY_SEND,
    VERIFY_WAIT,
    DONE,
    ERROR
  };
  Stage stage = IDLE;
  unsigned section = 0, field = 0;
  uint32_t request_id = 0, sent_at = 0, revision = 0;
  uint64_t baseline = 0;
  bool loaded = false;
  uint8_t passkey[32] = {};
  size_t passkey_len = 0;
  Frame current, desired, work[6];
  char input[1025] = {}, expected[1025] = {}, scratch[1025] = {};
  char status[112] = "Choose a settings group and read it from the radio.";
  void say(Stage s, const char *text) {
    stage = s;
    snprintf(status, sizeof(status), "%s", text);
    ++revision;
  }
  bool busy() const {
    return stage == READ_SEND || stage == READ_WAIT || stage == FRESH_SEND ||
           stage == FRESH_WAIT || stage == WRITE_SEND || stage == WRITE_WAIT ||
           stage == VERIFY_SEND || stage == VERIFY_WAIT;
  }
  void disconnect() {
    loaded = false;
    memset(passkey, 0, sizeof(passkey));
    passkey_len = 0;
    current.len = desired.len = 0;
    memset(input, 0, sizeof(input));
    memset(expected, 0, sizeof(expected));
    say(ERROR, "Radio disconnected. Changes are not verified; reconnect and "
               "read settings.");
  }
  bool read(unsigned group) {
    if (group >= settings::SECTION_COUNT || busy())
      return false;
    section = group;
    loaded = false;
    passkey_len = 0;
    current.len = 0;
    say(READ_SEND, "Reading settings from the radio...");
    return true;
  }
  bool apply(unsigned group, unsigned index, const char *text,
             uint32_t expected_revision) {
    if (group != section || !loaded || busy() ||
        revision != expected_revision ||
        index >= settings::sections[section].count || !text ||
        strlen(text) >= sizeof(input))
      return false;
    const auto &spec = settings::sections[section].fields[index];
    if (!settings::patch({current.data, current.len}, spec, text, desired,
                         work)) {
      snprintf(status, sizeof(status),
               "Invalid value, read-only field or settings too large.");
      return false;
    }
    if (!settings::value({desired.data, desired.len}, spec, expected,
                         sizeof(expected)))
      return false;
    strcpy(input, text);
    field = index;
    baseline = fingerprint({current.data, current.len});
    passkey_len = 0;
    say(FRESH_SEND,
        "Checking current settings and authorization before saving...");
    return true;
  }
  bool next(uint32_t now, uint32_t id, Frame &admin) {
    if ((stage == READ_WAIT || stage == FRESH_WAIT || stage == WRITE_WAIT ||
         stage == VERIFY_WAIT) &&
        uint32_t(now - sent_at) > 15000) {
      loaded = false;
      say(ERROR, "Radio did not confirm this operation. Read again; no "
                 "automatic write retry.");
    }
    if (stage != READ_SEND && stage != FRESH_SEND && stage != WRITE_SEND &&
        stage != VERIFY_SEND)
      return false;
    const auto &s = settings::sections[section];
    Writer a(admin);
    if (stage == WRITE_SEND) {
      if (s.family < 2) {
        Writer wrapper(work[2]);
        wrapper.message(s.tag, desired);
        a.message(s.family ? 35 : 34, work[2]);
      } else if (s.family == 4)
        a.message(46, desired);
      else if (s.family >= 5) {
        const View text = settings::child({desired.data, desired.len}, 1);
        a.bytes(s.family == 5 ? 36 : 37, text.data, text.len);
      } else
        a.message(s.family == 2 ? 32 : 33, desired);
      if (passkey_len)
        a.bytes(101, passkey, passkey_len);
      if (admin.len > 237) {
        say(ERROR,
            "Settings exceed this radio's admin packet limit; no write sent.");
        return false;
      }
      say(WRITE_WAIT, "Write submitted; waiting for radio confirmation...");
    } else {
      if (s.family == 0)
        a.scalar(5, s.tag - 1);
      else if (s.family == 1)
        a.scalar(7, s.tag - 1);
      else if (s.family == 2)
        a.scalar(3, 1);
      else if (s.family == 3)
        a.scalar(1, s.tag + 1);
      else
        a.scalar(s.family == 4 ? 44 : s.family == 5 ? 10 : 14, 1);
      stage = stage == READ_SEND    ? READ_WAIT
              : stage == FRESH_SEND ? FRESH_WAIT
                                    : VERIFY_WAIT;
    }
    if (!a.ok) {
      say(ERROR, "Settings packet is too large.");
      return false;
    }
    request_id = id ? id : 1;
    sent_at = now;
    return true;
  }
  void routing(uint32_t request, uint32_t code) {
    if (request != request_id || !busy())
      return;
    if (code) {
      loaded = false;
      say(ERROR, "Radio rejected settings or does not support this operation.");
      return;
    }
    if (stage == WRITE_WAIT)
      say(VERIFY_SEND, "Radio accepted the write; reading back to verify...");
  }
  bool receive(View admin, uint32_t request) {
    if (request != request_id ||
        (stage != READ_WAIT && stage != FRESH_WAIT && stage != VERIFY_WAIT))
      return true;
    const auto &s = settings::sections[section];
    Reader r(admin);
    Field f;
    View config;
    bool found = false;
    const unsigned response=s.family==0?6:s.family==1?8:s.family==2?4:s.family==3?2:s.family==4?45:s.family==5?11:15;
    while (r.next(f)) {
      if (f.tag == 101) {
        if (f.wire != 2 || f.bytes.len > sizeof(passkey))
          return false;
        memcpy(passkey, f.bytes.data, f.bytes.len);
        passkey_len = f.bytes.len;
      }
      if (f.tag == response) {
        if (f.wire != 2)
          return false;
        config = f.bytes;
        found = true;
      }
    }
    if (!r.ok)
      return false;
    if (!found)
      return true;
    if (s.family < 2) {
      Reader wrapper(config);
      bool match = false;
      while (wrapper.next(f))
        if (f.tag == s.tag && f.wire == 2) {
          config = f.bytes;
          match = true;
          break;
        }
      if (!wrapper.ok || !match) {
        say(ERROR, "Requested settings are unavailable on this firmware.");
        return true;
      }
    }
    if (s.family >= 5) {
      Writer wrapper(work[2]);
      wrapper.bytes(1, config.data, config.len);
      if (!wrapper.ok)
        return false;
      config = {work[2].data, work[2].len};
    }
    if (config.len > MAX_FRAME || !valid(config))
      return false;
    if (s.family == 3) {
      Reader channel(config);
      unsigned index = 0;
      while (channel.next(f))
        if (f.tag == 1 && f.wire == 0)
          index = f.value;
      if (!channel.ok || index != s.tag)
        return false;
    }
    if (stage == FRESH_WAIT) {
      if (fingerprint(config) != baseline) {
        loaded = false;
        say(ERROR,
            "Settings changed since reading. Read again before editing.");
        return true;
      }
      say(WRITE_SEND,
          "Current settings checked; preparing one local-radio change.");
      return true;
    }
    current.len = config.len;
    if (config.len)
      memcpy(current.data, config.data, config.len);
    loaded = true;
    if (stage == VERIFY_WAIT) {
      const auto &spec = s.fields[field];
      if (settings::value(config, spec, scratch, sizeof(scratch)) &&
          !strcmp(scratch, expected))
        say(DONE, "Saved and verified by reading the radio.");
      else
        say(ERROR,
            "Radio returned a different value. Change was not verified.");
      memset(input, 0, sizeof(input));
      memset(expected, 0, sizeof(expected));
      memset(scratch, 0, sizeof(scratch));
    } else
      say(LOADED, "Settings read from the radio. Select a field to edit.");
    return true;
  }
};
