#pragma once
// Included within meshcrossway; every call runs on the LVGL owner.
namespace cfgui {
static lv_obj_t *dialog = nullptr, *list = nullptr, *status = nullptr,
                *editor = nullptr, *input = nullptr, *options = nullptr,
                *keyboard = nullptr, *save = nullptr;
static lv_timer_t *timer = nullptr;
static unsigned group = 0, field = 0;
static uint32_t drawn = UINT32_MAX, edit_revision = 0;
static uint8_t edit_radio[7] = {};
static bool confirmed = false, showing_groups = true;
static lv_obj_t *category_filter=nullptr, *back_button=nullptr;static int selected_category=0;
static void close(lv_event_t *) {
  if (timer) {
    lv_timer_del(timer);
    timer = nullptr;
  }
  if (dialog)
    lv_obj_del(dialog);
  dialog = list = status = editor = input = options = keyboard = save = back_button = nullptr;
}
static void refresh(lv_timer_t *);
static void edit(lv_event_t *e);
static void groups(lv_event_t *);
static void select_category(lv_event_t *e){selected_category=int((intptr_t)lv_event_get_user_data(e));groups(nullptr);}
static void back_categories(lv_event_t *){
  if(editor){
    if(keyboard)lv_keyboard_set_textarea(keyboard,nullptr);
    lv_obj_del(editor);editor=input=options=keyboard=save=nullptr;
    if(back_button)lv_label_set_text(lv_obj_get_child(back_button,0),"BACK");
    return;
  }
  if(!showing_groups){groups(nullptr);return;}
  if(selected_category){selected_category=0;groups(nullptr);}
}
static void read_group(lv_event_t *e) {
  auto *c = meshmt::client;
  if (!c||!c->connected||!c->initialized||!c->history_ready||c->fault){lv_label_set_text(status,"Connect and synchronize this radio first.");return;}
  group = unsigned((intptr_t)lv_event_get_user_data(e));
  if (c->settings.read(group)) {showing_groups=false;drawn=UINT32_MAX;if(back_button){lv_obj_clear_flag(back_button,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(lv_obj_get_child(back_button,0),"BACK");}}
  refresh(nullptr);
}
static void groups(lv_event_t *) {
  showing_groups=true;
  if (editor) {
    lv_obj_del(editor);
    editor = input = options = keyboard = save = nullptr;
  }
  lv_obj_clean(list);
  int y = 0;
  if(back_button){
    if(showing_groups&&!selected_category)lv_obj_add_flag(back_button,LV_OBJ_FLAG_HIDDEN);
    else {lv_obj_clear_flag(back_button,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(lv_obj_get_child(back_button,0),"BACK");}
  }
  if(!selected_category){
    const char *categories[]={"Radio","Device & network","Modules","Channels","Identity","Interface"};
    for(unsigned cat=1;cat<=6;++cat){
      auto *row=action_button(list,categories[cat-1],4,y,448,32,panel2,select_category,cat);
      lv_obj_set_style_radius(row,6,0);
      y+=35;
    }
    drawn=UINT32_MAX;
    return;
  }
  for (unsigned i = 0; i < meshmt::settings::SECTION_COUNT; ++i) {
    const auto &g = meshmt::settings::sections[i];
    const bool channels=g.family==3||g.family==5||g.family==6;
    const unsigned category=channels?4:g.family==1?3:g.family==2?5:g.family==4?6:(i==1||i==2||i==5)?1:2;
    if(selected_category&&selected_category!=category)continue;
    char label[80];
    snprintf(label, sizeof(label), "%s%s", g.family == 1 ? "Module / " : "",
             g.name);
    auto *row=action_button(list, label, 4, y, 448, 31, panel2, read_group, i);
    lv_obj_set_style_radius(row,6,0);
    y += 34;
  }
  drawn = UINT32_MAX;
}
static void apply(lv_event_t *) {
  auto *c = meshmt::client;
  if (!c || !c->connected)
    return;
  if (!confirmed) {
    confirmed = true;
    lv_label_set_text(lv_obj_get_child(save, 0), "CONFIRM");
    lv_label_set_text(
        status, "Apply to this radio? Some settings restart or disconnect it.");
    return;
  }
  if (memcmp(edit_radio, c->data.radio, 7)) {
    lv_label_set_text(status,
                      "Radio changed. Close this editor and read again.");
    return;
  }
  const auto &spec = meshmt::settings::sections[group].fields[field];
  const char *value = input ? lv_textarea_get_text(input) : "";
  char choice[128];
  if (options) {
    lv_dropdown_get_selected_str(options, choice, sizeof(choice));
    char *colon = strchr(choice, ':');
    if (colon)
      *colon = 0;
    value = choice;
  }
  if (c->settings.apply(group, field, value, edit_revision)) {
    lv_obj_del(editor);
    editor = input = options = keyboard = save = nullptr;
    drawn = UINT32_MAX;
    refresh(nullptr);
  } else {
    confirmed = false;
    lv_label_set_text(lv_obj_get_child(save, 0), "SAVE");
    lv_label_set_text(
        status,
        spec.readonly
            ? "This field is read-only."
            : "Value invalid, radio busy or settings changed. Read again.");
  }
}
static void edit(lv_event_t *e) {
  auto *c = meshmt::client;
  if (!c || !c->settings.loaded || c->settings.busy())
    return;
  group = c->settings.section;
  field = unsigned((intptr_t)lv_event_get_user_data(e));
  const auto &spec = meshmt::settings::sections[group].fields[field];
  if (spec.readonly)
    return;
  edit_revision = c->settings.revision;
  memcpy(edit_radio, c->data.radio, 7);
  confirmed = false;
  editor = panel_at(dialog, 4, 42, 472, 238, bg, 0);
  txt(editor, spec.name, 4, 2, 340, white);
  save = action_button(editor, "SAVE", 364, 0, 100, 32, orange, apply, 0);
  if (spec.choices[0] && !spec.repeated) {
    options = lv_dropdown_create(editor);
    lv_obj_set_pos(options, 4, 40);
    lv_obj_set_size(options, 456, 40);
    std::string choices = spec.choices;
    for (char &ch : choices)
      if (ch == '|')
        ch = '\n';
    lv_dropdown_set_options(options, choices.c_str());
    if (meshmt::settings::value(
            {c->settings.current.data, c->settings.current.len}, spec,
            c->settings.scratch, sizeof(c->settings.scratch))) {
      const std::string prefix = std::string(c->settings.scratch) + ":";
      unsigned index = 0;
      size_t pos = 0;
      while (pos < choices.size()) {
        if (choices.compare(pos, prefix.size(), prefix) == 0) {
          lv_dropdown_set_selected(options, index);
          break;
        }
        pos = choices.find('\n', pos);
        if (pos == std::string::npos)
          break;
        ++pos;
        ++index;
      }
    }
    auto *hint = txt(editor,
                     "Choose the value supported by your radio. The result is "
                     "checked by reading it back.",
                     6, 96, 445, muted);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  } else {
    input = lv_textarea_create(editor);
    lv_obj_set_pos(input, 4, 34);
    lv_obj_set_size(input, 456, 43);
    lv_textarea_set_one_line(input, !spec.repeated);
    lv_textarea_set_max_length(input, 1024);
    lv_textarea_set_password_mode(input, spec.secret);
    if (!spec.secret &&
        meshmt::settings::value(
            {c->settings.current.data, c->settings.current.len}, spec,
            c->settings.scratch, sizeof(c->settings.scratch)))
      lv_textarea_set_text(input, c->settings.scratch);
    else
      lv_textarea_set_text(input, "");
    keyboard = lv_keyboard_create(editor);
    lv_obj_set_pos(keyboard, 0, 82);
    lv_obj_set_size(keyboard, 464, 154);
    lv_keyboard_set_textarea(keyboard, input);
  }
}
static void refresh(lv_timer_t *) {
  auto *c = meshmt::client;
  if (!dialog || !c)
    return;
  auto &s = c->settings;
  if (editor||showing_groups)
    return;
  lv_label_set_text(status, s.status);
  if (drawn == s.revision)
    return;
  drawn = s.revision;
  if (!s.loaded)
    return;
  group = s.section;
  lv_obj_clean(list);
  int y = 0;
  const auto &g = meshmt::settings::sections[group];
  for (unsigned i = 0; i < g.count; ++i) {
    const auto &f = g.fields[i];
    bool valid = meshmt::settings::value({s.current.data, s.current.len}, f,
                                         s.scratch, sizeof(s.scratch));
    char label[160];
    snprintf(label, sizeof(label), "%s\n%.76s%s", f.name,
             f.secret ? "Hidden; edit to replace"
             : valid  ? s.scratch
                      : "Unavailable",
             f.readonly ? " (read only)" : "");
    auto *b = action_button(list, label, 4, y, 448, 43, panel2, edit, i);
    if (s.busy() || f.readonly || !valid)
      lv_obj_add_state(b, LV_STATE_DISABLED);
    y += 46;
  }
}
static void open_mt(lv_event_t *) {
  mtui::close();
  close(nullptr);
  dialog = panel_at(lv_scr_act(), 0, 0, 480, 320, bg, 0);
  txt(dialog, "MESHTASTIC / SETTINGS", 8, 8, 255, orange);
  back_button=action_button(dialog, "BACK", 270, 4, 94, 30, panel2, back_categories, 0);
  lv_obj_add_flag(back_button,LV_OBJ_FLAG_HIDDEN);
  action_button(dialog, "CLOSE", 374, 4, 100, 30, panel2, close, 0);
  category_filter=nullptr;selected_category=0;showing_groups=true;
  list = panel_at(dialog, 4, 42, 472, 234, bg, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  status = txt(dialog, "Select a group to read its settings from the radio.", 8,
               280, 464, muted);
  lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
  groups(nullptr);
  timer = lv_timer_create(refresh, 300, nullptr);
}
} // namespace cfgui
