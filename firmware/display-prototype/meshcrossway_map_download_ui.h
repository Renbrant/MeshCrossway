#pragma once
// Included inside gpsui, after the common navigation page helpers.
static lv_obj_t *download_owner = nullptr, *download_preview = nullptr,
                *download_image = nullptr, *download_hint = nullptr,
                *download_zoom = nullptr, *download_start = nullptr,
                *download_delete = nullptr, *download_info = nullptr,
                *download_notice = nullptr, *download_cards[4] = {};
static mcxmap::Frame *download_frame = nullptr;
static mcxoffline_map::Area download_area;
static unsigned download_selected = 4;
static uint32_t download_pending = 0;
static bool download_drag = false, download_creating = false;
static uint32_t download_command_at = 0;
static bool download_magnified = false;
static lv_point_t download_first = {}, download_last = {};
static unsigned download_scale = 256;
static int download_image_y = 0;
struct DownloadRect {
  lv_area_t box = {};
  bool visible = false, selected = false;
};
static DownloadRect download_rectangles[5];
static void download_reset() {
  if (download_frame) {
    lv_img_cache_invalidate_src(&download_frame->image);
    mcxmap::release(download_frame);
    download_frame = nullptr;
  }
  download_owner = download_preview = download_image = download_hint =
      download_zoom = download_start = download_delete = download_info =
          download_notice = nullptr;
  for (auto &p : download_cards)
    p = nullptr;
  download_drag = false;
  download_creating = false;
  download_pending = 0;
  download_selected = 4;
}
static mcxmap::Point download_geo(int x, int y) {
  const auto &v = download_frame->view;
  const auto center = mcxmap::project(v.latitude, v.longitude, v.zoom);
  const double inverse = download_magnified ? 0.8 : 1.0;
  return mcxmap::unproject(
      center.x + (x * 256.0 / download_scale - v.width / 2.0) * inverse,
      center.y +
          ((y - download_image_y) * 256.0 / download_scale - v.height / 2.0) *
              inverse,
      v.zoom);
}
static void download_from_rect(int x1, int y1, int x2, int y2) {
  const auto nw = download_geo(x1, y1), se = download_geo(x2, y2);
  const auto cfg = mcxmap::configuration();
  download_area = mcxoffline_map::Area{};
  download_area.source = cfg;
  download_area.source.pack[0] = 0;
  download_area.source.online = 1;
  download_area.north = nw.x;
  download_area.west = nw.y;
  download_area.south = se.x;
  download_area.east = se.y;
  const double world = 256.0 * (1u << download_frame->view.zoom);
  if ((x2 - x1) * 256.0 / download_scale *
          (mcxmap::map_text_magnified(cfg) ? 0.8 : 1) >=
      world) {
    download_area.west = -180;
    download_area.east = 180;
  }
  const unsigned level = download_zoom
                             ? unsigned(lv_slider_get_value(download_zoom))
                             : download_frame->view.zoom;
  download_area.max_zoom = uint8_t(level);
  download_area.min_zoom = 0;
  snprintf(download_area.name, sizeof(download_area.name), "Area %.3f %.3f",
           (nw.x + se.x) / 2, nw.y);
}
static lv_point_t download_pixel(double latitude, double longitude) {
  const auto &v = download_frame->view;
  const auto center = mcxmap::project(v.latitude, v.longitude, v.zoom),
             p = mcxmap::project(latitude, longitude, v.zoom);
  const double magnify = download_magnified ? 1.25 : 1;
  const double scale = download_scale / 256.0;
  const double x =
      (v.width / 2.0 + mcxmap::delta(p.x, center.x, v.zoom) * magnify) * scale;
  const double y =
      download_image_y + (v.height / 2.0 + (p.y - center.y) * magnify) * scale;
  return {lv_coord_t(lround(fmax(-16000, fmin(16000, x)))),
          lv_coord_t(lround(fmax(-16000, fmin(16000, y))))};
}
static void download_outline(DownloadRect &rect, const mcxoffline_map::Area &a,
                             bool selected) {
  const auto nw = download_pixel(a.north, a.west),
             se = download_pixel(a.south, a.east);
  rect.box = {nw.x, nw.y, se.x, se.y};
  rect.visible = rect.box.x1 <= rect.box.x2 && rect.box.y1 <= rect.box.y2;
  rect.selected = selected;
}
static bool download_copy_frame() {
  if (!download_preview || !download_image || !navui::front)
    return false;
  const auto *front = navui::front;
  auto *next = (mcxmap::Frame *)heap_caps_malloc(
      sizeof(mcxmap::Frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!next)
    return false;
  new (next) mcxmap::Frame();
  auto *pixels = (uint8_t *)heap_caps_malloc(
      front->image.data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pixels) {
    mcxmap::release(next);
    return false;
  }
  memcpy(pixels, front->image.data, front->image.data_size);
  next->image = front->image;
  next->image.data = pixels;
  next->view = front->view;
  next->source = front->source;
  download_magnified = mcxmap::map_text_magnified(mcxmap::configuration());
  const unsigned width = unsigned(lv_obj_get_width(download_preview));
  download_scale = width * 256 / front->view.width;
  if (!download_scale)
    download_scale = 1;
  download_image_y = (lv_obj_get_height(download_preview) -
                      int(front->view.height * download_scale / 256)) /
                     2;
  lv_img_set_pivot(download_image, 0, 0);
  lv_img_set_antialias(download_image, false);
  lv_img_set_zoom(download_image, uint16_t(download_scale));
  lv_obj_set_pos(download_image, 0, download_image_y);
  lv_img_set_src(download_image, &next->image);
  if (download_frame) {
    lv_img_cache_invalidate_src(&download_frame->image);
    mcxmap::release(download_frame);
  }
  download_frame = next;
  download_pending = 0;
  return true;
}
static void download_refresh() {
  if (!download_owner)
    return;
  if (download_owner != info_dialog) {
    download_reset();
    return;
  }
  if (download_pending && navui::front &&
      navui::front->view.generation == download_pending)
    if (download_copy_frame() && download_selected == 4 &&
        download_area.north <= download_area.south)
      download_from_rect(0, 0, lv_obj_get_width(download_preview) - 1,
                         lv_obj_get_height(download_preview) - 1);
  const auto &s = mcxoffline_map::snapshot();
  char text[240];
  if (download_creating && download_selected < 4 &&
      (s.areas[download_selected].mode != mcxoffline_map::EMPTY ||
       uint32_t(mcxmap::milliseconds() - download_command_at) > 3000))
    download_creating = false;
  if (!download_creating && download_selected < 4 &&
      s.areas[download_selected].mode == mcxoffline_map::EMPTY) {
    download_selected = 4;
    if (download_frame)
      download_from_rect(0, 0, lv_obj_get_width(download_preview) - 1,
                         lv_obj_get_height(download_preview) - 1);
  }
  bool active = false;
  unsigned empty = 4;
  for (unsigned i = 0; i < 4; ++i) {
    const auto &a = s.areas[i];
    if (a.mode == mcxoffline_map::RUNNING || a.mode == mcxoffline_map::DELETING)
      active = true;
    if (a.mode == mcxoffline_map::EMPTY && empty == 4)
      empty = i;
    if (download_cards[i]) {
      navui::set_hidden(download_cards[i], a.mode == mcxoffline_map::EMPTY);
      if (a.mode != mcxoffline_map::EMPTY) {
        snprintf(text, sizeof(text), "%s / %.1f MiB\n%s / %lu of %lu / z%u-%u",
                 a.name, a.bytes / 1048576.0, area_mode(a.mode),
                 (unsigned long)a.cursor, (unsigned long)a.total, a.min_zoom,
                 a.max_zoom);
        chatview::label(lv_obj_get_child(download_cards[i], 0), text);
      }
    }
    download_rectangles[i].visible = false;
    if (download_frame && a.mode != mcxoffline_map::EMPTY)
      download_outline(download_rectangles[i], a, download_selected == i);
  }
  const auto tiles = mcxoffline_map::count(download_area);
  const bool existing = download_selected < 4;
  const auto mode = existing ? s.areas[download_selected].mode
                             : uint8_t(mcxoffline_map::EMPTY);
  if (download_hint) {
    snprintf(text, sizeof(text),
             existing ? "Saved area / zoom %u"
                      : "Drag a rectangle / detail zoom %u",
             download_area.max_zoom);
    chatview::label(download_hint, text);
  }
  if (download_info) {
    snprintf(text, sizeof(text),
             "%lu tiles / about %.1f MiB\nFree SD %.1f MiB%s",
             (unsigned long)tiles, tiles / 32.0, s.free_bytes / 1048576.0,
             tiles ? "" : " / select a smaller area or lower zoom");
    chatview::label(download_info, text);
  }
  if (download_notice)
    chatview::label(download_notice, download_creating ? "Starting download..."
                                     : download_pending
                                         ? "Loading area preview..."
                                         : s.status);
  const bool can_start =
      download_frame && !download_pending && !existing && s.ready && !active &&
      empty < 4 && tiles && mcxmap::bulk_allowed(download_area.source) &&
      uint64_t(tiles) * mcxmap::MAX_PNG + mcxoffline_map::RESERVE <=
          s.free_bytes;
  if (download_start) {
    const bool can_resume =
        existing && !download_creating && !active &&
        (mode == mcxoffline_map::PAUSED || mode == mcxoffline_map::ERROR);
    const bool can_pause = existing && mode == mcxoffline_map::RUNNING;
    chatview::label(lv_obj_get_child(download_start, 0),
                    mode == mcxoffline_map::COMPLETE ? "COMPLETE"
                    : can_pause                      ? "PAUSE"
                    : existing                       ? "RESUME"
                                                     : "DOWNLOAD");
    if (can_start || can_resume || can_pause)
      lv_obj_clear_state(download_start, LV_STATE_DISABLED);
    else
      lv_obj_add_state(download_start, LV_STATE_DISABLED);
  }
  if (download_delete) {
    if (existing && !download_creating &&
        s.areas[download_selected].mode != mcxoffline_map::DELETING)
      lv_obj_clear_state(download_delete, LV_STATE_DISABLED);
    else
      lv_obj_add_state(download_delete, LV_STATE_DISABLED);
  }
  if (download_zoom) {
    if (existing)
      lv_obj_add_state(download_zoom, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(download_zoom, LV_STATE_DISABLED);
  }
  download_rectangles[4].visible = false;
  if (download_frame && !existing && !download_pending)
    download_outline(download_rectangles[4], download_area, true);
  static DownloadRect previous[5] = {};
  if (download_preview &&
      memcmp(previous, download_rectangles, sizeof(previous))) {
    memcpy(previous, download_rectangles, sizeof(previous));
    lv_obj_invalidate(download_preview);
  }
}
static void download_show_area(unsigned slot) {
  const auto &s = mcxoffline_map::snapshot();
  if (slot >= 4 || s.areas[slot].mode == mcxoffline_map::EMPTY)
    return;
  const auto a = s.areas[slot];
  download_selected = slot;
  download_area = a;
  if (download_zoom)
    lv_slider_set_value(download_zoom, a.max_zoom, LV_ANIM_OFF);
  auto cfg = mcxmap::configuration();
  if (mcxmap::source_hash(cfg) != mcxmap::source_hash(a.source)) {
    const bool online = cfg.online;
    cfg = a.source;
    cfg.online = online;
    mcxmap::configure(cfg);
  }
  mcxmap::Point corners[] = {{a.north, a.west}, {a.south, a.east}};
  mcxmap::Point center;
  unsigned zoom = a.min_zoom;
  const unsigned width =
      navui::canvas ? unsigned(lv_obj_get_width(navui::canvas)) : 400;
  const unsigned height =
      download_scale
          ? unsigned(lv_obj_get_height(download_preview)) * 256 / download_scale
          : 160;
  if (mcxmap::fit_points(
          corners, 2, width, height, mcxmap::max_zoom(mcxmap::configuration()),
          mcxmap::map_text_magnified(mcxmap::configuration()), center, zoom)) {
    navui::latitude = center.x;
    navui::longitude = center.y;
    navui::zoom = zoom;
    navui::centered = true;
    navui::following = false;
    navui::request();
    download_pending = navui::generation;
  }
  lv_obj_scroll_to_y(dialog, 0, LV_ANIM_OFF);
  download_refresh();
}
static void download_pointer(lv_event_t *e) {
  if (!download_frame || download_pending || download_creating)
    return;
  auto *input = lv_indev_get_act();
  if (!input)
    return;
  lv_point_t point;
  lv_indev_get_point(input, &point);
  lv_area_t bounds;
  lv_obj_get_coords(download_preview, &bounds);
  point.x = lv_coord_t(fmax(
      0, fmin(lv_obj_get_width(download_preview) - 1, point.x - bounds.x1)));
  point.y = lv_coord_t(fmax(
      0, fmin(lv_obj_get_height(download_preview) - 1, point.y - bounds.y1)));
  const auto code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    download_first = download_last = point;
    download_drag = true;
  } else if (code == LV_EVENT_PRESSING && download_drag) {
    download_last = point;
    lv_obj_invalidate(download_preview);
  } else if (code == LV_EVENT_PRESS_LOST) {
    download_drag = false;
    lv_obj_invalidate(download_preview);
  } else if (code == LV_EVENT_RELEASED && download_drag) {
    download_drag = false;
    download_last = point;
    if (abs(download_first.x - point.x) >= 8 &&
        abs(download_first.y - point.y) >= 8) {
      download_selected = 4;
      download_from_rect(std::min(download_first.x, point.x),
                         std::min(download_first.y, point.y),
                         std::max(download_first.x, point.x),
                         std::max(download_first.y, point.y));
      download_refresh();
    }
  }
}
static void downloads() {
  lv_obj_update_layout(body);
  base("MAP DOWNLOADS");
  mcxmap::start();
  download_owner = info_dialog;
  navui::following = false;
  auto cfg = mcxmap::configuration();
  if (!mcxmap::bulk_allowed(cfg)) {
    mcxmap::prepare_offline(cfg);
    mcxmap::configure(cfg);
    navui::request();
    download_pending = navui::generation;
  }
  download_preview =
      panel_at(dialog, 0, 0, lv_obj_get_width(body) - 16, 128, panel2, 0);
  lv_obj_add_flag(download_preview,
                  LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(download_preview, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
  lv_obj_add_event_cb(download_preview, download_pointer, LV_EVENT_ALL,
                      nullptr);
  download_image = lv_img_create(download_preview);
  lv_obj_clear_flag(download_image, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      download_preview,
      [](lv_event_t *e) {
        auto *ctx = lv_event_get_draw_ctx(e);
        lv_area_t origin;
        lv_obj_get_coords(download_preview, &origin);
        lv_draw_rect_dsc_t line;
        lv_draw_rect_dsc_init(&line);
        line.bg_opa = LV_OPA_TRANSP;
        line.border_width = 2;
        for (const auto &r : download_rectangles)
          if (r.visible) {
            auto box = r.box;
            box.x1 += origin.x1;
            box.x2 += origin.x1;
            box.y1 += origin.y1;
            box.y2 += origin.y1;
            line.border_color = r.selected ? orange : green;
            lv_draw_rect(ctx, &line, &box);
          }
        if (download_drag) {
          lv_area_t box = {lv_coord_t(origin.x1 + std::min(download_first.x,
                                                           download_last.x)),
                           lv_coord_t(origin.y1 + std::min(download_first.y,
                                                           download_last.y)),
                           lv_coord_t(origin.x1 + std::max(download_first.x,
                                                           download_last.x)),
                           lv_coord_t(origin.y1 + std::max(download_first.y,
                                                           download_last.y))};
          line.border_color = orange;
          lv_draw_rect(ctx, &line, &box);
        }
      },
      LV_EVENT_DRAW_POST, nullptr);
  auto *credit = single_label(download_preview, cfg.attribution, 2, 110,
                              lv_obj_get_width(body) - 24, white);
  lv_obj_set_style_bg_color(credit, bg, 0);
  lv_obj_set_style_bg_opa(credit, LV_OPA_80, 0);
  lv_obj_set_style_text_font(credit, &lv_font_montserrat_10, 0);
  download_hint = single_label(dialog, "", 0, 0, 440, white);
  download_zoom = lv_slider_create(dialog);
  lv_obj_set_height(download_zoom, 14);
  lv_obj_set_style_bg_color(download_zoom, orange, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(download_zoom, orange, LV_PART_KNOB);
  lv_slider_set_range(download_zoom, 0, 19);
  lv_slider_set_value(
      download_zoom, int(navui::zoom < 19 ? navui::zoom + 1 : 19), LV_ANIM_OFF);
  lv_obj_add_event_cb(
      download_zoom,
      [](lv_event_t *) {
        if (download_selected < 4)
          return;
        const auto z = unsigned(lv_slider_get_value(download_zoom));
        download_area.max_zoom = uint8_t(z);
        download_area.min_zoom = 0;
        download_refresh();
      },
      LV_EVENT_VALUE_CHANGED, nullptr);
  auto *row = control_row();
  download_start = control_button(
      row, "DOWNLOAD", orange,
      [](lv_event_t *) {
        const auto &s = mcxoffline_map::snapshot();
        mcxoffline_map::Command c;
        if (download_selected < 4) {
          c.slot = download_selected;
          c.action = s.areas[c.slot].mode == mcxoffline_map::RUNNING
                         ? mcxoffline_map::PAUSE
                         : mcxoffline_map::RESUME;
          mcxoffline_map::submit(c);
          return;
        }
        c.action = mcxoffline_map::CREATE;
        c.slot = 4;
        for (unsigned i = 0; i < 4; ++i)
          if (s.areas[i].mode == mcxoffline_map::EMPTY) {
            c.slot = i;
            break;
          }
        c.area = download_area;
        if (mcxoffline_map::submit(c)) {
          download_selected = c.slot;
          download_creating = true;
          download_command_at = mcxmap::milliseconds();
          chatview::label(download_notice, "Starting download...");
        }
      },
      0);
  download_delete = control_button(
      row, "DELETE", red,
      [](lv_event_t *) {
        if (download_selected >= 4)
          return;
        mcxoffline_map::Command c;
        c.action = mcxoffline_map::REMOVE;
        c.slot = download_selected;
        if (mcxoffline_map::submit(c))
          chatview::label(download_notice, "Deleting area...");
      },
      0);
  download_info = single_label(dialog, "", 0, 0, 440, muted);
  download_notice = single_label(dialog, "", 0, 0, 440, cyan);
  single_label(dialog, "SAVED AREAS / TAP TO SHOW ON MAP", 0, 0, 440, orange);
  for (unsigned i = 0; i < 4; ++i) {
    download_cards[i] = action_button(
        dialog, "", 0, 0, 440, 52, panel2,
        [](lv_event_t *e) {
          download_show_area(unsigned(uintptr_t(lv_event_get_user_data(e))));
        },
        i);
    auto *label = lv_obj_get_child(download_cards[i], 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_height(label, 46);
  }
  single_label(dialog,
               "Select an area above, then Delete to free its unshared tiles. "
               "Drag on the map to select a new area. Downloads need Wi-Fi; "
               "saved maps work offline.",
               0, 0, 440, muted);
  lv_obj_update_layout(dialog);
  const auto pending = download_pending;
  if (download_copy_frame())
    download_from_rect(0, 0, lv_obj_get_width(download_preview) - 1,
                       lv_obj_get_height(download_preview) - 1);
  download_pending = pending;
  download_refresh();
}
