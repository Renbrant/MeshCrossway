#pragma once
// Included inside meshcrossway after the shared LVGL helpers. All callbacks stay on LVGL thread.
namespace mcui {
static lv_obj_t *dialog = nullptr, *scroller = nullptr, *feedback = nullptr, *editor = nullptr,
                *edit_value = nullptr, *keyboard = nullptr, *edit_target = nullptr;
static lv_obj_t *fields[10] = {};
static lv_obj_t *radio_status = nullptr, *home_name = nullptr, *home_status = nullptr;
static uint8_t field_count = 0, selected_command = 0;
static bool confirm_pending = false, group = false, conversation = false, reveal = false;
static uint8_t target_key[32] = {};
static uint16_t target_slot = 0, list_offset = 0;
static uint32_t rendered_generation = 0;
static int mode = 0; // 0 menu, 1 typed command, 2 compose, 3 result, 4 contact details
static void tools();
static void form(uint8_t code);
static void render();
static meshmc::Client *session() {
  return meshmc::client;
}
static bool ready() {
  return session() && session()->initialized && session()->connected && !session()->fault;
}
static void close_editor() {
  if (!editor)
    return;
  lv_keyboard_set_textarea(keyboard, nullptr);
  lv_obj_t *old = editor;
  editor = edit_value = keyboard = edit_target = nullptr;
  lv_obj_del(old);
}
static void close() {
  close_editor();
  if (dialog)
    lv_obj_del(dialog);
  dialog = scroller = feedback = nullptr;
  memset(fields, 0, sizeof(fields));
  field_count = 0;
  confirm_pending = false;
  reveal = false;
}
static void on_close(lv_event_t *) {
  close();
}
static void on_tools(lv_event_t *) {
  tools();
}
static void base(const char *title) {
  close();
  dialog = panel_at(lv_scr_act(), 0, 0, 480, 320, bg, 0);
  txt(dialog, title, 9, 12, 350, orange);
  action_button(dialog, "CLOSE", 377, 4, 96, 36, panel2, on_close, 0);
  scroller = panel_at(dialog, 0, 46, 480, 218, bg, 0);
  lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(scroller, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_AUTO);
}
static void editor_done(lv_event_t *) {
  if (edit_target && edit_value)
    lv_textarea_set_text(edit_target, lv_textarea_get_text(edit_value));
  close_editor();
  confirm_pending = false;
}
static void editor_cancel(lv_event_t *) {
  close_editor();
}
static void edit(lv_event_t *e) {
  if (editor)
    return;
  edit_target = lv_event_get_target(e);
  editor = panel_at(dialog, 0, 0, 480, 320, bg, 0);
  txt(editor, "MESHCORE / EDIT", 8, 10, 260, orange);
  action_button(editor, "CANCEL", 276, 4, 94, 32, panel2, editor_cancel, 0);
  action_button(editor, "DONE", 376, 4, 96, 32, orange, editor_done, 0);
  edit_value = lv_textarea_create(editor);
  lv_obj_set_pos(edit_value, 6, 42);
  lv_obj_set_size(edit_value, 468, 48);
  lv_textarea_set_one_line(edit_value, true);
  lv_textarea_set_max_length(edit_value, 350);
  lv_textarea_set_text(edit_value, lv_textarea_get_text(edit_target));
  lv_textarea_set_password_mode(edit_value, lv_textarea_get_password_mode(edit_target));
  keyboard = lv_keyboard_create(editor);
  lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(keyboard, 4, 96);
  lv_obj_set_size(keyboard, 472, 218);
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, wifi_qwerty_lower, wifi_qwerty_lower_ctrl);
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, wifi_qwerty_upper, wifi_qwerty_upper_ctrl);
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, wifi_qwerty_symbols, wifi_qwerty_symbols_ctrl);
  lv_keyboard_set_textarea(keyboard, edit_value);
  lv_obj_add_event_cb(keyboard, editor_done, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, editor_cancel, LV_EVENT_CANCEL, nullptr);
}
static void field(const char *label, const char *value, int y, bool secret = false) {
  if (field_count == 10)
    return;
  txt(scroller, label, 10, y, 450, white);
  lv_obj_t *t = lv_textarea_create(scroller);
  fields[field_count++] = t;
  lv_obj_set_pos(t, 10, y + 23);
  lv_obj_set_size(t, 455, 43);
  lv_textarea_set_one_line(t, true);
  lv_textarea_set_max_length(t, 350);
  lv_textarea_set_text(t, value);
  lv_textarea_set_password_mode(t, secret);
  lv_obj_add_event_cb(t, edit, LV_EVENT_CLICKED, nullptr);
}
static void show_data(lv_event_t *) {
  reveal = !reveal;
  if (session())
    rendered_generation = session()->generation - 1;
}
static void result() {
  base("MESHCORE / RESULT");
  mode = 3;
  feedback = txt(scroller, session() ? session()->result : "No radio session", 10, 5, 453, white);
  lv_label_set_long_mode(feedback, LV_LABEL_LONG_WRAP);
  action_button(dialog, "TOOLS", 10, 273, 220, 40, panel2, on_tools, 0);
  action_button(dialog, "SHOW / HIDE DATA", 240, 273, 230, 40, panel2, show_data, 0);
}
static void apply(lv_event_t *e) {
  if (!ready()) {
    lv_label_set_text(feedback, "Connect and finish synchronization first.");
    return;
  }
  const meshmc::Command *c = meshmc::command(selected_command);
  if (!c)
    return;
  if (c->confirm && !confirm_pending) {
    confirm_pending = true;
    lv_label_set_text(feedback, "Apply this change to the radio? Press CONFIRM.");
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), "CONFIRM");
    return;
  }
  const char *values[10];
  for (int i = 0; i < field_count; i++)
    values[i] = lv_textarea_get_text(fields[i]);
  char derived_key[33];
  if (selected_command == 32 && field_count == 3 && values[1][0] &&
      (!values[2][0] || !strcmp(values[2], "00000000000000000000000000000000"))) {
    uint8_t key[16];
    if (!meshmc::channel_secret(values[1], key)) {
      lv_label_set_text(feedback, "Could not create channel key");
      return;
    }
    meshmc::hex(key, 16, derived_key, sizeof(derived_key));
    memset(key, 0, sizeof(key));
    values[2] = derived_key;
  }
  meshmc::Frame frame;
  char error[120] = "Invalid parameters";
  if (!meshmc::build(*c, values, field_count, frame, error, sizeof(error))) {
    confirm_pending = false;
    lv_label_set_text(feedback, error);
    return;
  }
  if (!session()->enqueue(frame)) {
    confirm_pending = false;
    lv_label_set_text(feedback,
                      "Queue full, unsupported firmware, slot/power out of range, or frame exceeds BLE MTU.");
    return;
  }
  if (selected_command == 4)
    list_offset = 0;
  result();
  lv_label_set_text(feedback, "Command queued. Waiting for radio response...");
}
static void form(uint8_t code) {
  const meshmc::Command *c = meshmc::command(code);
  if (!c)
    return;
  base(c->name);
  mode = 1;
  selected_command = code;
  char schema[360];
  snprintf(schema, sizeof(schema), "%s", c->fields);
  char *save = nullptr;
  int y = 0;
  for (char *token = strtok_r(schema, "|", &save); token; token = strtok_r(nullptr, "|", &save)) {
    char *colon = strchr(token, ':');
    if (!colon)
      continue;
    *colon = 0;
    if (!strcmp(token, "zero"))
      continue;
    char value[360] = "";
    if (!strcmp(token, "key"))
      meshmc::hex(target_key, 32, value, sizeof(value));
    else if (!strcmp(token, "prefix"))
      meshmc::hex(target_key, 6, value, sizeof(value));
    else if (!strcmp(token, "byte") || !strcmp(token, "word") || !strcmp(token, "uint") ||
             !strcmp(token, "int") || !strcmp(token, "sbyte"))
      snprintf(value, sizeof(value), "0");
    if (strstr(colon + 1, "Channel slot"))
      snprintf(value, sizeof(value), "%u", target_slot);
    if (strstr(colon + 1, "Unix"))
      snprintf(value, sizeof(value), "%lu", (unsigned long)(session() ? session()->timestamp() : 0));
    if (session()) {
      if (code == 8)
        snprintf(value, sizeof(value), "%s", session()->name);
      if (code == 11) {
        if (field_count == 0)
          snprintf(value, sizeof(value), "%lu", (unsigned long)session()->frequency);
        if (field_count == 1)
          snprintf(value, sizeof(value), "%lu", (unsigned long)session()->bandwidth);
        if (field_count == 2)
          snprintf(value, sizeof(value), "%u", session()->sf);
        if (field_count == 3)
          snprintf(value, sizeof(value), "%u", session()->cr);
        if (field_count == 4)
          snprintf(value, sizeof(value), "%u", session()->repeat_enabled ? 1 : 0);
      }
      if (code == 38) {
        unsigned policies[] = {session()->manual_contacts, session()->telemetry_policy,
                               session()->location_policy, session()->multi_acks};
        snprintf(value, sizeof(value), "%u", policies[field_count]);
      }
      if (code == 12)
        snprintf(value, sizeof(value), "%d", (int8_t)session()->tx_power);
      if (code == 32 && target_slot < session()->channel_count) {
        if (field_count == 1)
          snprintf(value, sizeof(value), "%s", session()->channels[target_slot].name);
        if (field_count == 2)
          meshmc::hex(session()->channels[target_slot].secret, 16, value, sizeof(value));
      }
      if (code == 66)
        snprintf(value, sizeof(value), "ver");
      if (code == 39 && field_count == 0 && session()->find_contact(target_key) >= 0)
        meshmc::hex(target_key, 32, value, sizeof(value));
      if (code == 62 && field_count == 1)
        snprintf(value, sizeof(value), "255");
    }
    field(colon + 1, value, y,
          strstr(colon + 1, "Password") || strstr(colon + 1, "Private identity") ||
              strstr(colon + 1, "key (hex)"));
    y += 76;
  }
  if (!field_count)
    txt(scroller,
        c->confirm ? "This operation changes or exposes radio data."
                   : "Press RUN to request fresh data from the radio.",
        10, 10, 450, white);
  feedback = txt(scroller, "", 10, y + 8, 450, orange);
  lv_label_set_long_mode(feedback, LV_LABEL_LONG_WRAP);
  action_button(dialog, c->confirm ? "APPLY" : "RUN", 10, 273, 220, 40, orange, apply, 0);
  action_button(dialog, "TOOLS", 240, 273, 230, 40, panel2, on_tools, 0);
}
static void choose_command(lv_event_t *e) {
  form((uint8_t)(intptr_t)lv_event_get_user_data(e));
}
static void tools() {
  base("MESHCORE / TOOLS & SETTINGS");
  mode = 0;
  int y = 0;
  for (auto &c : meshmc::COMMANDS) {
    // Messaging has dedicated forms; include binary/expert tools without pretending they are
    // board-independent.
    lv_obj_t *b = action_button(scroller, c.name, 8, y, 460, 43, panel2, choose_command, c.code);
    if (!ready() || (session()->protocol && session()->protocol < c.min_version))
      lv_obj_add_state(b, LV_STATE_DISABLED);
    y += 49;
  }
  feedback = txt(dialog, session() ? session()->status : "Connect your MeshCore radio in More / Radios.", 8,
                 279, 460, muted);
}
static const char *delivery(meshmc::Delivery d) {
  return d == meshmc::QUEUED      ? "QUEUED"
         : d == meshmc::ACCEPTED  ? "RADIO ACCEPTED"
         : d == meshmc::DELIVERED ? "DELIVERED"
         : d == meshmc::REJECTED  ? "REJECTED"
         : d == meshmc::UNCERTAIN ? "UNCONFIRMED"
                                  : "RECEIVED";
}
static bool in_conversation(const meshmc::Message &m) {
  return m.id && m.channel == group && (group ? m.slot == target_slot : !memcmp(m.key, target_key, 6));
}
static void compose_send(lv_event_t *) {
  if (!session() || !fields[0])
    return;
  int target = group ? target_slot : session()->find_contact(target_key);
  if (target < 0 || !session()->send_text(group, target, lv_textarea_get_text(fields[0]))) {
    lv_label_set_text(feedback,
                      "Cannot send: check radio connection/clock, recipient, queue and 150-byte text limit.");
    return;
  }
  close();
  page = 3;
  draw();
}
static void compose() {
  if (!conversation) {
    notice("MESHCORE / COMPOSE", "Choose a real MeshCore contact or channel first.");
    return;
  }
  base("MESHCORE / COMPOSE");
  mode = 2;
  char limit[80];
  snprintf(limit, sizeof(limit), "Message (up to %u UTF-8 bytes)",
           (unsigned)(session() ? session()->text_limit(group) : 0));
  field(limit, "", 0);
  feedback = txt(scroller, "The radio must be connected with a valid clock.", 10, 88, 450, muted);
  lv_label_set_long_mode(feedback, LV_LABEL_LONG_WRAP);
  action_button(dialog, "SEND", 10, 273, 460, 40, orange, compose_send, 0);
}
static void compose_event(lv_event_t *) {
  compose();
}
static void contact_chat(lv_event_t *) {
  close();
  group = false;
  conversation = true;
  page = 3;
  draw();
}
static void contact_details() {
  if (!session())
    return;
  int index = session()->find_contact(target_key);
  if (index < 0)
    return;
  const auto &c = session()->contacts[index];
  base(c.name);
  mode = 4;
  char key[65], text[300];
  meshmc::hex(c.key, 32, key, sizeof(key));
  snprintf(text, sizeof(text),
           "%s\nType: %s\nPath: %u / last advert: %lu\nPosition: %.6f, %.6f\nPublic key\n%s", c.name,
           c.type == 2   ? "Repeater"
           : c.type == 3 ? "Room server"
           : c.type == 4 ? "Sensor"
                         : "Companion",
           c.path_len, (unsigned long)c.advert, c.lat / 1e6, c.lon / 1e6, key);
  lv_obj_t *l = txt(scroller, text, 9, 0, 455, white);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  action_button(scroller, "CHAT", 8, 170, 215, 42, orange, contact_chat, 0);
  action_button(scroller, "LOGIN", 234, 170, 235, 42, panel2, choose_command, 26);
  action_button(scroller, "STATUS", 8, 220, 215, 42, panel2, choose_command, 27);
  action_button(scroller, "TELEMETRY", 234, 220, 235, 42, panel2, choose_command, 39);
  action_button(scroller, "DISCOVER PATH", 8, 270, 215, 42, panel2, choose_command, 52);
  action_button(scroller, "RESET PATH", 234, 270, 235, 42, panel2, choose_command, 13);
  action_button(scroller, "REMOVE CONTACT", 8, 320, 460, 42, panel2, choose_command, 15);
}
static void choose_contact_now(void *data) {
  int index = (int)(intptr_t)data;
  if (!session() || index < 0 || index >= session()->contact_count)
    return;
  memcpy(target_key, session()->contacts[index].key, 32);
  contact_details();
}
static void choose_contact(lv_event_t *e) {
  lv_async_call(choose_contact_now, lv_event_get_user_data(e));
}
static void choose_channel_now(void *data) {
  target_slot = (uint16_t)(intptr_t)data;
  if (session() && (!session()->channels[target_slot].loaded || !session()->channels[target_slot].name[0])) {
    form(32);
    return;
  }
  group = true;
  conversation = true;
  page = 3;
  draw();
}
static void choose_channel(lv_event_t *e) {
  lv_async_call(choose_channel_now, lv_event_get_user_data(e));
}
static void paginate_now(void *data) {
  int direction = (int)(intptr_t)data;
  list_offset = direction > 0 ? list_offset + 10 : (list_offset >= 10 ? list_offset - 10 : 0);
  draw();
}
static void paginate(lv_event_t *e) {
  lv_async_call(paginate_now, lv_event_get_user_data(e));
}
static int unread(bool is_group, int index) {
  if (!session())
    return 0;
  int count = 0;
  for (auto &m : session()->messages)
    if (m.id && !m.outgoing && !m.read && m.channel == is_group &&
        (is_group ? m.slot == index : !memcmp(m.key, session()->contacts[index].key, 6)))
      ++count;
  return count;
}
static void render() {
  const int w = content_width();
  auto *s = session();
  rendered_generation = s ? s->generation : 0;
  if (page == 1 || page == 2) {
    const bool channels = page == 2;
    show_center_heading(channels ? "GROUPS / CHANNELS" : "NODES / MESHCORE",
                        ready() ? "Live MeshCore session" : "Connect a MeshCore Companion");
    int filter = channels ? groups_filter : nodes_filter;
    filter_tabs(w, filter);
    if (filter == 1) {
      single_label(body, "Meshtastic is not integrated yet.", 8, 107, w - 16, muted);
      return;
    }
    if (!s || !s->initialized || !s->connected) {
      single_label(body, s ? s->status : "More > Radios > Enable BLE", 8, 107, w - 16, orange);
      return;
    }
    const int total = channels ? s->channel_count : s->contact_count;
    if (list_offset >= total)
      list_offset = 0;
    action_button(body, "<", 8, 99, 45, 34, panel2, paginate, -1);
    action_button(body, ">", 59, 99, 45, 34, panel2, paginate, 1);
    action_button(body, "TOOLS", 112, 99, w - 120, 34, panel2, on_tools, 0);
    int y = 141;
    if (!total)
      single_label(body, "No entries reported by the radio.", 8, y, w - 16, muted);
    for (int i = list_offset; i < total && i < list_offset + 10; i++) {
      char title[75], detail[100];
      bool loaded = channels && s->channels[i].loaded;
      if (channels) {
        snprintf(title, sizeof(title), "%u / %s", i,
                 loaded && s->channels[i].name[0] ? s->channels[i].name : "Empty slot");
        snprintf(detail, sizeof(detail), "%d unread / tap to %s", unread(true, i),
                 loaded && s->channels[i].name[0] ? "chat" : "configure");
      } else {
        const auto &c = s->contacts[i];
        snprintf(title, sizeof(title), "%s", c.name);
        snprintf(detail, sizeof(detail), "%s / %d unread",
                 c.type == 2   ? "Repeater"
                 : c.type == 3 ? "Room"
                 : c.type == 4 ? "Sensor"
                               : "Companion",
                 unread(false, i));
      }
      lv_obj_t *b =
          action_button(body, "", 8, y, w - 16, 60, panel, channels ? choose_channel : choose_contact, i);
      txt(b, title, 7, 6, w - 30, white);
      txt(b, detail, 7, 33, w - 30, muted);
      y += 67;
    }
  } else if (page == 3) {
    if (!conversation || !s) {
      show_center_heading("CHAT / NO SELECTION", "Choose a MeshCore Node or Group");
      return;
    }
    int ci = group ? -1 : s->find_contact(target_key);
    const char *title = group     ? s->channels[target_slot].name
                        : ci >= 0 ? s->contacts[ci].name
                                  : "Unknown contact";
    show_center_heading(title[0] ? title : "Empty channel",
                        ready() ? "MeshCore / live" : "MeshCore / disconnected history");
    action_button(body, "COMPOSE", 8, 53, (w - 24) / 2, 40, orange, compose_event, 0);
    action_button(body, "TOOLS", 16 + (w - 24) / 2, 53, (w - 24) / 2, 40, panel2, on_tools, 0);
    int y = 104;
    for (uint32_t id = s->next_id > s->message_count ? s->next_id - s->message_count : 1; id < s->next_id;
         id++) {
      auto *m = s->by_id(id);
      if (!m || !in_conversation(*m))
        continue;
      m->read = true;
      char label[120];
      snprintf(label, sizeof(label), "%s / %s / %lu", m->outgoing ? "YOU" : "RX", delivery(m->delivery),
               (unsigned long)m->timestamp);
      if (!m->outgoing && m->type == 2) {
        char signer[9];
        meshmc::hex(m->signer, 4, signer, sizeof(signer));
        snprintf(label, sizeof(label), "RX / signer %s / %lu", signer, (unsigned long)m->timestamp);
      }
      lv_obj_t *card = panel_at(body, 8, y, w - 16, 94, panel, 7);
      txt(card, label, 7, 5, w - 30, m->outgoing ? orange : green);
      lv_obj_t *text = txt(card, m->text, 7, 29, w - 30, white);
      lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
      lv_obj_set_height(text, 60);
      y += 102;
    }
    if (y == 104)
      single_label(body, "No messages in this session.", 8, 109, w - 16, muted);
    lv_obj_scroll_to_y(body, y, LV_ANIM_OFF);
  }
}
static void poll(lv_timer_t *) {
  meshmc::poll();
  auto *s = session();
  if (s) {
    const char *state = s->initialized && !s->busy && !s->fault ? "MeshCore ready" : s->status;
    if (radio_status && strcmp(lv_label_get_text(radio_status), state))
      lv_label_set_text(radio_status, state);
    if (home_name && s->initialized && strcmp(lv_label_get_text(home_name), s->name))
      lv_label_set_text(home_name, s->name);
    const char *summary = s->connected ? (s->initialized ? "MeshCore connected" : "Synchronizing...")
                                       : "Connect in More / Radios";
    if (home_status && strcmp(lv_label_get_text(home_status), summary))
      lv_label_set_text(home_status, summary);
  }
  if (dialog && mode == 3 && feedback && s && s->generation != rendered_generation) {
    if (reveal) {
      char data[360], view[520];
      meshmc::hex(s->last_frame.data, s->last_frame.len, data, sizeof(data));
      snprintf(view, sizeof(view), "Private/local response data. Hide before sharing screenshots.\n%s", data);
      lv_label_set_text(feedback, view);
    } else
      lv_label_set_text(feedback, s->result[0] ? s->result : s->status);
    rendered_generation = s->generation;
  }
  if (!dialog && !wifi_dialog && !ble_dialog && s && s->generation != rendered_generation &&
      (page == 1 || page == 2 || page == 3)) {
    // Do not delete a finger's current target. Redraw only when the input is released.
    bool pressed = false;
    for (lv_indev_t *input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input))
      if (input->proc.state == LV_INDEV_STATE_PRESSED)
        pressed = true;
    if (!pressed)
      draw();
  }
}
} // namespace mcui
