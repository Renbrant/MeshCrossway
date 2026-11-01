#pragma once
#include "meshcrossway_remote_chat_http.h"
namespace mcxsettingshttp {
struct Workspace {
  char network[3] = {}, identity[65] = {}, value[2049] = {}, body[2305] = {},
       response[32768] = {};
  uint32_t action = 0, section = 0, field = 0, revision = 0;
  unsigned length = 0;
  int code = 200;
};
static Workspace *work = nullptr;
static std::atomic<unsigned> phase{0};
static bool init() {
  if (work)
    return true;
  void *p =
      heap_caps_malloc(sizeof(Workspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p)
    return false;
  work = new (p) Workspace();
  return true;
}
static void result(const char *text, int code) {
  mcxremotechat::Json j{work->response, sizeof(work->response)};
  j.add("{\"message\":");
  j.text(text);
  j.add("}");
  work->length = j.used;
  work->code = code;
}
static void process() {
  const bool mc = !strcmp(work->network, "mc");
  auto *c = meshmc::client;
  auto *m = meshmt::client;
  if ((mc && !c) || (!mc && !m)) {
    result("Radio model unavailable.", 409);
    return;
  }
  char identity[65] = {};
  if (mc)
    mcxremotechat::token("mc", c->public_key, 32, identity);
  else
    mcxremotechat::token("mt", m->data.radio, 7, identity);
  const bool connected =
      mc ? (c->connected && c->initialized && !c->fault)
         : (m->connected && m->initialized && m->history_ready && !m->fault);
  if (work->action) {
    struct ClearValue {~ClearValue(){memset(work->value,0,sizeof(work->value));}} clear_value;
    if (!connected) {
      result("Connect and synchronize this radio first.", 409);
      return;
    }
    if (work->action != 1 && strcmp(identity, work->identity)) {
      result("Radio identity changed. Read settings again.", 409);
      return;
    }
    if (!mc) {
      auto &s = m->settings;
      bool ok = work->action == 1
                    ? s.read(work->section)
                    : work->action == 2 && s.apply(work->section, work->field,
                                                   work->value, work->revision);
      result(ok ? s.status
                : "Settings changed, value invalid or radio busy. Read again.",
             ok ? 202 : 409);
    } else {
      const auto *cmd = meshmc::command(work->section);
      if (work->action != 3 || !cmd || c->protocol < cmd->min_version) {
        result("Command unavailable on this firmware.", 409);
        return;
      }
      const char *values[10] = {};
      size_t count = 0;
      char *p = work->value;
      if (cmd->fields[0]) {
        values[count++] = p;
        while ((p = strchr(p, '\n'))) {
          *p++ = 0;
          if (count == 10) {
            result("Too many command fields.", 409);
            return;
          }
          values[count++] = p;
        }
      }
      meshmc::Frame frame;
      char error[120] = {};
      if (!meshmc::build(*cmd, values, count, frame, error, sizeof(error))) {
        result(error, 409);
        return;
      }
      const bool ok = c->enqueue(frame);
      memset(frame.data, 0, sizeof(frame.data));
      result(ok ? "Command queued; follow the radio response below."
                : "Radio busy, unsupported parameters or payload too large.",
             ok ? 202 : 409);
    }
    memset(work->value, 0, sizeof(work->value));
    return;
  }
  mcxremotechat::Json j{work->response, sizeof(work->response)};
  j.add("{\"identity\":");
  j.text(identity);
  j.add(",\"connected\":");
  j.add(connected ? "true" : "false");
  j.add(",\"firmware\":");
  j.text(mc ? c->firmware : m->firmware);
  if (mc) {
    j.add(",\"status\":");
    j.text(c->status);
    j.add(",\"result\":");
    const unsigned response = c->last_frame.len ? c->last_frame.data[0] : 0;
    j.text(response == 14 || response == 18 || response == 28
               ? "Sensitive response: inspect locally on the device."
               : c->result);
    j.add(",\"frequency\":");
    j.number(c->frequency);
    j.add(",\"bandwidth\":");
    j.number(c->bandwidth);
    j.add(",\"sf\":");
    j.number(c->sf);
    j.add(",\"cr\":");
    j.number(c->cr);
    j.add(",\"presets\":[");
    for (unsigned i = 0; i < meshmc::RADIO_PRESET_COUNT; ++i) {
      const auto &p = meshmc::RADIO_PRESETS[i];
      if (i)
        j.add(",");
      j.add("{\"name\":");
      j.text(p.name);
      j.add(",\"values\":[");
      j.number(p.frequency);
      j.add(",");
      j.number(p.bandwidth);
      j.add(",");
      j.number(p.sf);
      j.add(",");
      j.number(p.cr);
      j.add(",0],\"hash_bytes\":");
      j.number(p.hash_bytes);
      j.add("}");
    }
    j.add("]");
    j.add(",\"commands\":[");
    bool first = true;
    for (const auto &cmd : meshmc::COMMANDS) {
      if (!first)
        j.add(",");
      first = false;
      j.add("{\"id\":");
      j.number(cmd.code);
      j.add(",\"name\":");
      j.text(cmd.name);
      j.add(",\"fields\":");
      j.text(cmd.fields);
      j.add(",\"confirm\":");
      j.add(cmd.confirm ? "true" : "false");
      j.add(",\"supported\":");
      j.add(connected && c->protocol >= cmd.min_version ? "true" : "false");
      j.add("}");
    }
    j.add("]");
  } else {
    auto &s = m->settings;
    j.add(",\"status\":");
    j.text(s.status);
    j.add(",\"revision\":");
    j.number(s.revision);
    j.add(",\"section\":");
    j.number(s.section);
    j.add(",\"busy\":");
    j.add(s.busy() ? "true" : "false");
    j.add(",\"loaded\":");
    j.add(s.loaded ? "true" : "false");
    j.add(",\"groups\":[");
    for (unsigned i = 0; i < meshmt::settings::SECTION_COUNT; ++i) {
      if (i)
        j.add(",");
      const auto &g = meshmt::settings::sections[i];
      j.add("{\"id\":");
      j.number(i);
      j.add(",\"name\":");
      j.text(g.name);
      j.add(",\"module\":");
      j.add(g.family == 1 ? "true" : "false");
      j.add("}");
    }
    j.add("],\"fields\":[");
    if (s.loaded) {
      const auto &g = meshmt::settings::sections[s.section];
      for (unsigned i = 0; i < g.count; ++i) {
        const auto &f = g.fields[i];
        bool valid = meshmt::settings::value({s.current.data, s.current.len}, f,
                                             s.scratch, sizeof(s.scratch));
        if (i)
          j.add(",");
        j.add("{\"id\":");
        j.number(i);
        j.add(",\"name\":");
        j.text(f.name);
        j.add(",\"type\":");
        j.number(f.type);
        j.add(",\"repeated\":");
        j.add(f.repeated ? "true" : "false");
        j.add(",\"secret\":");
        j.add(f.secret ? "true" : "false");
        j.add(",\"readonly\":");
        j.add(f.readonly || !valid ? "true" : "false");
        j.add(",\"choices\":");
        j.text(f.choices);
        j.add(",\"value\":");
        j.text(f.secret ? "" : valid ? s.scratch : "Unavailable");
        j.add("}");
      }
      memset(s.scratch, 0, sizeof(s.scratch));
    }
    j.add("]");
  }
  j.add("}");
  if (!j.ok) {
    result("Settings response exceeds capacity.", 503);
    return;
  }
  work->length = j.used;
  work->code = 200;
}
static void poll() {
  unsigned expected = 1;
  if (!phase.compare_exchange_strong(expected, 2))
    return;
  process();
  phase.store(3);
}
static esp_err_t exchange(httpd_req_t *r) {
  phase.store(1);
  unsigned waited = 0;
  while (phase.load() != 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (++waited == 150) {
      unsigned expected = 1;
      if (phase.compare_exchange_strong(expected, 0)) {
        memset(work->value, 0, sizeof(work->value));
        return mcxremotechat::error(r, "503 Service Unavailable",
                                    "Device busy; settings action cancelled.");
      }
    }
  }
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_status(r, work->code == 202   ? "202 Accepted"
                           : work->code == 409 ? "409 Conflict"
                           : work->code == 503 ? "503 Service Unavailable"
                                               : "200 OK");
  const auto rc = httpd_resp_send(r, work->response, work->length);
  phase.store(0);
  return rc;
}
static esp_err_t state(httpd_req_t *r) {
  if (!work || phase.load() != 0)
    return mcxremotechat::error(r, "503 Service Unavailable", "Settings busy.");
  char query[32] = {};
  if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "network", work->network,
                            sizeof(work->network)) != ESP_OK ||
      (strcmp(work->network, "mc") && strcmp(work->network, "mt")))
    return mcxremotechat::error(r, "400 Bad Request", "Choose a network.");
  work->action = 0;
  return exchange(r);
}
static esp_err_t send(httpd_req_t *r) {
  if (!work || phase.load() != 0)
    return mcxremotechat::error(r, "503 Service Unavailable", "Settings busy.");
  char header[8] = {};
  if (httpd_req_get_hdr_value_str(r, "X-MCX-Request", header, sizeof(header)) !=
          ESP_OK ||
      strcmp(header, "1"))
    return mcxremotechat::error(r, "403 Forbidden",
                                "Local application header required.");
  if (r->content_len < 10 || r->content_len >= int(sizeof(work->body)))
    return mcxremotechat::error(r, "400 Bad Request",
                                "Invalid settings length.");
  unsigned used = 0;
  while (used < unsigned(r->content_len)) {
    int n = httpd_req_recv(r, work->body + used, r->content_len - used);
    if (n <= 0) {
      memset(work->body, 0, sizeof(work->body));
      return mcxremotechat::error(r, "408 Request Timeout",
                                  "Incomplete settings request.");
    }
    used += unsigned(n);
  }
  work->body[used] = 0;
  char *parts[7] = {work->body};
  bool valid = !memchr(work->body, 0, used);
  for (unsigned i = 1; i < 7 && valid; ++i) {
    char *p = strchr(parts[i - 1], '|');
    if (!p)
      valid = false;
    else {
      *p++ = 0;
      parts[i] = p;
    }
  }
  if (valid)
    valid = (!strcmp(parts[0], "mc") || !strcmp(parts[0], "mt")) &&
            mcxnavhttp::number(parts[1], work->action) && work->action >= 1 &&
            work->action <= 3 && mcxnavhttp::number(parts[2], work->section) &&
            mcxnavhttp::number(parts[3], work->field) &&
            mcxnavhttp::number(parts[4], work->revision) &&
            mcxremotechat::valid_target(parts[5]) &&
            strlen(parts[6]) < sizeof(work->value);
  if (valid) {
    strcpy(work->network, parts[0]);
    strcpy(work->identity, parts[5]);
    strcpy(work->value, parts[6]);
  }
  memset(work->body, 0, sizeof(work->body));
  if (!valid)
    return mcxremotechat::error(r, "400 Bad Request",
                                "Invalid settings fields.");
  return exchange(r);
}
} // namespace mcxsettingshttp
