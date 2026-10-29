#pragma once
#include "meshcrossway_chat_presentation.h" // Also included by standalone pairing host fixture.
// Included inside meshcrossway after the shared LVGL helpers. All callbacks stay on LVGL thread.
namespace mcui {
static lv_obj_t *dialog = nullptr, *scroller = nullptr, *feedback = nullptr, *editor = nullptr,
                *edit_value = nullptr, *keyboard = nullptr, *edit_target = nullptr;
static lv_obj_t *fields[10] = {};
static lv_obj_t *radio_status = nullptr, *home_name = nullptr, *home_status = nullptr, *history_status=nullptr;
static char home_name_source[80] = {};
static uint8_t field_count = 0, selected_command = 0;
static bool confirm_pending = false, group = false, conversation = false, reveal = false;
static uint8_t target_key[32] = {};
static uint16_t target_slot = 0, list_offset = 0;
static uint32_t rendered_generation = 0;
static mcxchat::Key selected;
static int mode = 0; // 0 menu, 1 typed command, 2 compose, 3 result, 4 contact details
static void tools();
static void compose();
static void save_draft();
static void chat_tools();
static void show_list();
static void room_menu();
static void select(const mcxchat::Key &);
static void thread_info();
static void form(uint8_t code);
static void render();
static meshmc::Client *session() {
  return meshmc::client;
}
static bool ready() {
  return session() && session()->initialized && session()->history_ready && session()->connected && !session()->fault;
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
  save_draft();
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
// Apply emoji font only to visible name labels. Protocol names and message
// buffers remain untouched and are never written back as private-use glyphs.
static lv_obj_t *emoji_txt(lv_obj_t *parent,const char *value,int x,int y,int width,lv_color_t color){
  lv_obj_t *label=txt(parent,value,x,y,width,color);
#if defined(MCX_EMOJI_RENDER_ENABLED)
  mcxemoji::view::set_message_text(label,value);
#endif
  return label;
}
static void emoji_center_heading(const char *name,const char *subtitle){
  const int width=lv_obj_get_width(body);
  lv_obj_t *label=single_label(body,name,8,8,width-16,white);
  lv_obj_set_height(label,19); // Compact 14px color glyph in normal text row.
#if defined(MCX_EMOJI_RENDER_ENABLED)
  mcxemoji::view::set_message_text(label,name);
#endif
  if(subtitle)single_label(body,subtitle,8,30,width-16,muted);
}
static void set_home_name(lv_obj_t *label,const char *name){
  if(!label||!name)return;
#if defined(MCX_EMOJI_RENDER_ENABLED)
  mcxemoji::view::set_message_text(label,name);
#else
  lv_label_set_text(label,name);
#endif
  snprintf(home_name_source,sizeof(home_name_source),"%s",name);
}
static void base(const char *title) {
  close();
  dialog = panel_at(lv_scr_act(), 0, 0, 480, 320, bg, 0);
  emoji_txt(dialog, title, 9, 12, 350, orange);
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
  save_draft();
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
  return session() && !memcmp(selected.radio,session()->public_key,32) && mcxchat::belongs(m,selected);
}
static void compose_send(lv_event_t *) {
  if (!session() || !fields[0])
    return;
  if (!mcxchat::send(mcxchat::Network::MC,*session(),selected,lv_textarea_get_text(fields[0]))) {
    lv_label_set_text(feedback,
                      "Cannot send: check radio connection/clock, recipient, queue and 150-byte text limit.");
    return;
  }
  lv_textarea_set_text(fields[0],"");save_draft();
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
  auto *pref=mcxhistory::local.get(selected);
  field(limit, pref?pref->draft:"", 0);
  feedback = txt(scroller, "Draft is local. Send uses MeshCore and requires a connected radio with a valid clock.", 10, 88, 450, muted);
  lv_label_set_long_mode(feedback, LV_LABEL_LONG_WRAP);
  action_button(dialog, "SEND", 10, 273, 460, 40, orange, compose_send, 0);
}
static void compose_event(lv_event_t *) {
  compose();
}
static void contact_chat(lv_event_t *) {
  close();
  if(session()){int i=session()->find_contact(target_key);if(i>=0)select(mcxchat::key_for(*session(),false,i));}
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
  lv_obj_t *l = emoji_txt(scroller, text, 9, 0, 455, white);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  action_button(scroller, "CHAT", 8, 170, 215, 42, orange, contact_chat, 0);
  action_button(scroller, "LOGIN", 234, 170, 235, 42, panel2, choose_command, 26);
  action_button(scroller, "STATUS", 8, 220, 215, 42, panel2, choose_command, 27);
  action_button(scroller, "TELEMETRY", 234, 220, 235, 42, panel2, choose_command, 39);
  action_button(scroller, "DISCOVER PATH", 8, 270, 215, 42, panel2, choose_command, 52);
  action_button(scroller, "RESET PATH", 234, 270, 235, 42, panel2, choose_command, 13);
  action_button(scroller, "REMOVE CONTACT", 8, 320, 460, 42, panel2, choose_command, 15);
}
// Chat context actions are deliberately in the right rail, not duplicated
// above message history. Info opens contact details or safe channel metadata.
static void thread_info() {
  if (!conversation || !session()) {
    notice("CHAT / NO SELECTION","Select a node or group to inspect its details.");
    return;
  }
  if(selected.kind==mcxchat::Kind::Room){room_menu();return;}
  if (!group) { contact_details(); return; }
  const auto &channel=session()->channels[target_slot];
  char detail[160];
  snprintf(detail,sizeof(detail),"Channel slot: %u\nName: %.80s\nLoaded: %s\nMeshCore network only.",
           (unsigned)target_slot,channel.name,channel.loaded?"yes":"no");
  notice("MESHCORE / CHANNEL DETAILS",detail);
}
static void choose_contact_now(void *data) {
  int index = (int)(intptr_t)data;
  if (!session() || index < 0 || index >= session()->contact_count)
    return;
  select(mcxchat::key_for(*session(),false,index));
  if(session()->contacts[index].type==3)room_menu();else contact_details();
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
  if(session())select(mcxchat::key_for(*session(),true,target_slot));
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
static mcxchat::Thread thread_rows[meshmc::MAX_MESSAGES];
static size_t thread_count=0;
static char search_text[81]={};
static bool show_blocked=false;
static uint32_t detail_id=0,notification_id=0,notification_cursor=0,notice_until=0;
static lv_obj_t *notification=nullptr;
static uint8_t view_radio[32]={};
static bool groups_rooms=false;
static uint16_t display_indices[meshmc::MAX_CONTACTS];
static uint64_t display_scores[meshmc::MAX_CONTACTS];
static bool view_restored=false;
static void chat_tools();
static void room_menu();
static void select(const mcxchat::Key &key){
  selected=key;group=key.kind==mcxchat::Kind::Channel;
  memcpy(target_key,key.native,32);target_slot=group?key.native[0]:0;conversation=true;
  mcxhistory::local.context=key;mcxhistory::local.has_context=true;++mcxhistory::local.revision;
}
static void show_list(){close();conversation=false;search_text[0]=0;page=3;draw();}
static void list_event(lv_event_t *){show_list();}
static mcxchat::Preference *preference(bool create=false){return mcxhistory::local.get(selected,create);}
static void save_draft(){
  if(mode!=2||!conversation||!fields[0])return;
  const char *text=lv_textarea_get_text(fields[0]);
  if(strlen(text)>meshmc::MAX_TEXT||!meshmc::valid_utf8(text,strlen(text)))return;
  auto *p=preference(true);if(p&&strcmp(p->draft,text)){
    snprintf(p->draft,sizeof(p->draft),"%s",text);++mcxhistory::local.revision;
  }
}
static const char *thread_name(const mcxchat::Key &key){
  auto *p=mcxhistory::local.get(key);if(p&&p->alias[0])return p->alias;
  auto *s=session();if(!s)return "Unknown sender";
  if(key.kind==mcxchat::Kind::Channel)return memcmp(key.native+1,s->channels[key.native[0]].secret,16)?"Archived channel":s->channels[key.native[0]].name;
  const int i=s->find_contact(key.native);return i>=0?s->contacts[i].name:"Unknown sender";
}
static void open_row_now(void *data){
  const size_t i=(size_t)(intptr_t)data;if(i>=thread_count)return;
  select(thread_rows[i].key);page=3;draw();
}
static void open_row(lv_event_t *e){lv_async_call(open_row_now,lv_event_get_user_data(e));}
static void text_apply(lv_event_t *){
  const char *value=fields[0]?lv_textarea_get_text(fields[0]):"";
  if(mode==5){snprintf(search_text,sizeof(search_text),"%.80s",value);}
  else if(mode==6){auto *p=preference(true);if(!p||strlen(value)>32){lv_label_set_text(feedback,"Alias limit: 32 UTF-8 bytes / local preferences full.");return;}
    snprintf(p->alias,sizeof(p->alias),"%s",value);++mcxhistory::local.revision;
  }else if(mode==7){
    if(strlen(value)>meshmc::MAX_TEXT||!meshmc::valid_utf8(value,strlen(value))){lv_label_set_text(feedback,"Limit: 150 valid UTF-8 bytes.");return;}
    snprintf(mcxhistory::local.quick[selected_command],sizeof(mcxhistory::local.quick[0]),"%s",value);++mcxhistory::local.revision;
  }
  close();draw();
}
static void search_event(lv_event_t *){
  base("LOCAL SEARCH / MC");mode=5;field("Name or message text",search_text,0);
  feedback=txt(scroller,"Search covers the retained local history.",10,90,450,muted);
  action_button(dialog,"SEARCH",10,273,460,40,orange,text_apply,0);
}
static void alias_event(lv_event_t *){
  base("LOCAL ALIAS / MC");mode=6;auto *p=preference();field("Local name (32 UTF-8 bytes)",p?p->alias:"",0);
  feedback=txt(scroller,"Does not change the radio identity.",10,90,450,muted);
  action_button(dialog,"SAVE",10,273,460,40,orange,text_apply,0);
}
static void quick_edit(lv_event_t *e){
  selected_command=(uint8_t)(intptr_t)lv_event_get_user_data(e);
  base("QUICK REPLY / EDIT");mode=7;field("Saved text",mcxhistory::local.quick[selected_command],0);
  feedback=txt(scroller,"Local only; saving does not transmit.",10,90,450,muted);
  action_button(dialog,"SAVE",10,273,460,40,orange,text_apply,0);
}
static void quick_use(lv_event_t *e){
  unsigned i=(unsigned)(intptr_t)lv_event_get_user_data(e);if(i>=mcxchat::QUICK_COUNT||!conversation)return;
  compose();if(fields[0]){lv_textarea_set_text(fields[0],mcxhistory::local.quick[i]);save_draft();}
}
static void local_action(lv_event_t *e){
  const int action=(int)(intptr_t)lv_event_get_user_data(e);auto *s=session();
  if(action==0&&s)mcxchat::mark_read(*s);
  else if(action==5)mcxhistory::local.previews=!mcxhistory::local.previews;
  else if(action==6)show_blocked=!show_blocked;
  else if(conversation){auto *p=preference(true);if(!p){notice("LOCAL SETTINGS","Preference limit reached (128 conversations).");return;}
    if(action==1)p->favorite=!p->favorite;
    if(action==2)p->notify=(mcxchat::Notify)((unsigned(p->notify)+1)%3);
    if(action==3)p->blocked=!p->blocked;
    if(action==4&&s){
      if(!confirm_pending){confirm_pending=true;lv_label_set_text(feedback,"Clear saved history? Press CLEAR again. No radio data is deleted.");return;}
      if(!mcxchat::clear(*s,selected)){lv_label_set_text(feedback,"Wait for queued/accepted sends to finish or disconnect first.");return;}
    }
  }
  ++mcxhistory::local.revision;chat_tools();
}
static void chat_tools_event(lv_event_t *){chat_tools();}
static void room_menu_event(lv_event_t *){room_menu();}
static void toggle_groups(lv_event_t *){groups_rooms=!groups_rooms;list_offset=0;draw();}
static void notification_open_now(void *){
  auto *s=session();auto *m=s?s->by_id(notification_id):nullptr;
  if(m){select(mcxchat::key_for_message(*s,*m));page=3;draw();}
}
static void notification_open(lv_event_t *){lv_async_call(notification_open_now,nullptr);}
static void notifications(meshmc::Client &s,uint32_t now){
  if(notification&&(int32_t)(now-notice_until)>=0){lv_obj_del(notification);notification=nullptr;}
  if(!mcxhistory::restored)return;
  if(memcmp(view_radio,s.public_key,32)||!view_restored){
    memcpy(view_radio,s.public_key,32);view_restored=true;
    conversation=false;
    if(mcxhistory::local.has_context)select(mcxhistory::local.context);
    notification_cursor=s.next_id-1;
  }
  for(uint32_t id=notification_cursor+1;id<s.next_id;++id){
    auto *m=s.by_id(id);if(!m)continue;
    const auto key=mcxchat::key_for_message(s,*m);const auto *p=mcxhistory::local.get(key);
    if(!mcxchat::notify(p,*m,s.name)||(conversation&&page==3&&mcxchat::equal(key,selected)&&!dialog))continue;
    if(notification)lv_obj_del(notification);
    notification=panel_at(lv_scr_act(),80,35,320,65,panel2,6);
    lv_obj_add_flag(notification,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(notification,notification_open,LV_EVENT_CLICKED,nullptr);
    char preview[140];snprintf(preview,sizeof(preview),"MC / %.32s\n%.80s",thread_name(key),mcxhistory::local.previews?m->text:"New message");
    auto *l=emoji_txt(notification,preview,8,7,304,white);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);
    notification_id=id;notice_until=now+5000;
  }
  notification_cursor=s.next_id-1;
}
static void chat_tools(){
  base("CHAT / LOCAL TOOLS");mode=8;
  action_button(scroller,"CONVERSATIONS",8,0,220,40,panel2,list_event,0);
  action_button(scroller,"SEARCH",238,0,230,40,panel2,search_event,0);
  action_button(scroller,"MARK ALL READ",8,48,220,40,panel2,local_action,0);
  action_button(scroller,mcxhistory::local.previews?"PREVIEWS: ON":"PREVIEWS: OFF",238,48,230,40,panel2,local_action,5);
  action_button(scroller,show_blocked?"HIDE BLOCKED":"SHOW BLOCKED",8,96,460,40,panel2,local_action,6);
  int y=144;
  if(conversation){
    auto *p=preference();
    action_button(scroller,p&&p->favorite?"UNFAVORITE":"FAVORITE",8,y,220,40,panel2,local_action,1);
    action_button(scroller,"LOCAL ALIAS",238,y,230,40,panel2,alias_event,0);y+=48;
    action_button(scroller,p&&p->notify==mcxchat::Notify::Muted?"NOTIFY: MUTED":p&&p->notify==mcxchat::Notify::Mentions?"NOTIFY: MENTIONS":"NOTIFY: ALL",8,y,220,40,panel2,local_action,2);
    action_button(scroller,p&&p->blocked?"UNBLOCK LOCALLY":"BLOCK LOCALLY",238,y,230,40,panel2,local_action,3);y+=48;
    action_button(scroller,"CLEAR HISTORY",8,y,220,40,panel2,local_action,4);
    if(selected.kind==mcxchat::Kind::Room)action_button(scroller,"ROOM SESSION",238,y,230,40,panel2,room_menu_event,0);
    y+=48;
  }
  for(unsigned i=0;i<mcxchat::QUICK_COUNT;++i){
    char label[45];snprintf(label,sizeof(label),"%.40s",mcxhistory::local.quick[i]);
    action_button(scroller,label,8,y,350,40,panel2,quick_use,i);
    action_button(scroller,"EDIT",368,y,100,40,panel2,quick_edit,i);y+=48;
  }
  feedback=txt(scroller,"Mute/block affect this screen only, not RF reception. History is plaintext on SD.",10,y,450,muted);
  lv_label_set_long_mode(feedback,LV_LABEL_LONG_WRAP);
}
static const char *room_state(){
  auto *s=session();auto *r=s?s->room(target_key):nullptr;
  if(!ready())return "Disconnected / authentication must be checked again";
  if(!r)return "Not authenticated / status unknown";
  switch(r->state){case meshmc::RoomState::Joining:return "Waiting for room login response";
    case meshmc::RoomState::Joined:return "Authenticated with room";
    case meshmc::RoomState::Failed:return "Room rejected login / not connected";
    case meshmc::RoomState::Left:return "Room session left";
    case meshmc::RoomState::TimedOut:return "Room login timed out / no automatic retry";
    default:return "Room authentication unknown";}
}
static void room_menu(){
  base("MESHCORE / ROOM SESSION");mode=9;
  feedback=txt(scroller,room_state(),10,5,450,orange);
  txt(scroller,"Room sessions use native MeshCore login. Passwords are not saved.",10,60,450,muted);
  action_button(scroller,"LOGIN",8,120,220,40,panel2,choose_command,26);
  action_button(scroller,"CHECK SESSION",238,120,230,40,panel2,choose_command,28);
  action_button(scroller,"LEAVE ROOM",8,170,220,40,panel2,choose_command,29);
  action_button(scroller,"OPEN ROOM CHAT",238,170,230,40,orange,contact_chat,0);
}
static void retry_send(lv_event_t *){
  auto *s=session();auto *m=s?s->by_id(detail_id):nullptr;
  if(!m||!mcxchat::belongs(*m,selected)||!m->outgoing||
     (m->delivery!=meshmc::REJECTED&&m->delivery!=meshmc::UNCERTAIN&&m->delivery!=meshmc::ACCEPTED))return;
  if(!confirm_pending){confirm_pending=true;lv_label_set_text(feedback,"This message may already have arrived. Press RESEND again to confirm possible duplication.");return;}
  char text[meshmc::MAX_FRAME+1];snprintf(text,sizeof(text),"%s",m->text);
  if(!mcxchat::send(mcxchat::Network::MC,*s,selected,text)){lv_label_set_text(feedback,"Not sent. Check connection, recipient, room login and queue.");return;}
  close();draw();
}
static void message_details_now(void *data){
  auto *s=session();detail_id=(uint32_t)(intptr_t)data;auto *m=s?s->by_id(detail_id):nullptr;
  if(!m||!mcxchat::belongs(*m,selected))return;
  base("MESSAGE / MC");mode=10;
  char detail[450];
  snprintf(detail,sizeof(detail),"Network: MeshCore\nSender: %.32s\nSender time: %lu Unix\nObserved time: %lu Unix\nState: %s\n%s",
    m->outgoing?"You":thread_name(selected),(unsigned long)m->timestamp,(unsigned long)m->observed_at,
    delivery(m->delivery),"Accepting a message is not delivery or reading.");
  auto *details=txt(scroller,detail,10,0,450,white);lv_label_set_long_mode(details,LV_LABEL_LONG_WRAP);
  char signal[90],route[96];mcxchat::meta(m->observed_at,m->path,m->outgoing,nullptr,route,sizeof(route));
  if(m->has_snr)snprintf(signal,sizeof(signal),"SNR: %.2f dB / %.65s",m->snr/4.0,route);
  else snprintf(signal,sizeof(signal),"SNR not provided / %.65s",route);
  txt(scroller,signal,10,130,450,muted);
  char link[meshmc::MAX_FRAME+1];
  if(mcxchat::url(m->text,link,sizeof(link))){
    txt(scroller,"Open this URL on another device:",10,150,450,orange);
    auto *label=txt(scroller,link,10,177,450,white);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
  }
  feedback=txt(dialog,"",10,233,450,orange);lv_label_set_long_mode(feedback,LV_LABEL_LONG_WRAP);
  if(m->outgoing&&(m->delivery==meshmc::REJECTED||m->delivery==meshmc::UNCERTAIN||m->delivery==meshmc::ACCEPTED))
    action_button(dialog,"RESEND",10,278,460,36,panel2,retry_send,0);
}
static void message_details(lv_event_t *e){lv_async_call(message_details_now,lv_event_get_user_data(e));}
static void conversation_list(int w){
  auto *s=session();show_center_heading("CHATS / CONVERSATIONS",nullptr);
  history_status=single_label(body,mcxhistory::status(),8,30,w-16,muted);filter_tabs(w,chats_filter);
  action_button(body,"SEARCH",8,98,(w-22)/2,34,panel2,search_event,0);
  action_button(body,"TOOLS",(w+2)/2,98,(w-22)/2,34,panel2,chat_tools_event,0);
  if(chats_filter==1){single_label(body,"Meshtastic: not available",8,145,w-16,muted);return;}
  if(!s)return;
  thread_count=mcxchat::list(*s,mcxhistory::local,thread_rows,meshmc::MAX_MESSAGES,search_text,false,show_blocked);
  int y=141;
  for(size_t i=0;i<thread_count;++i){auto &t=thread_rows[i];auto *m=s->by_id(t.last);
    auto *p=mcxhistory::local.get(t.key);char heading[105],sub[190],meta[96];
    snprintf(heading,sizeof(heading),"MC / %s%s / %u unread",p&&p->favorite?"* ":"",thread_name(t.key),t.unread);
    if(m){mcxchat::meta(m->observed_at,m->path,m->outgoing,m->outgoing?delivery(m->delivery):nullptr,meta,sizeof(meta));
      snprintf(sub,sizeof(sub),"%.80s\n%s / %s",m->text,meta,ready()?"connected":"offline");}
    else snprintf(sub,sizeof(sub),"DRAFT / %.100s",p?p->draft:"");
    auto *b=action_button(body,"",8,y,w-16,90,panel,open_row,i);
    emoji_txt(b,heading,7,5,w-30,white);auto *label=emoji_txt(b,sub,7,32,w-30,muted);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);y+=97;
  }
  if(!thread_count)single_label(body,search_text[0]?"No local search results.":"Choose a Node or Group to start.",8,y,w-16,muted);
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
    if (!s || (!s->initialized && !s->contact_count && !s->channel_count)) {
      lv_obj_t *status = single_label(body,
          s ? s->status : "More > Radios > Enable BLE", 8, 107, w - 16, orange);
      // Diagnostic code/length must remain readable, not end in "...".
      lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP);
      lv_obj_set_height(status,52);
      return;
    }
    int total=0;
    const bool room_list=channels&&groups_rooms;
    for(int i=0;i<(channels&&!room_list?s->channel_count:s->contact_count);++i){
      if(room_list&&s->contacts[i].type!=3)continue;
      display_indices[total++]=i;
    }
    // Most recently active first; local favorites precede other nodes/channels.
    auto rank=[&](int i)->uint64_t {
      auto key=mcxchat::key_for(*s,channels&&!room_list,i);auto *p=mcxhistory::local.get(key);
      uint32_t last=0;for(const auto &m:s->messages)if(mcxchat::belongs(m,key)&&m.id>last)last=m.id;
      return (uint64_t(p&&p->favorite)<<32)|last;
    };
    for(int a=0;a<total;++a)display_scores[display_indices[a]]=rank(display_indices[a]);
    for(int a=1;a<total;++a){uint16_t value=display_indices[a];int b=a;const uint64_t score=display_scores[value];
      while(b&&display_scores[display_indices[b-1]]<score){display_indices[b]=display_indices[b-1];--b;}display_indices[b]=value;
    }
    if (list_offset >= total)
      list_offset = 0;
    action_button(body, "<", 8, 99, 45, 34, panel2, paginate, -1);
    action_button(body, ">", 59, 99, 45, 34, panel2, paginate, 1);
    if(channels)action_button(body,groups_rooms?"ROOMS / CHANNELS":"CHANNELS / ROOMS",112,99,w-120,34,panel2,toggle_groups,0);
    else action_button(body, "TOOLS", 112, 99, w - 120, 34, panel2, on_tools, 0);
    int y = 141;
    if (!total)
      single_label(body, "No entries reported by the radio.", 8, y, w - 16, muted);
    for (int row = list_offset; row < total && row < list_offset + 10; row++) {
      const int i=display_indices[row];const bool is_channel=channels&&!room_list;
      const auto key=mcxchat::key_for(*s,is_channel,i);const auto *pref=mcxhistory::local.get(key);
      char title[75], detail[100];
      bool loaded = is_channel && s->channels[i].loaded;
      if (is_channel) {
        snprintf(title, sizeof(title), "%u / %s", i,
                 loaded && s->channels[i].name[0] ? s->channels[i].name : "Empty slot");
        snprintf(detail, sizeof(detail), "%d unread / tap to %s", unread(true, i),
                 loaded && s->channels[i].name[0] ? "chat" : "configure");
      } else {
        const auto &c = s->contacts[i];
        snprintf(title, sizeof(title), "%s%s",pref&&pref->favorite?"* ":"",pref&&pref->alias[0]?pref->alias:c.name);
        snprintf(detail, sizeof(detail), "%s / %d unread",
                 c.type == 2   ? "Repeater"
                 : c.type == 3 ? "Room"
                 : c.type == 4 ? "Sensor"
                               : "Companion",
                 unread(false, i));
      }
      uint32_t last=0;for(const auto &m:s->messages)if(mcxchat::belongs(m,key)&&m.id>last)last=m.id;
      if(last){auto *m=s->by_id(last);char recent[100];snprintf(recent,sizeof(recent),"%d unread / %.65s",mcxchat::unread(*s,key),m->text);snprintf(detail,sizeof(detail),"%s",recent);}
      lv_obj_t *b =
          action_button(body, "", 8, y, w - 16, 60, panel, is_channel ? choose_channel : choose_contact, i);
      emoji_txt(b, title, 7, 6, w - 30, white);
      txt(b, detail, 7, 33, w - 30, muted);
      y += 67;
    }
  } else if (page == 3) {
    if (!conversation || !s) {conversation_list(w);return;}
    int ci = group ? -1 : s->find_contact(target_key);
    const char *title = group     ? s->channels[target_slot].name
                        : ci >= 0 ? s->contacts[ci].name
                                  : "Unknown contact";
    title=thread_name(selected);
    emoji_center_heading(title[0] ? title : "Empty channel",
                        ready() ? "MeshCore / live" : "MeshCore / disconnected history");
    // Compose and Tools now live exclusively in the contextual right rail.
    action_button(body,"< CHATS",8,54,100,32,panel2,list_event,0);
    history_status=single_label(body,mcxhistory::status(),114,61,w-122,muted);
    int y = 96;
    bool newly_read = false;
    for (uint32_t id = s->next_id > s->message_count ? s->next_id - s->message_count : 1; id < s->next_id;
         id++) {
      auto *m = s->by_id(id);
      if (!m || !in_conversation(*m) || !mcxchat::contains(m->text,search_text))
        continue;
      if (!m->read && !m->outgoing) { m->read = true; newly_read = true; }
      // Chat cards contain sender, clean message body and genuine metadata.
      // The technical message id and RX/RECEIVED protocol state stay internal.
      char sender[65] = {};
      const char *display_text = m->text; // Never write back to radio buffers.
      if (m->outgoing) {
        snprintf(sender, sizeof(sender), "You");
      } else if (m->channel) {
        // MeshCore channel text commonly uses "Name: message". This is a
        // sender claim in plaintext, not a verified cryptographic identity.
        const mcxchat::Sender parsed = mcxchat::channel_sender(m->text);
        if (parsed.claimed) {
          snprintf(sender, sizeof(sender), "%s", parsed.name);
          display_text = parsed.body;
        } else {
          snprintf(sender, sizeof(sender), "Unknown sender");
        }
      } else {
        const int origin = s->find_contact(m->key, 6);
        snprintf(sender, sizeof(sender), "%s",
                 origin >= 0 && s->contacts[origin].name[0]
                   ? s->contacts[origin].name : "Unknown sender");
      }
      const lv_color_t sender_colors[6] = {
        lv_color_hex(0x72D8A5), lv_color_hex(0x6CBDE6),
        lv_color_hex(0xB99AFF), lv_color_hex(0xFFA36A),
        lv_color_hex(0xE39CD4), lv_color_hex(0x83D5CF)
      };
      const lv_color_t sender_color = m->outgoing ? orange :
                   sender_colors[mcxchat::color_index(sender)];
      char metadata[96];
      mcxchat::meta(m->observed_at, m->path, m->outgoing,
                    m->outgoing ? delivery(m->delivery) : nullptr,
                    metadata, sizeof(metadata));
      lv_obj_t *card = panel_at(body, 8, y, w - 16, 94, panel, 7);
      lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(card,message_details,LV_EVENT_CLICKED,(void *)(intptr_t)m->id);
      lv_obj_t *name = emoji_txt(card, sender, 8, 6, w - 32, sender_color);
      lv_obj_set_height(name, 18);
      lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
      lv_obj_t *text = txt(card, "", 8, 34, w - 32, mcxchat::mention(m->text,s->name)?orange:white);
      lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
      // Transform ONLY the visible text. The radio's UTF-8 bytes are untouched.
#if defined(MCX_EMOJI_RENDER_ENABLED)
      mcxemoji::view::set_message_text(text, display_text);
#else
      lv_label_set_text(text, display_text);
#endif
      lv_obj_update_layout(text);
      const int metadata_y = lv_obj_get_y(text) + lv_obj_get_height(text) + 9;
      lv_obj_t *footer = single_label(card, metadata, 8, metadata_y, w - 32, muted);
      lv_obj_set_height(footer, 18);
      const int card_height = metadata_y + 23 > 94 ? metadata_y + 23 : 94;
      lv_obj_set_height(card, card_height);
      y += card_height + 8;
    }
    if (newly_read) ++s->generation; // Persist unread state after a thread is opened.
    if (y == 96)
      single_label(body, "No messages in saved history.", 8, 65, w - 16, muted);
    lv_obj_scroll_to_y(body, y, LV_ANIM_OFF);
  }
}
static void poll(lv_timer_t *) {
  meshmc::poll();
  auto *s = session();
  if (s) {
    const uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    mcxhistory::poll(*s,now,meshmc::connection_epoch.load());
    if(!s->initialized&&s->connected){view_restored=false;conversation=false;}
    notifications(*s,now);
    if(history_status&&strcmp(lv_label_get_text(history_status),mcxhistory::status()))lv_label_set_text(history_status,mcxhistory::status());
  }
  if (s) {
    const char *state = s->initialized && !s->busy && !s->fault ? "MeshCore ready" : s->status;
    if (radio_status && strcmp(lv_label_get_text(radio_status), state))
      lv_label_set_text(radio_status, state);
    if (home_name && s->initialized && strcmp(home_name_source, s->name))
      set_home_name(home_name,s->name);
    const char *summary = s->connected ? (s->initialized ? "MeshCore connected" : "Synchronizing...")
                                       : "Connect in More / Radios";
    if (home_status && strcmp(lv_label_get_text(home_status), summary))
      lv_label_set_text(home_status, summary);
  }
  if(dialog&&mode==9&&feedback)lv_label_set_text(feedback,room_state());
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
