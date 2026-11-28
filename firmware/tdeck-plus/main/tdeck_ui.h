#pragma once
// Minimal T-Deck Plus UI: radios, chats and contacts for two independent BLE
// radios (Meshtastic PhoneAPI + MeshCore Companion). No map in this edition.
// Everything here runs on the LVGL task, like the Waveshare protocol polls.
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "meshcrossway_ble.h"
#include "tdeck_board.h"

#ifndef MCX_TDECK_VERSION
#define MCX_TDECK_VERSION "dev"
#endif

namespace tdui {
using meshble::Radio;
enum Screen { HOME, RADIO, CHATS, CONTACTS, CONVERSATION };

static const lv_color_t BG = lv_color_hex(0x0c1117), PANEL = lv_color_hex(0x19212b),
                        ORANGE = lv_color_hex(0xff8a24), TEXT = lv_color_hex(0xe8eef4),
                        MUTED = lv_color_hex(0x9aa8b6), MT_COLOR = lv_color_hex(0x67ea94),
                        MC_COLOR = lv_color_hex(0x4fb3ff), BAD = lv_color_hex(0xff6b6b);

struct Target {
  Radio radio = meshble::UNKNOWN;
  bool channel = false;
  uint16_t index = 0;   // MeshCore channel slot/contact index, Meshtastic channel.
  uint32_t node = 0;    // Meshtastic destination (BROADCAST for channels).
  uint8_t key[6] = {};  // MeshCore contact key prefix, stable across list changes.
  char title[56] = {};
};

static Screen screen = HOME;
static Radio radio_screen = meshble::MESHTASTIC;
static Target target;
static lv_obj_t *root = nullptr, *status_line = nullptr, *battery_label = nullptr;
static lv_obj_t *mt_label = nullptr, *mc_label = nullptr, *radio_status = nullptr;
static lv_obj_t *messages = nullptr, *composer = nullptr, *send_feedback = nullptr;
static lv_obj_t *pin_area = nullptr;
static uint32_t drawn_signature = 0, last_draw_ms = 0;

static uint32_t now_ms() { return uint32_t(esp_timer_get_time() / 1000); }
static const char *phase_name(meshble::Phase p) {
  static const char *names[] = {"Bluetooth off", "Starting", "Idle", "Scanning", "Connecting",
                                "Verifying", "Pairing", "Connected", "Failed"};
  return unsigned(p) < 9 ? names[p] : "Unknown";
}
static bool ble_off() {
  return meshble::snapshot(meshble::MESHTASTIC).phase == meshble::OFF &&
         meshble::snapshot(meshble::MESHCORE).phase == meshble::OFF;
}
static bool mt_ready() {
  auto *c = meshmt::client;
  return c && c->connected && c->initialized && !c->fault;
}
static bool mc_ready() {
  auto *c = meshmc::client;
  return c && c->connected && c->initialized && !c->fault;
}

static void show(Screen next);
static void show_now(Screen next);

// ---------- building blocks ----------
static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color,
                       const lv_font_t *font = &lv_font_montserrat_14) {
  auto *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_color(l, color, 0);
  lv_obj_set_style_text_font(l, font, 0);
  return l;
}
static void back_cb(lv_event_t *) { show(screen == CONVERSATION ? (target.channel ? CHATS : CONTACTS) : HOME); }
static lv_obj_t *frame(const char *title, bool back) {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), BG, 0);
  root = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(root);
  lv_obj_set_size(root, tdeck::WIDTH, tdeck::HEIGHT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(root, 4, 0);
  lv_obj_set_style_pad_row(root, 3, 0);
  auto *bar = lv_obj_create(root);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, LV_PCT(100), 24);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, 6, 0);
  if (back) {
    auto *b = lv_btn_create(bar);
    lv_obj_set_size(b, 44, 22);
    lv_obj_set_style_bg_color(b, PANEL, 0);
    lv_obj_add_event_cb(b, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_center(label(b, LV_SYMBOL_LEFT, ORANGE));
  }
  auto *t = label(bar, title, ORANGE, &lv_font_montserrat_16);
  lv_obj_set_flex_grow(t, 1);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  battery_label = label(bar, "", MUTED, &lv_font_montserrat_12);
  status_line = nullptr;
  mt_label = mc_label = radio_status = messages = composer = send_feedback = pin_area = nullptr;
  return root;
}
static lv_obj_t *list(lv_obj_t *parent) {
  auto *l = lv_list_create(parent);
  lv_obj_set_width(l, LV_PCT(100));
  lv_obj_set_flex_grow(l, 1);
  lv_obj_set_style_bg_color(l, BG, 0);
  lv_obj_set_style_border_width(l, 0, 0);
  lv_obj_set_style_pad_all(l, 0, 0);
  lv_obj_set_style_pad_row(l, 2, 0);
  return l;
}
static lv_obj_t *item(lv_obj_t *l, const char *icon, const char *text, lv_color_t color,
                      lv_event_cb_t cb, intptr_t data) {
  auto *b = lv_list_add_btn(l, icon, text);
  lv_obj_set_style_bg_color(b, PANEL, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x523219), LV_STATE_FOCUSED);
  lv_obj_set_style_text_color(b, color, 0);
  lv_obj_set_style_text_font(b, &lv_font_montserrat_14, 0);
  lv_obj_set_style_pad_ver(b, 6, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  auto *text_label = lv_obj_get_child(b, -1);
  if (text_label) lv_label_set_long_mode(text_label, LV_LABEL_LONG_DOT);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)data);
  return b;
}
static void focus_first(lv_obj_t *obj) {
  if (obj) lv_group_focus_obj(obj);
}

// ---------- home ----------
static void go_cb(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  if (v == 1) show(CHATS);
  else if (v == 2) show(CONTACTS);
  else if (v == 3 || v == 4) {
    radio_screen = v == 3 ? meshble::MESHTASTIC : meshble::MESHCORE;
    show(RADIO);
  } else if (v == 5) {
    if (ble_off()) (void)meshble::request_start();
    else (void)meshble::request_stop();
    show(HOME);
  }
}
static void network_text(char *out, size_t n, Radio r) {
  auto s = meshble::snapshot(r);
  if (r == meshble::MESHTASTIC && mt_ready())
    snprintf(out, n, "Meshtastic: %s", meshmt::client->status);
  else if (r == meshble::MESHCORE && mc_ready())
    snprintf(out, n, "MeshCore: %s (%u contacts)", meshmc::client->name, unsigned(meshmc::client->contact_count));
  else
    snprintf(out, n, "%s: %s%s%s", r == meshble::MESHCORE ? "MeshCore" : "Meshtastic", phase_name(s.phase),
             s.active_name[0] ? " / " : "", s.active_name);
}
static void draw_home() {
  auto *r = frame("MeshCrossway", false);
  char text[160];
  network_text(text, sizeof(text), meshble::MESHTASTIC);
  mt_label = label(r, text, MT_COLOR, &lv_font_montserrat_12);
  lv_obj_set_width(mt_label, LV_PCT(100));
  lv_label_set_long_mode(mt_label, LV_LABEL_LONG_DOT);
  network_text(text, sizeof(text), meshble::MESHCORE);
  mc_label = label(r, text, MC_COLOR, &lv_font_montserrat_12);
  lv_obj_set_width(mc_label, LV_PCT(100));
  lv_label_set_long_mode(mc_label, LV_LABEL_LONG_DOT);
  auto *l = list(r);
  auto *first = item(l, LV_SYMBOL_ENVELOPE, "Chats & channels", TEXT, go_cb, 1);
  item(l, LV_SYMBOL_LIST, "Contacts & nodes", TEXT, go_cb, 2);
  item(l, LV_SYMBOL_BLUETOOTH, "Meshtastic radio", MT_COLOR, go_cb, 3);
  item(l, LV_SYMBOL_BLUETOOTH, "MeshCore radio", MC_COLOR, go_cb, 4);
  item(l, LV_SYMBOL_POWER, ble_off() ? "Turn Bluetooth on" : "Turn Bluetooth off", MUTED, go_cb, 5);
  char version[64];
  snprintf(version, sizeof(version), "T-Deck Plus %s  -  no message forwarding between networks", MCX_TDECK_VERSION);
  label(r, version, MUTED, &lv_font_montserrat_12);
  focus_first(first);
}

// ---------- radios ----------
static void radio_cb(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  meshble::select(radio_screen);
  if (v == -1) (void)meshble::request_start();
  else if (v == -2) meshble::scan(false);
  else if (v == -3) meshble::disconnect(radio_screen);
  else if (v == -4) (void)meshble::confirm_number(radio_screen, true);
  else if (v == -5) (void)meshble::confirm_number(radio_screen, false);
  else if (v >= 0) meshble::connect(int(v));
  drawn_signature = 0; // Rebuild with the new phase.
}
static void pin_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_READY && lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (!pin_area) return;
  const char *text = lv_textarea_get_text(pin_area);
  const uint32_t pin = uint32_t(strtoul(text, nullptr, 10));
  if (strlen(text) == 6) (void)meshble::submit_pin(radio_screen, pin);
  drawn_signature = 0;
}
static uint32_t radio_signature() {
  auto s = meshble::snapshot(radio_screen);
  return 1 + uint32_t(s.phase) * 7919u + s.count * 131u + (s.pin_required ? 17u : 0) +
         (s.confirm_required ? 29u : 0) + s.compare_number;
}
static void draw_radio() {
  const bool mt = radio_screen == meshble::MESHTASTIC;
  meshble::select(radio_screen);
  auto *r = frame(mt ? "Meshtastic radio" : "MeshCore radio", true);
  auto s = meshble::snapshot(radio_screen);
  char text[160];
  snprintf(text, sizeof(text), "%s%s%s\n%s", phase_name(s.phase), s.active_name[0] ? " - " : "", s.active_name,
           s.status);
  radio_status = label(r, text, mt ? MT_COLOR : MC_COLOR, &lv_font_montserrat_12);
  lv_obj_set_width(radio_status, LV_PCT(100));
  lv_label_set_long_mode(radio_status, LV_LABEL_LONG_WRAP);
  lv_obj_t *first = nullptr;
  if (s.pin_required) {
    label(r, "Enter the 6-digit PIN shown on the radio, then Enter:", TEXT, &lv_font_montserrat_12);
    pin_area = lv_textarea_create(r);
    lv_textarea_set_one_line(pin_area, true);
    lv_textarea_set_accepted_chars(pin_area, "0123456789");
    lv_textarea_set_max_length(pin_area, 6);
    lv_obj_set_width(pin_area, LV_PCT(100));
    lv_obj_add_event_cb(pin_area, pin_cb, LV_EVENT_READY, nullptr);
    focus_first(pin_area);
    drawn_signature = radio_signature();
    return;
  }
  auto *l = list(r);
  if (s.confirm_required) {
    snprintf(text, sizeof(text), "Confirm %06u", unsigned(s.compare_number));
    first = item(l, LV_SYMBOL_OK, text, ORANGE, radio_cb, -4);
    item(l, LV_SYMBOL_CLOSE, "Reject", BAD, radio_cb, -5);
  } else if (s.phase == meshble::OFF) {
    first = item(l, LV_SYMBOL_POWER, "Turn Bluetooth on", ORANGE, radio_cb, -1);
  } else {
    if (s.phase == meshble::READY) first = item(l, LV_SYMBOL_CLOSE, "Disconnect", BAD, radio_cb, -3);
    auto *scan = item(l, LV_SYMBOL_REFRESH, s.phase == meshble::SCANNING ? "Scanning..." : "Scan for radios",
                      ORANGE, radio_cb, -2);
    if (!first) first = scan;
    for (unsigned i = 0; i < s.count && i < 8; ++i) {
      const auto &p = s.peers[i];
      const bool match = p.advertised == radio_screen || p.advertised == meshble::UNKNOWN;
      snprintf(text, sizeof(text), "%s  %d dBm%s", p.name[0] ? p.name : "(unnamed)", int(p.rssi),
               match ? "" : "  (other network)");
      item(l, LV_SYMBOL_BLUETOOTH, text, match ? TEXT : MUTED, radio_cb, intptr_t(i));
    }
  }
  focus_first(first);
  drawn_signature = radio_signature();
}

// ---------- chats / contacts ----------
static void open_mt_channel(lv_event_t *e) {
  const unsigned ch = unsigned((intptr_t)lv_event_get_user_data(e));
  auto *c = meshmt::client;
  if (!c || ch >= meshmt::MAX_CHANNELS) return;
  target = Target{};
  target.radio = meshble::MESHTASTIC;
  target.channel = true;
  target.index = uint16_t(ch);
  target.node = meshmt::BROADCAST;
  snprintf(target.title, sizeof(target.title), "MT # %s", c->data.channels[ch].name[0] ? c->data.channels[ch].name : "Primary");
  show(CONVERSATION);
}
static void open_mc_channel(lv_event_t *e) {
  const unsigned ch = unsigned((intptr_t)lv_event_get_user_data(e));
  auto *c = meshmc::client;
  if (!c || ch >= c->channel_count) return;
  target = Target{};
  target.radio = meshble::MESHCORE;
  target.channel = true;
  target.index = uint16_t(ch);
  snprintf(target.title, sizeof(target.title), "MC # %s", c->channels[ch].name);
  show(CONVERSATION);
}
static void open_mt_node(lv_event_t *e) {
  const uint32_t num = uint32_t((uintptr_t)lv_event_get_user_data(e));
  auto *c = meshmt::client;
  if (!c) return;
  target = Target{};
  target.radio = meshble::MESHTASTIC;
  target.node = num;
  auto *n = c->node(num);
  if (n && n->name[0]) snprintf(target.title, sizeof(target.title), "MT %s", n->name);
  else snprintf(target.title, sizeof(target.title), "MT !%08lx", (unsigned long)num);
  show(CONVERSATION);
}
static void open_mc_contact(lv_event_t *e) {
  const unsigned i = unsigned((intptr_t)lv_event_get_user_data(e));
  auto *c = meshmc::client;
  if (!c || i >= c->contact_count) return;
  target = Target{};
  target.radio = meshble::MESHCORE;
  target.index = uint16_t(i);
  memcpy(target.key, c->contacts[i].key, 6);
  snprintf(target.title, sizeof(target.title), "MC %s", c->contacts[i].name);
  show(CONVERSATION);
}
static unsigned mt_unread(unsigned ch) {
  unsigned n = 0;
  if (auto *c = meshmt::client)
    for (const auto &m : c->data.messages)
      if (m.seq && !m.outgoing && !m.read && m.to == meshmt::BROADCAST && m.channel == ch) ++n;
  return n;
}
static unsigned mc_unread(unsigned slot) {
  unsigned n = 0;
  if (auto *c = meshmc::client)
    for (unsigned i = 0; i < meshmc::MAX_MESSAGES; ++i) {
      const auto &m = c->messages[i];
      if (m.id && m.channel && !m.outgoing && !m.read && m.slot == slot) ++n;
    }
  return n;
}
static void draw_chats() {
  auto *r = frame("Chats & channels", true);
  auto *l = list(r);
  lv_obj_t *first = nullptr;
  char text[96];
  if (auto *c = meshmt::client)
    for (unsigned i = 0; i < meshmt::MAX_CHANNELS; ++i) {
      const auto &ch = c->data.channels[i];
      if (!ch.loaded || !ch.role) continue;
      const unsigned unread = mt_unread(i);
      snprintf(text, sizeof(text), "%s%s", ch.name[0] ? ch.name : "Primary", unread ? "  *" : "");
      auto *b = item(l, "MT", text, MT_COLOR, open_mt_channel, intptr_t(i));
      if (!first) first = b;
    }
  if (auto *c = meshmc::client)
    for (unsigned i = 0; i < c->channel_count && i < meshmc::MAX_CHANNELS; ++i) {
      const auto &ch = c->channels[i];
      if (!ch.loaded || !ch.name[0]) continue;
      snprintf(text, sizeof(text), "%s%s", ch.name, mc_unread(i) ? "  *" : "");
      auto *b = item(l, "MC", text, MC_COLOR, open_mc_channel, intptr_t(i));
      if (!first) first = b;
    }
  if (!first) label(r, "No channels yet. Connect radios first.", MUTED, &lv_font_montserrat_12);
  focus_first(first);
}
static void draw_contacts() {
  auto *r = frame("Contacts & nodes", true);
  auto *l = list(r);
  lv_obj_t *first = nullptr;
  char text[96];
  static constexpr unsigned LIMIT = 48;
  if (auto *c = meshmt::client) {
    uint16_t order[meshmt::MAX_NODES];
    unsigned n = 0;
    for (unsigned i = 0; i < c->data.node_count && i < meshmt::MAX_NODES; ++i)
      if (c->data.nodes[i].num && c->data.nodes[i].num != c->data.my_num) order[n++] = uint16_t(i);
    std::sort(order, order + n, [c](uint16_t a, uint16_t b) {
      return c->data.nodes[a].last_heard > c->data.nodes[b].last_heard;
    });
    for (unsigned k = 0; k < n && k < LIMIT; ++k) {
      const auto &node = c->data.nodes[order[k]];
      if (node.name[0]) snprintf(text, sizeof(text), "%s (%s)", node.name, node.short_name);
      else snprintf(text, sizeof(text), "!%08lx", (unsigned long)node.num);
      auto *b = item(l, "MT", text, MT_COLOR, open_mt_node, intptr_t(node.num));
      if (!first) first = b;
    }
  }
  if (auto *c = meshmc::client) {
    static uint16_t order[meshmc::MAX_CONTACTS];
    unsigned n = 0;
    for (unsigned i = 0; i < c->contact_count && i < meshmc::MAX_CONTACTS; ++i) order[n++] = uint16_t(i);
    std::sort(order, order + n, [c](uint16_t a, uint16_t b) {
      return c->contacts[a].lastmod > c->contacts[b].lastmod;
    });
    for (unsigned k = 0; k < n && k < LIMIT; ++k) {
      const auto &contact = c->contacts[order[k]];
      const char *kind = contact.type == 2 ? "repeater" : contact.type == 3 ? "room" : contact.type == 4 ? "sensor" : "";
      snprintf(text, sizeof(text), "%s%s%s", contact.name[0] ? contact.name : "(unnamed)", kind[0] ? "  - " : "", kind);
      auto *b = item(l, "MC", text, MC_COLOR, open_mc_contact, intptr_t(order[k]));
      if (!first) first = b;
    }
  }
  if (!first) label(r, "No contacts yet. Connect radios first.", MUTED, &lv_font_montserrat_12);
  focus_first(first);
}

// ---------- conversation ----------
static bool mc_target_index(uint16_t &index) {
  auto *c = meshmc::client;
  if (!c) return false;
  if (target.channel) {
    index = target.index;
    return index < c->channel_count;
  }
  for (unsigned i = 0; i < c->contact_count && i < meshmc::MAX_CONTACTS; ++i)
    if (!memcmp(c->contacts[i].key, target.key, 6)) {
      index = uint16_t(i);
      return true;
    }
  return false;
}
static const char *mt_sender(uint32_t num) {
  static char fallback[16];
  if (auto *c = meshmt::client)
    if (auto *n = c->node(num)) {
      if (n->short_name[0]) return n->short_name;
      if (n->name[0]) return n->name;
    }
  snprintf(fallback, sizeof(fallback), "!%08lx", (unsigned long)num);
  return fallback;
}
static const char *mc_delivery(meshmc::Delivery d) {
  switch (d) {
  case meshmc::QUEUED: return " (queued)";
  case meshmc::ACCEPTED: return " (sent)";
  case meshmc::DELIVERED: return " (delivered)";
  case meshmc::REJECTED: return " (failed)";
  case meshmc::UNCERTAIN: return " (unconfirmed)";
  default: return "";
  }
}
static void bubble(const char *text, bool outgoing, lv_color_t color) {
  auto *l = label(messages, text, outgoing ? ORANGE : color, &lv_font_montserrat_12);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  if (outgoing) lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
}
static uint32_t conversation_generation() {
  if (target.radio == meshble::MESHTASTIC) return meshmt::client ? meshmt::client->generation : 0;
  return meshmc::client ? meshmc::client->generation : 0;
}
static void render_messages() {
  if (!messages) return;
  lv_obj_clean(messages);
  static constexpr unsigned SHOW = 30;
  char line[320];
  unsigned shown = 0;
  if (target.radio == meshble::MESHTASTIC) {
    auto *c = meshmt::client;
    if (c) {
      const auto t = c->target(target.node, uint8_t(target.index));
      uint16_t order[meshmt::MAX_MESSAGES];
      unsigned n = 0;
      for (unsigned i = 0; i < meshmt::MAX_MESSAGES; ++i)
        if (meshmt::message_belongs(c->data.messages[i], t, c->data.my_num)) order[n++] = uint16_t(i);
      std::sort(order, order + n, [c](uint16_t a, uint16_t b) { return c->data.messages[a].seq < c->data.messages[b].seq; });
      for (unsigned k = n > SHOW ? n - SHOW : 0; k < n; ++k) {
        auto &m = c->data.messages[order[k]];
        if (m.outgoing) snprintf(line, sizeof(line), "%s  [%s]", m.text, meshmt::delivery_name(m.delivery));
        else snprintf(line, sizeof(line), "%s: %s", mt_sender(m.from), m.text);
        bubble(line, m.outgoing, MT_COLOR);
        m.read = true;
        ++shown;
      }
    }
  } else if (auto *c = meshmc::client) {
    uint16_t index = 0;
    if (mc_target_index(index)) {
      uint16_t order[meshmc::MAX_MESSAGES];
      unsigned n = 0;
      for (unsigned i = 0; i < meshmc::MAX_MESSAGES; ++i) {
        const auto &m = c->messages[i];
        if (!m.id || m.channel != target.channel) continue;
        if (target.channel ? m.slot == index : !memcmp(m.key, target.key, 6)) order[n++] = uint16_t(i);
      }
      std::sort(order, order + n, [c](uint16_t a, uint16_t b) { return c->messages[a].id < c->messages[b].id; });
      for (unsigned k = n > SHOW ? n - SHOW : 0; k < n; ++k) {
        auto &m = c->messages[order[k]];
        if (m.outgoing) snprintf(line, sizeof(line), "%s%s", m.text, mc_delivery(m.delivery));
        else if (target.channel) snprintf(line, sizeof(line), "%s", m.text); // Channel text carries "name: ".
        else snprintf(line, sizeof(line), "%s: %s", c->contacts[index].name, m.text);
        bubble(line, m.outgoing, MC_COLOR);
        m.read = true;
        ++shown;
      }
    }
  }
  if (!shown) bubble("No messages yet.", false, MUTED);
  lv_obj_scroll_to_y(messages, LV_COORD_MAX, LV_ANIM_OFF);
  drawn_signature = conversation_generation() + 1;
  last_draw_ms = now_ms();
}
static void send_cb(lv_event_t *) {
  if (!composer) return;
  const char *text = lv_textarea_get_text(composer);
  if (!text[0]) return;
  const char *problem = nullptr;
  bool ok = false;
  if (target.radio == meshble::MESHTASTIC) {
    auto *c = meshmt::client;
    if (!c) problem = "Meshtastic radio not connected.";
    else {
      const auto t = c->target(target.node, uint8_t(target.index));
      problem = c->send_problem(t, text);
      if (!problem) ok = c->send_text(t, text, esp_random() | 1u, now_ms());
    }
  } else {
    auto *c = meshmc::client;
    uint16_t index = 0;
    if (!c || !mc_ready()) problem = "MeshCore radio not ready.";
    else if (!mc_target_index(index)) problem = "Contact or channel is no longer listed.";
    else if (strlen(text) > c->text_limit(target.channel)) problem = "Message too long for MeshCore.";
    else ok = c->send_text(target.channel, index, text);
  }
  if (ok) {
    lv_textarea_set_text(composer, "");
    lv_label_set_text(send_feedback, "Queued for the radio.");
    render_messages();
  } else {
    lv_label_set_text(send_feedback, problem ? problem : "Could not send. Check the radio.");
  }
}
static void draw_conversation() {
  auto *r = frame(target.title, true);
  messages = lv_obj_create(r);
  lv_obj_set_width(messages, LV_PCT(100));
  lv_obj_set_flex_grow(messages, 1);
  lv_obj_set_flex_flow(messages, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_color(messages, PANEL, 0);
  lv_obj_set_style_border_width(messages, 0, 0);
  lv_obj_set_style_pad_all(messages, 4, 0);
  lv_obj_set_style_pad_row(messages, 3, 0);
  lv_group_remove_obj(messages);
  send_feedback = label(r, target.radio == meshble::MESHTASTIC ? "Type and press Enter to send to Meshtastic"
                                                               : "Type and press Enter to send to MeshCore",
                        MUTED, &lv_font_montserrat_12);
  lv_obj_set_width(send_feedback, LV_PCT(100));
  lv_label_set_long_mode(send_feedback, LV_LABEL_LONG_DOT);
  composer = lv_textarea_create(r);
  lv_textarea_set_one_line(composer, true);
  lv_textarea_set_max_length(composer, target.radio == meshble::MESHTASTIC ? meshmt::MAX_TEXT : meshmc::MAX_TEXT);
  lv_textarea_set_placeholder_text(composer, "Message");
  lv_obj_set_width(composer, LV_PCT(100));
  lv_obj_add_event_cb(composer, send_cb, LV_EVENT_READY, nullptr);
  render_messages();
  focus_first(composer);
}

// Screens are rebuilt outside the input event that requested them, so the
// clicked object is never deleted while LVGL is still delivering its event.
static void show_async(void *next) { show_now(Screen((intptr_t)next)); }
static void show(Screen next) { lv_async_call(show_async, (void *)(intptr_t)next); }
static void show_now(Screen next) {
  screen = next;
  drawn_signature = 0;
  switch (next) {
  case HOME: draw_home(); break;
  case RADIO: draw_radio(); break;
  case CHATS: draw_chats(); break;
  case CONTACTS: draw_contacts(); break;
  case CONVERSATION: draw_conversation(); break;
  }
  last_draw_ms = now_ms();
}

// ---------- periodic work (LVGL task) ----------
static void poll(lv_timer_t *) {
  meshmc::poll();
  meshmt::poll();
  const uint32_t now = now_ms();
  static uint32_t battery_at = 0;
  if (battery_label && now - battery_at > 5000) {
    battery_at = now;
    const int mv = tdeck::battery_mv(), pct = tdeck::battery_percent(mv);
    char text[24];
    if (pct >= 0) snprintf(text, sizeof(text), "%d%%", pct);
    else text[0] = 0;
    lv_label_set_text(battery_label, text);
  }
  if (now - last_draw_ms < 400) return;
  char text[160];
  switch (screen) {
  case HOME:
    if (mt_label) { network_text(text, sizeof(text), meshble::MESHTASTIC); lv_label_set_text(mt_label, text); }
    if (mc_label) { network_text(text, sizeof(text), meshble::MESHCORE); lv_label_set_text(mc_label, text); }
    last_draw_ms = now;
    break;
  case RADIO:
    if (radio_signature() != drawn_signature) show_now(RADIO);
    else if (radio_status) {
      auto s = meshble::snapshot(radio_screen);
      snprintf(text, sizeof(text), "%s%s%s\n%s", phase_name(s.phase), s.active_name[0] ? " - " : "", s.active_name, s.status);
      if (strcmp(lv_label_get_text(radio_status), text)) lv_label_set_text(radio_status, text);
      last_draw_ms = now;
    }
    break;
  case CONVERSATION:
    if (conversation_generation() + 1 != drawn_signature) render_messages();
    break;
  default: break; // Lists rebuild when reopened; avoids moving focus under the user.
  }
}

static void init() {
  lv_group_t *group = lv_group_create();
  lv_group_set_default(group);
  lv_group_set_wrap(group, true);
  show_now(HOME);
  lv_timer_create(poll, 50, nullptr);
}
} // namespace tdui
